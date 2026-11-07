/* Bounded structural JSON parser and OpenAPI 3 operation extractor, pure C11.
 * Does not resolve remote refs or call declared API endpoints. */
#include "internal.h"
#include <ctype.h>
#include <stdlib.h>
typedef struct { const char *s;size_t len,pos;int invalid; } json;
static void ws(json *j){while(j->pos<j->len&&(j->s[j->pos]==' '||j->s[j->pos]=='\n'||j->s[j->pos]=='\r'||j->s[j->pos]=='\t'))j->pos++;}
static int quoted(json *j,size_t *begin,size_t *end){
 ws(j);if(j->pos==j->len||j->s[j->pos]!='"')return -1;
 ++j->pos;*begin=j->pos;
 for(;j->pos<j->len;j->pos++){
  unsigned char c=(unsigned char)j->s[j->pos];
  if(c=='"'){*end=j->pos++;return 0;}
  if(c<32)return -1;
  if(c=='\\'){
   if(++j->pos==j->len)return -1;
   char e=j->s[j->pos];
   if(!strchr("\"\\/bfnrtu",e))return -1;
   if(e=='u'){for(int x=0;x<4;x++){if(++j->pos==j->len||!isxdigit((unsigned char)j->s[j->pos]))return -1;}}
  }
 }
 return -1;
}
static int skip(json *j,unsigned depth){
 if(depth>64)return -1;
 ws(j);if(j->pos>=j->len)return -1;
 char c=j->s[j->pos];
 if(c=='"'){size_t a,b;return quoted(j,&a,&b);}
 if(c=='{'||c=='['){
  j->pos++;ws(j);char close=c=='{'?'}':']';
  if(j->pos<j->len&&j->s[j->pos]==close){j->pos++;return 0;}
  for(;;){
   if(c=='{'){size_t a,b;if(quoted(j,&a,&b))return -1;ws(j);if(j->pos>=j->len||j->s[j->pos++]!=':')return -1;}
   if(skip(j,depth+1))return -1;
   ws(j);if(j->pos>=j->len)return -1;
   if(j->s[j->pos]==close){j->pos++;return 0;}
   if(j->s[j->pos++]!=',')return -1;
  }
 }
 size_t start=j->pos;
 while(j->pos<j->len&&!strchr(" \t\n\r,]}",j->s[j->pos]))j->pos++;
 if(start==j->pos)return -1;
 /* Primitive values are bounded; reject malformed primitive tokens. */
 size_t n=j->pos-start;
 if((n==4&&!strncmp(j->s+start,"true",4))||(n==5&&!strncmp(j->s+start,"false",5))||
    (n==4&&!strncmp(j->s+start,"null",4)))return 0;
 char *endptr=NULL;
 char buffer[64];if(n>=sizeof(buffer))return -1;
 memcpy(buffer,j->s+start,n);buffer[n]=0;
 (void)strtod(buffer,&endptr);
 return endptr!=buffer&&*endptr==0?0:-1;
}
static int same(const json *j,size_t a,size_t b,const char *s){
 return (b-a)==strlen(s)&&!memcmp(j->s+a,s,b-a);
}
static int field(json *j,size_t start,size_t end,const char *key,size_t *value_start,size_t *value_end){
 if(start>=end||j->s[start]!='{')return 0;
 json p=*j;p.pos=start+1;
 for(;;){
  ws(&p);if(p.pos>=end||p.s[p.pos]=='}')return 0;
  size_t ka,kb;if(quoted(&p,&ka,&kb))return -1;
  ws(&p);if(p.pos>=end||p.s[p.pos++]!=':')return -1;
  ws(&p);size_t a=p.pos;if(skip(&p,0))return -1;
  if(same(j,ka,kb,key)){*value_start=a;*value_end=p.pos;return 1;}
  ws(&p);
  if(p.pos>=end||p.s[p.pos]=='}')return 0;
  if(p.s[p.pos++]!=',')return -1;
 }
}
static int copy_string(json *j,size_t start,size_t end,char *out,size_t cap){
 if(cap==0||start>=end||j->s[start]!='"')return -1;
 json p=*j;p.pos=start;size_t a,b;
 if(quoted(&p,&a,&b)||p.pos!=end)return -1;
 size_t x=0;
 for(size_t i=a;i<b;i++){
  unsigned char c=(unsigned char)j->s[i];
  if(c=='\\'){
   if(++i>=b)return -1;
   switch(j->s[i]){
    case 'n':c=' ';break;case 'r':c=' ';break;case 't':c=' ';break;
    case '"':c='"';break;case '\\':c='\\';break;case '/':c='/';break;
    case 'b':case 'f':c=' ';break;
    case 'u':c='?';i+=4;break;
    default:return -1;
   }
  }
  if(x+1>=cap)return -1;
  out[x++]=(char)c;
 }
 out[x]=0;return 0;
}
static int read_field_string(json *j,size_t a,size_t b,const char *key,char *out,size_t cap){
 size_t va=0,vb=0;int f=field(j,a,b,key,&va,&vb);
 if(f<0)return -1;
 return f==1?copy_string(j,va,vb,out,cap):0;
}
int rm_openapi_import(rm_catalog *cat,const char *source,const char *sha,const char *text,size_t len,char *err,size_t cap){
 if(!cat||!text||len>RM_MAX_DOCUMENT_BYTES)return rm_error(err,cap,"OpenAPI source invalid or over 2 MiB");
 json j={text,len,0,0};ws(&j);
 if(j.pos>=len||j.s[j.pos]!='{')return rm_error(err,cap,"expected OpenAPI JSON object");
 size_t root=j.pos;if(skip(&j,0))return rm_error(err,cap,"invalid OpenAPI JSON");
 ws(&j);if(j.pos!=len)return rm_error(err,cap,"trailing JSON data");
 char version[64]={0};
 if(read_field_string(&j,root,len,"openapi",version,sizeof(version))||strncmp(version,"3.",2))
   return rm_error(err,cap,"expected OpenAPI version 3.x JSON");
 size_t paths_start,paths_end;
 if(field(&j,root,len,"paths",&paths_start,&paths_end)!=1||j.s[paths_start]!='{')
   return rm_error(err,cap,"OpenAPI paths object required");
 json p=j;p.pos=paths_start+1;
 size_t starting_count=cat->operation_count;
 for(;;){
  ws(&p);if(p.pos>=paths_end)return rm_error(err,cap,"invalid paths object");
  if(p.s[p.pos]=='}')break;
  size_t ka,kb;
  if(quoted(&p,&ka,&kb))return rm_error(err,cap,"invalid API path");
  char path[RM_MAX_PATH];
  /* Quoted bounds include the delimiters, including escaped chars. */
  if(copy_string(&j,ka-1,kb+1,path,sizeof(path)))return rm_error(err,cap,"API path exceeds limit");
  if(path[0]!='/'||strchr(path,'\r')||strchr(path,'\n'))
    return rm_error(err,cap,"invalid declared operation path");
  ws(&p);if(p.pos>=paths_end||p.s[p.pos++]!=':')return rm_error(err,cap,"invalid API path object");
  ws(&p);size_t value=p.pos;
  if(skip(&p,0))return rm_error(err,cap,"invalid path item");
  if(j.s[value]=='{'){
   static const char *methods[]={"get","post","put","patch","delete","head","options","trace"};
   for(size_t i=0;i<8;i++){
    size_t oa,ob;
    if(field(&j,value,p.pos,methods[i],&oa,&ob)!=1||j.s[oa]!='{')continue;
    if(cat->operation_count>=RM_MAX_OPERATIONS)return rm_error(err,cap,"too many declared operations");
    rm_operation *op=&cat->operations[cat->operation_count++];
    memset(op,0,sizeof(*op));
    for(size_t z=0;methods[i][z]&&z<sizeof(op->method)-1;z++)
     op->method[z]=(char)toupper((unsigned char)methods[i][z]);
    snprintf(op->path,sizeof(op->path),"%s",path);
    if(read_field_string(&j,oa,ob,"summary",op->summary,sizeof(op->summary)) ||
       read_field_string(&j,oa,ob,"operationId",op->operation_id,sizeof(op->operation_id)))
      return rm_error(err,cap,"operation metadata exceeds limit");
    if(source)snprintf(op->source_url,sizeof(op->source_url),"%s",source);
    if(sha)snprintf(op->source_sha256,sizeof(op->source_sha256),"%s",sha);
   }
  }
  ws(&p);if(p.pos>=paths_end)return rm_error(err,cap,"invalid paths separator");
  if(p.s[p.pos]=='}')break;
  if(p.s[p.pos++]!=',')return rm_error(err,cap,"invalid paths comma");
 }
 if(err&&cap)err[0]=0;
 return (int)(cat->operation_count-starting_count);
}

