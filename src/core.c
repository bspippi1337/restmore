#include "restmore/restmore.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
static int fail(char *err,size_t cap,const char *message) {
  if(err&&cap) snprintf(err,cap,"%s",message);
  return -1;
}
static int suffix(const char *text,const char *ending) {
  size_t n=strlen(text),k=strlen(ending);
  return n>=k && !strcmp(text+n-k,ending);
}
int rm_normalize(const char *input,rm_target *out,char *error,size_t cap) {
  if(!input||!out) return fail(error,cap,"input and target required");
  size_t n=strlen(input);
  if(!n||n>1900) return fail(error,cap,"enter one domain or HTTPS documentation URL");
  while(n&&isspace((unsigned char)*input)){++input;--n;}
  while(n&&isspace((unsigned char)input[n-1])) --n;
  if(!n) return fail(error,cap,"empty domain");
  char url[RM_MAX_URL];
  if(n>=8 && !strncmp(input,"https://",8)) {
    if(n>=sizeof(url)) return fail(error,cap,"URL too long");
    memcpy(url,input,n);url[n]=0;
  } else {
    if(strstr(input,"://")) return fail(error,cap,"only HTTPS URLs allowed");
    if(n+8>=sizeof(url)) return fail(error,cap,"URL too long");
    memcpy(url,"https://",8);memcpy(url+8,input,n);url[n+8]=0;
  }
  for(const unsigned char *p=(const unsigned char*)url;*p;++p) {
    if(*p<33||*p>126||*p=='@'||*p=='?'||*p=='#'||*p=='\\')
      return fail(error,cap,"credentials, queries, fragments, spaces and escapes forbidden");
  }
  const char *p=url+8,*end=p;
  while(*end && *end!='/') ++end;
  size_t hn=(size_t)(end-p);
  if(hn<4||hn>253) return fail(error,cap,"invalid DNS hostname");
  char host[254];memcpy(host,p,hn);host[hn]=0;
  if(host[0]=='.'||host[hn-1]=='.'||!strchr(host,'.')||strchr(host,':'))
    return fail(error,cap,"public DNS hostname required; no ports or IP literals");
  int all_digits=1;
  for(size_t i=0;i<hn;i++) {
    host[i]=(char)tolower((unsigned char)host[i]);
    char c=host[i];
    if(c!='.'&&!isdigit((unsigned char)c))all_digits=0;
    if(!(isalnum((unsigned char)c)||c=='-'||c=='.'))
      return fail(error,cap,"invalid DNS character");
    if(c=='.'&&(i==0||host[i-1]=='.'||host[i-1]=='-'))
      return fail(error,cap,"invalid DNS label");
    if(c=='-'&&(i==0||host[i-1]=='.'||i+1==hn||host[i+1]=='.'))
      return fail(error,cap,"invalid DNS hyphen");
  }
  if(all_digits) return fail(error,cap,"IP addresses forbidden");
  static const char *blocked[]={".local",".localhost",".internal",".test",".invalid",".example",".onion",".home",".lan"};
  for(size_t i=0;i<sizeof(blocked)/sizeof(blocked[0]);i++)
    if(suffix(host,blocked[i]))return fail(error,cap,"reserved or local hostname forbidden");
  size_t pathlen=strlen(end);
  if(pathlen>RM_MAX_PATH-1) return fail(error,cap,"URL path too long");
  if(strchr(end,'%'))return fail(error,cap,"percent-encoded URL paths not supported");
  if(strstr(end,"//")||strstr(end,"/../")||strstr(end,"/./")||suffix(end,"/..")||suffix(end,"/."))
    return fail(error,cap,"unsafe URL path");
  memset(out,0,sizeof(*out));
  memcpy(out->host,host,hn+1);
  if(snprintf(out->url,sizeof(out->url),"https://%s%s",host,*end?end:"/")>=(int)sizeof(out->url))
    return fail(error,cap,"URL too long");
  if(error&&cap)error[0]=0;
  return 0;
}
int rm_make_dir(const char *path) {
  if(!path||!*path)return -1;
#ifdef _WIN32
  return _mkdir(path)==0||errno==EEXIST?0:-1;
#else
  return mkdir(path,0700)==0||errno==EEXIST?0:-1;
#endif
}
