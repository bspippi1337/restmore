/* RESTMORE's deliberately bounded HTTPS documentation harvester.
 * Only explicitly authorized GETs. Never tries arbitrary API methods,
 * guesses endpoint names, follows redirects, proxies or cross-origin URLs. */
#include "internal.h"
#include <curl/curl.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
#define RM_QUEUE_LIMIT 1024u
#define RM_ROBOTS_LIMIT (512u * 1024u)
typedef struct {char *data;size_t length,limit;int overflow;} response;
typedef struct {char url[RM_MAX_URL];unsigned depth;} queued;
typedef struct {char blocked[80][256];size_t count;unsigned delay_ms;} robot_rules;
static void delay_ms(unsigned ms) {
#ifdef _WIN32
 Sleep(ms);
#else
 struct timespec tm;tm.tv_sec=(time_t)(ms/1000);tm.tv_nsec=(long)(ms%1000)*1000000L;
 nanosleep(&tm,NULL);
#endif
}
static int is_public(const struct sockaddr *sa) {
#if defined(RESTMORE_TEST_ONLY_LOOPBACK)
 /* Debug Linux CI fixture only. This branch is never built with this macro
    for release artifacts. The matching runtime guard is also mandatory. */
 if(sa&&sa->sa_family==AF_INET){
  const unsigned char *a=(const unsigned char *)&((const struct sockaddr_in*)sa)->sin_addr;
  const char *guard=getenv("RESTMORE_CI_LOOPBACK_FIXTURE");
  if(a[0]==127&&a[1]==0&&a[2]==0&&a[3]==1&&
     guard&&!strcmp(guard,"restmore-e2e-fixture.example.org"))return 1;
 }
#endif
 if(sa->sa_family==AF_INET){
  const unsigned char *p=(const unsigned char *)&((const struct sockaddr_in *)sa)->sin_addr;
  unsigned a=p[0],b=p[1],c=p[2];
  if(a==0||a==10||a==127||a>=224||a==169&&b==254||a==172&&b>=16&&b<=31 ||
     a==192&&b==168||a==100&&b>=64&&b<=127||a==192&&b==0&&c==0||
     a==192&&b==0&&c==2||a==192&&b==88&&c==99||
     a==198&&b>=18&&b<=19||a==198&&b==51&&c==100||
     a==203&&b==0&&c==113||a==255)return 0;
  return 1;
 }
 if(sa->sa_family==AF_INET6){
  const unsigned char *p=((const struct sockaddr_in6 *)sa)->sin6_addr.s6_addr;
  if(p[0]<0x20||p[0]>0x3f)return 0; /* global unicast range only */
  if(p[0]==0x20&&p[1]==0x01&&p[2]==0x0d&&p[3]==0xb8)return 0;
  if(p[0]==0x20&&p[1]==0x02)return 0; /* 6to4: IPv4 tunnel */
  if(p[0]==0x20&&p[1]==0x01&&p[2]==0&&p[3]==0)return 0; /* Teredo */
  return 1;
 }
 return 0;
}
static curl_socket_t safe_socket(void *ctx,curlsocktype purpose,struct curl_sockaddr *addr){
 (void)ctx;(void)purpose;
 if(!addr||!is_public(&addr->addr))return CURL_SOCKET_BAD;
 return socket(addr->family,addr->socktype,addr->protocol);
}
static size_t writer(char *data,size_t sz,size_t nm,void *ctx) {
 response *r=(response *)ctx;
 if(sz&&nm>SIZE_MAX/sz)return 0;
 size_t n=sz*nm;
 if(n>r->limit-r->length){r->overflow=1;return 0;}
 memcpy(r->data+r->length,data,n);r->length+=n;r->data[r->length]=0;
 return n;
}