/* Import our own saved library.json and compatible RESTLESS library.json.
 * This reads only local bytes; it does not send network requests. */
int rm_catalog_import_json(rm_catalog *cat,const char *text,size_t len,char *err,size_t cap){
 if(!cat||!text||len>RM_MAX_DOCUMENT_BYTES)return rm_error(err,cap,"catalog exceeds 2 MiB");
 json j={text,len,0,0};ws(&j);
 if(j.pos>=len||j.s[j.pos]!='{')return rm_error(err,cap,"expected catalog JSON object");
 size_t root=j.pos;if(skip(&j,0))return rm_error(err,cap,"invalid catalog JSON");
 ws(&j);if(j.pos!=len)return rm_error(err,cap,"trailing catalog JSON");
 size_t a,b;
 if(field(&j,root,len,"openapi",&a,&b)==1){
  memset(cat,0,sizeof(*cat));
  return rm_openapi_import(cat,"local OpenAPI JSON","",text,len,err,cap);
 }
 if(field(&j,root,len,"schema_version",&a,&b)!=1||b-a!=1||text[a]!='1')
  return rm_error(err,cap,"expected RESTMORE/RESTLESS schema_version 1 or OpenAPI 3");
 memset(cat,0,sizeof(*cat));
 if(field(&j,root,len,"target",&a,&b)==1 && j.s[a]=='{'){
  if(read_field_string(&j,a,b,"host",cat->target.host,sizeof(cat->target.host))||
     read_field_string(&j,a,b,"seed_url",cat->target.url,sizeof(cat->target.url)))
   return rm_error(err,cap,"invalid catalog target");
 }
 if(field(&j,root,len,"documents",&a,&b)==1 && j.s[a]=='['){
  json p=j;p.pos=a+1;
  for(;;){
   ws(&p);if(p.pos>=b)return rm_error(err,cap,"invalid documents");
   if(p.s[p.pos]==']')break;
   size_t ds=p.pos;if(skip(&p,0))return rm_error(err,cap,"invalid document object");
   if(j.s[ds]=='{'){
    if(cat->document_count>=RM_MAX_DOCUMENTS)return rm_error(err,cap,"too many catalog documents");
    rm_document *d=&cat->documents[cat->document_count++];
    if(read_field_string(&j,ds,p.pos,"url",d->url,sizeof(d->url)) ||
       read_field_string(&j,ds,p.pos,"sha256",d->sha256,sizeof(d->sha256)) ||
       read_field_string(&j,ds,p.pos,"content_type",d->content_type,sizeof(d->content_type)))
     return rm_error(err,cap,"invalid document metadata");
   }
   ws(&p);if(p.pos<b&&p.s[p.pos]==']')break;
   if(p.pos>=b||p.s[p.pos++]!=',')return rm_error(err,cap,"invalid documents separator");
  }
 }
 if(field(&j,root,len,"operations",&a,&b)!=1||j.s[a]!='[')
  return rm_error(err,cap,"catalog operations array missing");
 json p=j;p.pos=a+1;
 for(;;){
  ws(&p);if(p.pos>=b)return rm_error(err,cap,"invalid operations");
  if(p.s[p.pos]==']')break;
  size_t os=p.pos;if(skip(&p,0))return rm_error(err,cap,"invalid operation");
  if(j.s[os]=='{'){
   if(cat->operation_count>=RM_MAX_OPERATIONS)return rm_error(err,cap,"too many catalog operations");
   rm_operation *op=&cat->operations[cat->operation_count];
   if(read_field_string(&j,os,p.pos,"method",op->method,sizeof(op->method)) ||
      read_field_string(&j,os,p.pos,"path",op->path,sizeof(op->path)) ||
      read_field_string(&j,os,p.pos,"summary",op->summary,sizeof(op->summary)) ||
      read_field_string(&j,os,p.pos,"operation_id",op->operation_id,sizeof(op->operation_id)) ||
      read_field_string(&j,os,p.pos,"source_url",op->source_url,sizeof(op->source_url)) ||
      read_field_string(&j,os,p.pos,"source_sha256",op->source_sha256,sizeof(op->source_sha256)))
     return rm_error(err,cap,"invalid operation metadata");
   /* RESTLESS's library.json may use operationId rather than operation_id. */
   if(!op->operation_id[0]&&read_field_string(&j,os,p.pos,"operationId",op->operation_id,sizeof(op->operation_id)))
    return rm_error(err,cap,"invalid operationId");
   static const char *verbs[]={"GET","POST","PUT","PATCH","DELETE","HEAD","OPTIONS","TRACE"};
   int valid=0;for(size_t i=0;i<8;i++)if(!strcmp(op->method,verbs[i]))valid=1;
   if(!valid||op->path[0]!='/'||strchr(op->path,'\r')||strchr(op->path,'\n'))
    return rm_error(err,cap,"invalid catalog method or path");
   cat->operation_count++;
  }
  ws(&p);if(p.pos<b&&p.s[p.pos]==']')break;
  if(p.pos>=b||p.s[p.pos++]!=',')return rm_error(err,cap,"invalid operations separator");
 }
 if(err&&cap)err[0]=0;
 return (int)cat->operation_count;
}
