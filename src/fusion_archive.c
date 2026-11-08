/* RESTMORE 42 / RESTLESS Botanical archive bridge
 * Offline-only, fail-closed interchange. No network functions in this file.
 * Reads the historical RESTLESS index.jsonl + sha256/<digest> store; verifies
 * exact archived bytes and extracts ONLY explicit OpenAPI 3 operations.
 */
#include "internal.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FUSION_LINE_CAP 8192u
typedef struct {
 char url[RM_MAX_URL],sha[65],mime[96],etag[256],modified[96];
 unsigned depth;
 int status;
} archived;

static int hash_ok(const char *s){
 if(strlen(s)!=64)return 0;
 for(int i=0;i<64;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;
 return 1;
}
static const char *skip_space(const char *p){
 while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;
 return p;
}
/* The JSONL record is a flat object. Decode its quoted values without
 * accepting ambiguous escapes or injecting values from nested objects.
 * rm_json_valid confirms the complete line before extraction. */
static int field_string(const char *line,const char *name,char *out,size_t cap){
 const char *p=line;
 while(*p){
  if(*p!='"'){p++;continue;}
  p++;
  char key[80];size_t k=0;
  while(*p&&*p!='"'){
   if(*p=='\\'||k+1>=sizeof(key))return -1;
   key[k++]=*p++;
  }
  if(*p!='"')return -1;
  key[k]=0;p++;
  const char *v=skip_space(p);
  if(*v!=':'){p=v;continue;}
  v=skip_space(v+1);
  if(!strcmp(key,name)){
   if(*v!='"')return -1;
   v++;size_t n=0;
   while(*v&&*v!='"'){
    unsigned char c=(unsigned char)*v++;
    if(c=='\\'){
     unsigned char escaped=(unsigned char)*v++;
     if(escaped=='/'||escaped=='\\'||escaped=='"')c=escaped;
     else return -1; /* no control or Unicode escapes in validated URLs */
    }
    if(c<32||c>126||n+1>=cap)return -1;
    out[n++]=(char)c;
   }
   if(*v!='"')return -1;
   out[n]=0;return n?1:0;
  }
  p=v;
 }
 return 0;
}
static int load_file(const char *filename,char **body,size_t *length,
                     char *error,size_t cap){
 FILE *f=fopen(filename,"rb");
 if(!f)return rm_error(error,cap,"missing content-addressed RESTLESS archive");
 if(fseek(f,0,SEEK_END)){fclose(f);return rm_error(error,cap,"cannot seek archive");}
 long n=ftell(f);
 if(n<0||(unsigned long)n>RM_MAX_DOCUMENT_BYTES){fclose(f);
  return rm_error(error,cap,"archived document exceeds C11 document capacity");}
 rewind(f);
 char *data=(char *)malloc((size_t)n+1);
 if(!data){fclose(f);return rm_error(error,cap,"archive buffer allocation failed");}
 if(fread(data,1,(size_t)n,f)!=(size_t)n){fclose(f);free(data);
  return rm_error(error,cap,"short read of archived document");}
 if(fclose(f)){free(data);return rm_error(error,cap,"could not close archived document");}
 data[n]=0;*body=data;*length=(size_t)n;return 0;
}
static int json_mime(const char *mime){
 return !strncmp(mime,"application/json",16)||
        !strncmp(mime,"application/vnd.oai.openapi+json",32);
}
int rm_restless_import(const char *directory,rm_catalog *catalog,
                       rm_fusion_stats *stats,char *error,size_t cap){
 if(!directory||!directory[0]||strlen(directory)>2900||!catalog)
  return rm_error(error,cap,"RESTLESS archive directory and C11 catalog required");
 char manifest[4096];
 if(snprintf(manifest,sizeof(manifest),"%s/index.jsonl",directory)>=(int)sizeof(manifest))
  return rm_error(error,cap,"RESTLESS manifest path too long");
 FILE *f=fopen(manifest,"rb");
 if(!f)return rm_error(error,cap,"RESTLESS index.jsonl missing");
 /* Preserve destination on every failure. */
 archived *entries=(archived *)calloc(RM_MAX_DOCUMENTS,sizeof(*entries));
 rm_catalog *temp=(rm_catalog *)calloc(1,sizeof(*temp));
 if(!entries||!temp){fclose(f);free(entries);free(temp);
  return rm_error(error,cap,"RESTLESS import allocation failed");}
 rm_fusion_stats info={0};
 size_t used=0;char line[FUSION_LINE_CAP];
 char docs_host[254]={0};
 int failed=0;
 while(fgets(line,sizeof(line),f)){
  size_t n=strlen(line);
  if(!n)continue;
  if(n==sizeof(line)-1&&line[n-1]!='\n'){
   failed=rm_error(error,cap,"RESTLESS manifest record too long");break;
  }
  const char *p=skip_space(line);
  if(!*p)continue;
  if(!rm_json_valid(p,strlen(p))){
   failed=rm_error(error,cap,"RESTLESS manifest contains invalid JSON");break;
  }
  char url[RM_MAX_URL]={0},hash[65]={0},mime[96]={0},archivepath[128]={0};
  char etag[256]={0},modified[96]={0};
  if(field_string(p,"url",url,sizeof(url))!=1||
     field_string(p,"sha256",hash,sizeof(hash))!=1||
     field_string(p,"archive_path",archivepath,sizeof(archivepath))!=1||
     field_string(p,"content_type",mime,sizeof(mime))<0||
     field_string(p,"etag",etag,sizeof(etag))<0||
     field_string(p,"last_modified",modified,sizeof(modified))<0||!hash_ok(hash)){
   failed=rm_error(error,cap,"RESTLESS record lacks valid URL, digest or archive path");break;
  }
  char expected[80];snprintf(expected,sizeof(expected),"sha256/%s",hash);
  char windows[80];snprintf(windows,sizeof(windows),"sha256\\%s",hash);
  if(strcmp(archivepath,expected)&&strcmp(archivepath,windows)){
   failed=rm_error(error,cap,"RESTLESS archive path must equal sha256/<digest>");break;
  }
  rm_target normalized={0};
  if(rm_normalize(url,&normalized,error,cap)||strcmp(normalized.url,url)){
   failed=rm_error(error,cap,"RESTLESS source URL must be normalized public HTTPS");break;
  }
  if(!docs_host[0])snprintf(docs_host,sizeof(docs_host),"%s",normalized.host);
  if(strcmp(docs_host,normalized.host)){
   failed=rm_error(error,cap,"RESTLESS manifest mixes hosts: explicit separate scope required");break;
  }
  size_t found=used;
  for(size_t i=0;i<used;i++)if(!strcmp(entries[i].url,url)){found=i;break;}
  if(found==used){
   if(used>=RM_MAX_DOCUMENTS){
    failed=rm_error(error,cap,"unique document count exceeds current RESTMORE catalog capacity");break;
   }
   used++;
  }else info.revalidated++;
  snprintf(entries[found].url,sizeof(entries[found].url),"%s",url);
  snprintf(entries[found].sha,sizeof(entries[found].sha),"%s",hash);
  snprintf(entries[found].mime,sizeof(entries[found].mime),"%s",mime);
  snprintf(entries[found].etag,sizeof(entries[found].etag),"%s",etag);
  snprintf(entries[found].modified,sizeof(entries[found].modified),"%s",modified);
  info.manifest_records++;
 }
 if(ferror(f)&&!failed)failed=rm_error(error,cap,"RESTLESS manifest read failed");
 if(fclose(f)&&!failed)failed=rm_error(error,cap,"RESTLESS manifest close failed");
 if(!used&&!failed)failed=rm_error(error,cap,"RESTLESS archive manifest contains no documents");
 for(size_t i=0;!failed&&i<used;i++){
  char file[4096];
  if(snprintf(file,sizeof(file),"%s/sha256/%s",directory,entries[i].sha)>=(int)sizeof(file)){
   failed=rm_error(error,cap,"content-addressed archive path too long");break;
  }
  char *body=NULL;size_t length=0;
  if(load_file(file,&body,&length,error,cap)){failed=-1;break;}
  char actual[65];rm_sha256(body,length,actual);
  if(strcmp(actual,entries[i].sha)){
   free(body);failed=rm_error(error,cap,"RESTLESS archive SHA-256 mismatch: refuse tampered evidence");break;
  }
  rm_document *doc=&temp->documents[temp->document_count++];
  snprintf(doc->url,sizeof(doc->url),"%s",entries[i].url);
  snprintf(doc->sha256,sizeof(doc->sha256),"%s",actual);
  snprintf(doc->content_type,sizeof(doc->content_type),"%s",entries[i].mime);
  snprintf(doc->etag,sizeof(doc->etag),"%s",entries[i].etag);
  snprintf(doc->last_modified,sizeof(doc->last_modified),"%s",entries[i].modified);
  doc->bytes=length;doc->status=200;
  if(json_mime(entries[i].mime)){
   char import_error[256]={0};
   int added=rm_openapi_import(temp,doc->url,doc->sha256,body,length,import_error,sizeof(import_error));
   if(added>0){info.openapi_sources++;info.operations+=(size_t)added;}
  }
  free(body);
 }
 if(!failed){
  /* The RESTLESS documentation host may differ from an explicitly declared
     API server in OpenAPI. rm_openapi_import preserves server binding. */
  if(!temp->target.host[0]){
   if(rm_normalize(docs_host,&temp->target,error,cap))failed=-1;
  }
 }
 if(!failed){info.documents=temp->document_count;memcpy(catalog,temp,sizeof(*temp));
  if(stats)*stats=info;if(error&&cap)error[0]=0;}
 free(temp);free(entries);
 return failed?-1:0;
}
/* Stable offline relevance, derived entirely from documented source fields. */
static int contains_ci(const char *haystack,const char *term){
 size_t n=strlen(term);if(!n)return 0;
 for(const char *p=haystack;*p;p++){
  size_t i=0;
  while(i<n&&p[i]&&tolower((unsigned char)p[i])==tolower((unsigned char)term[i]))i++;
  if(i==n)return 1;
 }
 return 0;
}
size_t rm_fusion_search(const rm_catalog *catalog,const char *query,
                        rm_fusion_hit *hits,size_t capacity){
 if(!catalog||!query||!hits||capacity==0)return 0;
 char words[16][64];size_t word_count=0;
 const unsigned char *p=(const unsigned char *)query;
 while(*p&&word_count<16){
  while(*p&&!isalnum(*p))p++;
  if(!*p)break;
  size_t n=0;
  while(*p&&isalnum(*p)){
   if(n+1<sizeof(words[0]))words[word_count][n++]=(char)tolower(*p);
   p++;
  }
  if(n){words[word_count][n]=0;word_count++;}
 }
 if(!word_count)return 0;
 size_t filled=0;
 for(size_t i=0;i<catalog->operation_count;i++){
  const rm_operation *op=&catalog->operations[i];
  unsigned score=0,coverage=0;
  for(size_t w=0;w<word_count;w++){
   unsigned weight=0;
   if(contains_ci(op->operation_id,words[w]))weight+=8;
   if(contains_ci(op->path,words[w]))weight+=6;
   if(contains_ci(op->summary,words[w]))weight+=5;
   if(contains_ci(op->method,words[w]))weight+=2;
   if(contains_ci(op->source_url,words[w]))weight+=1;
   if(weight){coverage++;score+=weight;}
  }
  if(!score)continue;
  score+=coverage==word_count?20:coverage*2;
  size_t slot=filled<capacity?filled++:filled;
  if(slot==filled&&filled==capacity){
   if(score<=hits[filled-1].score)continue;
   slot=filled-1;
  }
  while(slot>0&&score>hits[slot-1].score){
   hits[slot]=hits[slot-1];slot--;
  }
  hits[slot]=(rm_fusion_hit){i,score};
 }
 return filled;
}
int rm_fusion_diff(const rm_catalog *before,const rm_catalog *after,
                   rm_fusion_diff_report *result){
 if(!before||!after||!result)return -1;
 memset(result,0,sizeof(*result));
 for(size_t i=0;i<after->operation_count;i++){
  const rm_operation *a=&after->operations[i],*b=NULL;
  for(size_t j=0;j<before->operation_count;j++)
   if(!strcmp(a->method,before->operations[j].method)&&
      !strcmp(a->path,before->operations[j].path)){b=&before->operations[j];break;}
  if(!b){result->added++;continue;}
  if(a->expected_status!=b->expected_status||
     strcmp(a->expected_content_type,b->expected_content_type)||
     strcmp(a->expected_json_type,b->expected_json_type)||
     strcmp(a->source_sha256,b->source_sha256))result->modified++;
  else result->unchanged++;
 }
 for(size_t i=0;i<before->operation_count;i++){
  const rm_operation *b=&before->operations[i];int found=0;
  for(size_t j=0;j<after->operation_count;j++)
   if(!strcmp(b->method,after->operations[j].method)&&
      !strcmp(b->path,after->operations[j].path)){found=1;break;}
  if(!found)result->removed++;
 }
 return 0;
}



/* The cached representation is eligible for conditional HTTP only after its
 * exact bytes have been checked against the latest manifest digest. */
int rm_fusion_cache_get(const char *directory,const char *url,rm_document *record,
                        char **body,size_t *length,char *error,size_t cap){
 if(!directory||!url||!record||!body||!length||strlen(directory)>2900)
  return rm_error(error,cap,"invalid cache lookup parameters");
 *body=NULL;*length=0;memset(record,0,sizeof(*record));
 rm_target target={0};
 if(rm_normalize(url,&target,error,cap)||strcmp(target.url,url))
  return rm_error(error,cap,"cache lookup requires normalized HTTPS source URL");
 char path[4096];
 if(snprintf(path,sizeof(path),"%s/index.jsonl",directory)>=(int)sizeof(path))
  return rm_error(error,cap,"cache manifest filename too long");
 FILE *f=fopen(path,"rb");
 if(!f){if(errno==ENOENT){if(error&&cap)error[0]=0;return 0;}
  return rm_error(error,cap,"cannot access existing cache manifest");}
 archived latest={0};char line[FUSION_LINE_CAP];
 int found=0,failed=0;
 while(fgets(line,sizeof(line),f)){
  size_t n=strlen(line);
  if(n==sizeof(line)-1&&line[n-1]!='\n'){
   failed=rm_error(error,cap,"cache manifest contains overlong record");break;}
  const char *p=skip_space(line);
  if(!*p)continue;
  if(!rm_json_valid(p,strlen(p))){
   failed=rm_error(error,cap,"invalid JSON in existing cache manifest");break;}
  char recorded_url[RM_MAX_URL]={0},sha[65]={0},rel[128]={0};
  char mime[96]={0},etag[256]={0},modified[96]={0};
  if(field_string(p,"url",recorded_url,sizeof(recorded_url))!=1||
     field_string(p,"sha256",sha,sizeof(sha))!=1||
     field_string(p,"archive_path",rel,sizeof(rel))!=1||
     field_string(p,"content_type",mime,sizeof(mime))<0||
     field_string(p,"etag",etag,sizeof(etag))<0||
     field_string(p,"last_modified",modified,sizeof(modified))<0||
     !hash_ok(sha)){
   failed=rm_error(error,cap,"invalid cached source metadata");break;
  }
  char expected[80],windows[80];
  snprintf(expected,sizeof(expected),"sha256/%s",sha);
  snprintf(windows,sizeof(windows),"sha256\\%s",sha);
  if(strcmp(rel,expected)&&strcmp(rel,windows)){
   failed=rm_error(error,cap,"cache archive path does not match SHA digest");break;
  }
  rm_target recorded={0};
  if(rm_normalize(recorded_url,&recorded,error,cap)||
     strcmp(recorded.host,target.host)||strcmp(recorded.url,recorded_url)){
   failed=rm_error(error,cap,"cache manifest contains mixed-host or unnormalized URL");break;
  }
  if(strcmp(recorded_url,url))continue;
  found=1;
  snprintf(latest.url,sizeof(latest.url),"%s",recorded_url);
  snprintf(latest.sha,sizeof(latest.sha),"%s",sha);
  snprintf(latest.mime,sizeof(latest.mime),"%s",mime);
  snprintf(latest.etag,sizeof(latest.etag),"%s",etag);
  snprintf(latest.modified,sizeof(latest.modified),"%s",modified);
 }
 if(ferror(f)&&!failed)failed=rm_error(error,cap,"cache manifest read failed");
 if(fclose(f)&&!failed)failed=rm_error(error,cap,"cache manifest close failed");
 if(failed)return -1;
 if(!found){if(error&&cap)error[0]=0;return 0;}
 if(snprintf(path,sizeof(path),"%s/sha256/%s",directory,latest.sha)>=(int)sizeof(path))
  return rm_error(error,cap,"cached archive path too long");
 char *bytes=NULL;size_t n=0;
 if(load_file(path,&bytes,&n,error,cap))return -1;
 char digest[65];rm_sha256(bytes,n,digest);
 if(strcmp(digest,latest.sha)){
  free(bytes);return rm_error(error,cap,"corrupted RESTLESS cache SHA-256: remote fetch refused");
 }
 snprintf(record->url,sizeof(record->url),"%s",latest.url);
 snprintf(record->sha256,sizeof(record->sha256),"%s",latest.sha);
 snprintf(record->content_type,sizeof(record->content_type),"%s",latest.mime);
 snprintf(record->etag,sizeof(record->etag),"%s",latest.etag);
 snprintf(record->last_modified,sizeof(record->last_modified),"%s",latest.modified);
 record->bytes=n;record->status=200;
 *body=bytes;*length=n;
 if(error&&cap)error[0]=0;
 return 1;
}

/* Append one RESTLESS-compatible provenance record AFTER archive bytes were
 * committed. RESTLESS can re-index this manifest with no format conversion. */
int rm_fusion_append_manifest(const char *directory,const rm_document *doc,
                              char *error,size_t cap){
 if(!directory||!directory[0]||strlen(directory)>2900||!doc||
    !hash_ok(doc->sha256)||doc->status!=200)
  return rm_error(error,cap,"valid archived document and provenance required");
 rm_target target={0};
 if(rm_normalize(doc->url,&target,error,cap)||strcmp(target.url,doc->url))
  return rm_error(error,cap,"cannot log unnormalized document URL");
 char index[4096],rel[128];
 if(snprintf(index,sizeof(index),"%s/index.jsonl",directory)>=(int)sizeof(index))
  return rm_error(error,cap,"manifest path too long");
 snprintf(rel,sizeof(rel),"sha256/%s",doc->sha256);
 time_t now=time(NULL);struct tm utc={0};
#ifdef _WIN32
 if(gmtime_s(&utc,&now)!=0)return rm_error(error,cap,"cannot produce UTC timestamp");
#else
 if(!gmtime_r(&now,&utc))return rm_error(error,cap,"cannot produce UTC timestamp");
#endif
 char stamp[32];
 if(!strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%SZ",&utc))
  return rm_error(error,cap,"cannot format archive timestamp");
 FILE *out=fopen(index,"ab");
 if(!out)return rm_error(error,cap,"cannot append RESTLESS provenance manifest");
 int bad=0;
 if(fputs("{\"url\":",out)==EOF||rm_json_escape(out,doc->url))bad=1;
 if(!bad&&(fputs(",\"source\":\"restmore-c11\",\"sha256\":",out)==EOF||
            rm_json_escape(out,doc->sha256)))bad=1;
 if(!bad&&(fputs(",\"archive_path\":",out)==EOF||rm_json_escape(out,rel)))bad=1;
 if(!bad&&(fputs(",\"content_type\":",out)==EOF||
            rm_json_escape(out,doc->content_type)))bad=1;
 if(!bad&&(fputs(",\"etag\":",out)==EOF||rm_json_escape(out,doc->etag)))bad=1;
 if(!bad&&(fputs(",\"last_modified\":",out)==EOF||rm_json_escape(out,doc->last_modified)))bad=1;
 if(!bad&&fprintf(out,",\"status\":200,\"response_status\":%d,"
        "\"bytes\":%zu,\"depth\":%u,\"fetched_at\":\"%s\"}\n",
        doc->response_status?doc->response_status:200,
        doc->bytes,doc->depth,stamp)<0)bad=1;
 if(fflush(out))bad=1;
 if(fclose(out))bad=1;
 if(bad)return rm_error(error,cap,"RESTLESS manifest append failed");
 if(error&&cap)error[0]=0;
 return 0;
}