typedef struct {char etag[256],modified[96];} http_validators;
static int prefix_ci(const char *data,size_t len,const char *prefix){
 size_t n=strlen(prefix);
 if(len<n)return 0;
 for(size_t i=0;i<n;i++)if(tolower((unsigned char)data[i])!=
                            tolower((unsigned char)prefix[i]))return 0;
 return 1;
}
/* Reject CRLF, control characters, oversize or untrusted validator bytes. */
static int safe_validator(const char *value,size_t max_length){
 if(!value||!*value)return 0;
 size_t n=strlen(value);
 if(n>=max_length)return 0;
 for(size_t i=0;i<n;i++)
  if((unsigned char)value[i]<32||(unsigned char)value[i]>126)return 0;
 return 1;
}
static size_t read_header(char *ptr,size_t size,size_t count,void *ctx){
 if(size&&count>SIZE_MAX/size)return 0;
 size_t n=size*count;
 http_validators *v=(http_validators*)ctx;
 if(!v||n<3||n>512)return n;
 if(prefix_ci(ptr,n,"HTTP/")){
  v->etag[0]=0;v->modified[0]=0;return n;
 }
 char *out=NULL;size_t cap=0,skip=0;
 if(prefix_ci(ptr,n,"ETag:")){out=v->etag;cap=sizeof(v->etag);skip=5;}
 else if(prefix_ci(ptr,n,"Last-Modified:")){out=v->modified;cap=sizeof(v->modified);skip=14;}
 if(!out)return n;
 while(skip<n&&(ptr[skip]==' '||ptr[skip]=='\t'))skip++;
 size_t end=n;
 while(end>skip&&(ptr[end-1]=='\r'||ptr[end-1]=='\n'||
                    ptr[end-1]==' '||ptr[end-1]=='\t'))end--;
 if(end<=skip||end-skip>=cap)return n;
 for(size_t i=skip;i<end;i++)
  if((unsigned char)ptr[i]<32||(unsigned char)ptr[i]>126)return n;
 memcpy(out,ptr+skip,end-skip);out[end-skip]=0;
 return n;
}
static int fetch(const char *url,const char *agent,size_t limit,response *out,
                 char *mime,size_t mime_cap,long *status,char *err,size_t cap,
                 const char *etag,const char *modified,http_validators *seen){
 out->data=(char *)malloc(limit+1);out->length=0;out->limit=limit;out->overflow=0;
 if(!out->data)return rm_error(err,cap,"cannot allocate response buffer");
 out->data[0]=0; /* Empty HTTP responses remain NUL terminated. */
 CURL *c=curl_easy_init();if(!c){free(out->data);out->data=NULL;return rm_error(err,cap,"curl initialization failed");}
 curl_easy_setopt(c,CURLOPT_URL,url);
 curl_easy_setopt(c,CURLOPT_USERAGENT,agent);
 curl_easy_setopt(c,CURLOPT_ACCEPT_ENCODING,"identity");
 curl_easy_setopt(c,CURLOPT_HTTPGET,1L);
 curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(c,CURLOPT_MAXREDIRS,0L);
 curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"https");
 curl_easy_setopt(c,CURLOPT_PROXY,"");
 curl_easy_setopt(c,CURLOPT_NOPROXY,"*");
 curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);
#if defined(RESTMORE_TEST_ONLY_LOOPBACK)
 /* Only the test-only Debug build can trust an ephemeral, local fixture CA.
    Production releases always use the normal platform trust store. */
 const char *fixture_ca=getenv("RESTMORE_CI_CA_FILE");
 if(fixture_ca&&*fixture_ca)curl_easy_setopt(c,CURLOPT_CAINFO,fixture_ca);
#endif
 curl_easy_setopt(c,CURLOPT_SSLVERSION,CURL_SSLVERSION_TLSv1_2);
 curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,8L);
 curl_easy_setopt(c,CURLOPT_TIMEOUT,20L);
 curl_easy_setopt(c,CURLOPT_OPENSOCKETFUNCTION,safe_socket);
 curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,writer);
 curl_easy_setopt(c,CURLOPT_WRITEDATA,out);
 curl_easy_setopt(c,CURLOPT_MAXFILESIZE_LARGE,(curl_off_t)limit);
 http_validators ignored={0};
 if(!seen)seen=&ignored;
 memset(seen,0,sizeof(*seen));
 curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,read_header);
 curl_easy_setopt(c,CURLOPT_HEADERDATA,seen);
 struct curl_slist *headers=NULL;
 if(etag&&*etag){
  if(!safe_validator(etag,256)){curl_easy_cleanup(c);free(out->data);out->data=NULL;
   return rm_error(err,cap,"corrupt cached ETag validator");}
  char line[280];
  snprintf(line,sizeof(line),"If-None-Match: %s",etag);
  headers=curl_slist_append(headers,line);
  if(!headers){curl_easy_cleanup(c);free(out->data);out->data=NULL;
   return rm_error(err,cap,"cannot allocate conditional ETag header");}
 }
 if(modified&&*modified){
  if(!safe_validator(modified,96)){
   curl_slist_free_all(headers);curl_easy_cleanup(c);free(out->data);out->data=NULL;
   return rm_error(err,cap,"corrupt cached Last-Modified validator");}
  char line[130];
  snprintf(line,sizeof(line),"If-Modified-Since: %s",modified);
  struct curl_slist *next=curl_slist_append(headers,line);
  if(!next){curl_slist_free_all(headers);curl_easy_cleanup(c);free(out->data);out->data=NULL;
   return rm_error(err,cap,"cannot allocate conditional Last-Modified header");}
  headers=next;
 }
 if(headers)curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);
 CURLcode result=curl_easy_perform(c);
 *status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,status);
 char *type=NULL;curl_easy_getinfo(c,CURLINFO_CONTENT_TYPE,&type);
 if(mime&&mime_cap)snprintf(mime,mime_cap,"%s",type?type:"");
 if(result!=CURLE_OK){
  char message[256];
  snprintf(message,sizeof(message),"HTTPS request refused/failed: %.185s",out->overflow?"size limit reached":curl_easy_strerror(result));
  curl_slist_free_all(headers);curl_easy_cleanup(c);free(out->data);out->data=NULL;
  return rm_error(err,cap,message);
 }
 curl_slist_free_all(headers);curl_easy_cleanup(c);
 return 0;
}
static int eqprefix(const char *a,const char *b){
 while(*b){if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++))return 0;}return 1;
}
static void parse_robots(char *text,robot_rules *policy) {
 memset(policy,0,sizeof(*policy));policy->delay_ms=2000;
 for(char *line=strtok(text,"\r\n");line;line=strtok(NULL,"\r\n")){
  while(isspace((unsigned char)*line))line++;
  char *comment=strchr(line,'#');if(comment)*comment=0;
  char *colon=strchr(line,':');if(!colon)continue;
  *colon++=0;while(isspace((unsigned char)*colon))colon++;
  size_t n=strlen(colon);while(n&&isspace((unsigned char)colon[n-1]))colon[--n]=0;
  /* Conservative union of ALL user-agent groups; may block more than required. */
  if(eqprefix(line,"disallow")){
   if(n&&policy->count<80){
    size_t j=0;
    if(colon[0]=='*'){snprintf(policy->blocked[policy->count++],256,"/");continue;}
    while(j<n&&colon[j]!='*'&&colon[j]!='$'&&j<255)j++;
    if(j&&colon[0]=='/'){
     memcpy(policy->blocked[policy->count],colon,j);
     policy->blocked[policy->count][j]=0;policy->count++;
    }
   }
  }else if(eqprefix(line,"crawl-delay")){
   unsigned seconds=0;int valid=1;
   for(size_t j=0;j<n;j++)if(!isdigit((unsigned char)colon[j]))valid=0;
   if(n&&valid&&n<3){seconds=(unsigned)strtoul(colon,NULL,10);if(seconds>60)seconds=60;
    if(seconds*1000>policy->delay_ms)policy->delay_ms=seconds*1000;}
  }
 }
}
static int allowed(const robot_rules *rules,const char *url,const char *host){
 const char *path=url+8+strlen(host);
 if(!*path)path="/";
 for(size_t i=0;i<rules->count;i++)
  if(!strncmp(path,rules->blocked[i],strlen(rules->blocked[i])))return 0;
 return 1;
}
static int has_ending(const char *s,const char *ending) {
 size_t a=strlen(s),b=strlen(ending);return a>=b&&!strcmp(s+a-b,ending);
}
static int looks_like_docs(const char *url,const char *host){
 const char *p=url+8+strlen(host);
 const char *needles[]={"/docs","/documentation","/developer","/reference","/openapi","/swagger","/spec","/whitepaper","/papers"};
 for(size_t i=0;i<sizeof(needles)/sizeof(needles[0]);i++)if(strstr(p,needles[i]))return 1;
 const char *ext[]={".html",".htm",".md",".json",".yaml",".yml",".pdf",".txt"};
 for(size_t i=0;i<sizeof(ext)/sizeof(ext[0]);i++)if(has_ending(p,ext[i]))return 1;
 return 0;
}
static int type_ok(const char *type){
 const char *allowed_types[]={"text/html","text/plain","text/markdown","application/json","application/pdf",
  "application/yaml","application/x-yaml","text/yaml","application/vnd.oai.openapi+json"};
 for(size_t i=0;i<sizeof(allowed_types)/sizeof(allowed_types[0]);i++)
  if(!strncmp(type,allowed_types[i],strlen(allowed_types[i]))&&
     (type[strlen(allowed_types[i])]==0||type[strlen(allowed_types[i])]==';'))return 1;
 return 0;
}
static int link_to_url(const char *href,const char *base,const char *host,char url[RM_MAX_URL]){
 if(!href||!*href||strlen(href)>1000||strchr(href,'?')||strchr(href,'#')||strchr(href,'&')||
    strchr(href,'@')||strchr(href,'\\')||strchr(href,'%')||!strncmp(href,"//",2))return 0;
 char candidate[RM_MAX_URL];
 if(!strncmp(href,"https://",8))snprintf(candidate,sizeof(candidate),"%s",href);
 else if(href[0]=='/')snprintf(candidate,sizeof(candidate),"https://%s%s",host,href);
 else if(strstr(href,":")||href[0]=='.')return 0;
 else {
  const char *path=base+8+strlen(host),*slash=strrchr(path,'/');
  size_t directory=slash?(size_t)(slash-path+1):1;
  if(directory>RM_MAX_PATH)return 0;
  snprintf(candidate,sizeof(candidate),"https://%s%.*s%s",host,(int)directory,path,href);
 }
 rm_target target;
 if(rm_normalize(candidate,&target,NULL,0)||strcmp(target.host,host)||!looks_like_docs(target.url,host))return 0;
 snprintf(url,RM_MAX_URL,"%s",target.url);
 return 1;
}
static void discover(const response *body,const char *base,const char *host,queued *queue,size_t *count,
                     unsigned depth){
 if(*count>=RM_QUEUE_LIMIT)return;
 for(size_t i=0;i+7<body->length&&*count<RM_QUEUE_LIMIT;i++){
  if(tolower((unsigned char)body->data[i])!='h'||!eqprefix(body->data+i,"href"))continue;
  if(i&&isalnum((unsigned char)body->data[i-1]))continue;
  size_t p=i+4;
  while(p<body->length&&isspace((unsigned char)body->data[p]))p++;
  if(p>=body->length||body->data[p++]!='=')continue;
  while(p<body->length&&isspace((unsigned char)body->data[p]))p++;
  if(p>=body->length||(body->data[p]!='"'&&body->data[p]!='\''))continue;
  char quote=body->data[p++];size_t start=p;
  while(p<body->length&&body->data[p]!=quote&&p-start<1000)p++;
  if(p==body->length||p-start>=1000)continue;
  char href[1001];memcpy(href,body->data+start,p-start);href[p-start]=0;
  if(link_to_url(href,base,host,queue[*count].url)){queue[*count].depth=depth+1;(*count)++;}
  i=p;
 }
}
static int mkdir_layout(const char *root,const char *host,char archive[4096],char *err,size_t cap) {
 if(!root||!*root||strlen(root)>3000)return rm_error(err,cap,"invalid output directory");
 if(rm_make_dir(root))return rm_error(err,cap,"cannot create output directory");
 if(snprintf(archive,4096,"%s/%s",root,host)>=4096)return rm_error(err,cap,"output path too long");
 if(rm_make_dir(archive))return rm_error(err,cap,"cannot create domain directory");
 char sha[4096];snprintf(sha,sizeof(sha),"%s/sha256",archive);
 if(rm_make_dir(sha))return rm_error(err,cap,"cannot create SHA256 archive");
 return 0;
}
int rm_crawl(const rm_options *options,rm_catalog *cat,rm_progress cb,void *ctx,char *err,size_t cap){
 if(!options||!options->input||!cat)return rm_error(err,cap,"crawl configuration required");
 memset(cat,0,sizeof(*cat));
 if(rm_normalize(options->input,&cat->target,err,cap))return -1;
 unsigned budget=options->max_pages?options->max_pages:12;
 if(budget>RM_MAX_DOCUMENTS||options->max_depth>8)
  return rm_error(err,cap,"scope exceeds current C11 capacity (256 documents, depth 8)");
 if(!options->execute){if(cb)cb("preview",cat->target.url,ctx);return 0;}
 if(!options->authorized)return rm_error(err,cap,"network denied: --execute AND --authorized required");
 const char *agent=options->user_agent?options->user_agent:"RESTMORE/0.1 (+https://github.com/bspippi1337/restmore)";
 if(!*agent||strchr(agent,'\r')||strchr(agent,'\n'))return rm_error(err,cap,"invalid user agent");
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return rm_error(err,cap,"curl global init failed");
 char robots_url[RM_MAX_URL];
 snprintf(robots_url,sizeof(robots_url),"https://%s/robots.txt",cat->target.host);
 response robots={0};char mime[128];long status=0;
 if(cb)cb("robots",robots_url,ctx);
 if(fetch(robots_url,agent,RM_ROBOTS_LIMIT,&robots,mime,sizeof(mime),&status,err,cap,NULL,NULL,NULL)){curl_global_cleanup();return -1;}
 robot_rules rules={0};
 if(status==200)parse_robots(robots.data,&rules);
 else if(status==404||status==410)rules.delay_ms=2000;
 else {free(robots.data);curl_global_cleanup();return rm_error(err,cap,"robots.txt unavailable: fail closed");}
 free(robots.data);
 unsigned delay=options->delay_ms<2000?2000:options->delay_ms;
 if(rules.delay_ms>delay)delay=rules.delay_ms;
 const char *output=options->output_dir?options->output_dir:".restmore";
 char directory[4096];
 if(mkdir_layout(output,cat->target.host,directory,err,cap)){curl_global_cleanup();return -1;}
 queued *queue=(queued *)calloc(RM_QUEUE_LIMIT,sizeof(queued));
 if(!queue){curl_global_cleanup();return rm_error(err,cap,"cannot allocate crawl queue");}
 size_t qcount=1,qhead=0;snprintf(queue[0].url,sizeof(queue[0].url),"%s",cat->target.url);
 unsigned attempts=0;
 int result=0;
 while(qhead<qcount&&attempts<budget){
  queued *item=&queue[qhead++];
  int duplicate=0;
  for(size_t i=0;i+1<qhead;i++)if(!strcmp(queue[i].url,item->url)){duplicate=1;break;}
  if(duplicate||!allowed(&rules,item->url,cat->target.host))continue;
  /* Cache integrity is checked before a conditional request. The single
     pacing interval below applies to the actual HTTP GET. */
  /* Verify all cached source bytes BEFORE sending conditional headers.
     A tampered archive blocks the network request rather than being reused. */
  rm_document cached={0};char *old_body=NULL;size_t old_len=0;
  int cached_status=rm_fusion_cache_get(directory,item->url,&cached,&old_body,
                                        &old_len,err,cap);
  if(cached_status<0){result=-1;break;}
  if(cb)cb(cached_status?"revalidate":"fetch",item->url,ctx);
  delay_ms(delay);
  response body={0};mime[0]=0;status=0;attempts++;
  http_validators received={0};
  const char *etag=cached_status&&cached.etag[0]?cached.etag:NULL;
  const char *modified=cached_status&&cached.last_modified[0]?cached.last_modified:NULL;
  if(fetch(item->url,agent,RM_MAX_DOCUMENT_BYTES,&body,mime,sizeof(mime),
           &status,err,cap,etag,modified,&received)){
    free(old_body);result=-1;break;
  }
  if(status==429||status>=500){
   free(body.data);free(old_body);
   result=rm_error(err,cap,"server requested pause; stopped without retry");break;
  }
  if(status==304){
   if(!cached_status||(!etag&&!modified)){
    free(body.data);free(old_body);
    result=rm_error(err,cap,"unexpected HTTP 304 without SHA-verified conditional cache");
    break;
   }
   free(body.data);
   body.data=old_body;body.length=old_len;body.limit=old_len;old_body=NULL;
   snprintf(mime,sizeof(mime),"%s",cached.content_type);
  }else free(old_body);
  if((status!=200&&status!=304)||!type_ok(mime)){free(body.data);continue;}
  if(cat->document_count>=RM_MAX_DOCUMENTS){free(body.data);break;}
  rm_document *doc=&cat->documents[cat->document_count++];
  snprintf(doc->url,sizeof(doc->url),"%s",item->url);
  snprintf(doc->content_type,sizeof(doc->content_type),"%.95s",mime);
  doc->bytes=body.length;doc->depth=item->depth;doc->status=200;
  doc->response_status=(int)status;
  if(status==304){
   snprintf(doc->sha256,sizeof(doc->sha256),"%s",cached.sha256);
   snprintf(doc->etag,sizeof(doc->etag),"%s",
            received.etag[0]?received.etag:cached.etag);
   snprintf(doc->last_modified,sizeof(doc->last_modified),"%s",
            received.modified[0]?received.modified:cached.last_modified);
  }else{
   rm_sha256(body.data,body.length,doc->sha256);
   snprintf(doc->etag,sizeof(doc->etag),"%s",received.etag);
   snprintf(doc->last_modified,sizeof(doc->last_modified),"%s",received.modified);
  }
  if(status==200){
   char file[4096];
   if(snprintf(file,sizeof(file),"%s/sha256/%s",directory,doc->sha256)>=(int)sizeof(file)){
    free(body.data);result=rm_error(err,cap,"archive filename too long");break;
   }
   FILE *archive=fopen(file,"wb");
   if(!archive){
    free(body.data);result=rm_error(err,cap,"cannot write documentation archive");break;
   }
   int saved=fwrite(body.data,1,body.length,archive)==body.length;
   if(fclose(archive)||!saved){
    free(body.data);result=rm_error(err,cap,"archive write failed");break;
   }
  }
  /* RESTLESS Botanical reads this append-only record. A 304 never rewrites
     source bytes; its manifest refers to the independently verified digest. */
  if(rm_fusion_append_manifest(directory,doc,err,cap)){
   free(body.data);result=-1;break;
  }
  if(strstr(mime,"json")){
    /* Ordinary JSON is valid; only confirmed OpenAPI 3 paths are cataloged. */
    char import_error[256]={0};
    rm_openapi_import(cat,doc->url,doc->sha256,body.data,body.length,import_error,sizeof(import_error));
  }
  if(cb)cb(status==304?"reused-304":"archived",doc->url,ctx);
  if(item->depth<options->max_depth&&!strncmp(mime,"text/html",9))
    discover(&body,item->url,cat->target.host,queue,&qcount,item->depth);
  free(body.data);
 }
 if(!result&&cat->document_count==0)result=rm_error(err,cap,"no permitted documentation returned HTTP 200");
 if(!result)result=rm_catalog_save(cat,directory,err,cap);
 free(queue);curl_global_cleanup();
 return result;
}

/* Deliberately mirror the planner's conservative path restrictions. */
static int ok_path_verify(const char *p){
 if(!p||p[0]!='/'||p[1]==0||strlen(p)>=RM_MAX_PATH)return 0;
 if(strstr(p,"//")||strstr(p,"/./")||strstr(p,"/../"))return 0;
 size_t n=strlen(p);
 if((n>=2&&!strcmp(p+n-2,"/."))||(n>=3&&!strcmp(p+n-3,"/..")))return 0;
 for(size_t i=1;i<n;i++){
  unsigned char c=(unsigned char)p[i];
  if(!(isalnum(c)||c=='/'||c=='_'||c=='-'||c=='.'||c=='~'))return 0;
 }
 static const char *deny[]={"logout","signout","login","password","token","secret",
 "delete","remove","reset","revoke","disable","shutdown","admin","purchase",
 "payment","billing","transaction","withdraw","transfer","refund",
 "unsubscribe","session","credential"};
 char lo[RM_MAX_PATH];for(size_t i=0;i<n;i++)lo[i]=(char)tolower((unsigned char)p[i]);lo[n]=0;
 for(size_t k=0;k<sizeof(deny)/sizeof(deny[0]);k++)if(strstr(lo,deny[k]))return 0;
 return 1;
}
static int rm_mime_matches(const char *actual,const char *expected){
 size_t i=0;for(;expected[i];i++){
  if(!actual[i]||tolower((unsigned char)actual[i])!=
     tolower((unsigned char)expected[i]))return 0;
 }
 return actual[i]==0||actual[i]==';'||isspace((unsigned char)actual[i]);
}
/* A bounded, human-authorized audit of declared OpenAPI GET responses.
 * This never guesses routes, sends credentials, replays mutating operations,
 * follows redirects or stores response bodies. */
int rm_verify_run(rm_verification *plan,rm_progress cb,void *ctx,char *err,size_t cap){
 if(!plan||!plan->authorized||!plan->github_reviewed)
  return rm_error(err,cap,"Denied: review GitHub, then click Authorize for this exact domain");
 if(!plan->count||plan->count>RM_MAX_VERIFY_CHECKS)
  return rm_error(err,cap,"No bounded, documented verification plan");
 if(!plan->source_bound)return rm_error(err,cap,"Atlas is not bound to this domain; import its OpenAPI server contract first");
 rm_target host={0};
 if(rm_normalize(plan->target.host,&host,err,cap)||strcmp(host.host,plan->target.host))
  return rm_error(err,cap,"Verification domain no longer matches authorized plan");
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)
  return rm_error(err,cap,"libcurl initialization failed");
 char robots_url[RM_MAX_URL],mime[128]={0};
 snprintf(robots_url,sizeof(robots_url),"https://%s/robots.txt",host.host);
 response robots={0};long status=0;
 if(cb)cb("verification-robots",robots_url,ctx);
 if(fetch(robots_url,"RESTMORE-Verifier/0.1 (+https://github.com/bspippi1337/restmore)",
          RM_ROBOTS_LIMIT,&robots,mime,sizeof(mime),&status,err,cap,NULL,NULL,NULL)){
  curl_global_cleanup();return -1;
 }
 robot_rules rules={0};
 if(status==200)parse_robots(robots.data,&rules);
 else if(status==404||status==410)rules.delay_ms=2000;
 else{free(robots.data);curl_global_cleanup();
  return rm_error(err,cap,"Cannot verify robots policy; verification stopped without API requests");}
 free(robots.data);
 unsigned gap=rules.delay_ms>2000?rules.delay_ms:2000;
 for(size_t i=0;i<plan->count;i++){
  rm_check *check=&plan->checks[i];
  check->actual_status=0;check->observed_content_type[0]=0;check->outcome=0;
  /* Double-check exact, fixed read-only operations against the original planner. */
  if(strcmp(check->method,"GET")||!ok_path_verify(check->path)||
     (check->expected_status!=200&&check->expected_status!=204)){
   check->outcome=5;continue;
  }
  char url[RM_MAX_URL];
  int n=snprintf(url,sizeof(url),"https://%s%s",host.host,check->path);
  rm_target normalized={0};
  if(n<0||(size_t)n>=sizeof(url)||rm_normalize(url,&normalized,NULL,0)||
     strcmp(normalized.host,host.host)||!allowed(&rules,url,host.host)){
   check->outcome=5;continue;
  }
  if(cb)cb("verify-GET",url,ctx);
  delay_ms(gap);
  response body={0};status=0;mime[0]=0;
  if(fetch(url,"RESTMORE-Verifier/0.1 (+https://github.com/bspippi1337/restmore)",
           128u*1024u,&body,mime,sizeof(mime),&status,err,cap,NULL,NULL,NULL)){
    check->outcome=4;curl_global_cleanup();return -1;
  }
  check->actual_status=(int)status;
  snprintf(check->observed_content_type,sizeof(check->observed_content_type),"%s",mime);
  if(status==429||status>=500){
   check->outcome=4;free(body.data);curl_global_cleanup();
   return rm_error(err,cap,"Remote service requested pause; verification stopped");
  }
  if(status!=check->expected_status)check->outcome=3;
  else if(!check->expected_content_type[0])check->outcome=2;
  else if(!rm_mime_matches(mime,check->expected_content_type))check->outcome=3;
  else if(!strcmp(check->expected_content_type,"application/json")&&
          (!rm_json_valid(body.data,body.length)||
           (check->expected_json_type[0]&&
            !rm_json_root_is(body.data,body.length,check->expected_json_type))))
   check->outcome=3;
  else check->outcome=1;
  free(body.data);
 }
 curl_global_cleanup();
 if(err&&cap)err[0]=0;
 return 0;
}
