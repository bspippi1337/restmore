/* RESTMORE / EVIDENCE VERIFICATION PLAN
 * No network code: domain -> GitHub human review -> exact, documented safe GETs.
 * This is not fuzzing: no mutation, discovery or speculative paths. */
#include "internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int ok_path(const char *path){
 if(!path||path[0]!='/'||path[1]==0||strlen(path)>=RM_MAX_PATH)return 0;
 if(strstr(path,"//")||strstr(path,"/./")||strstr(path,"/../"))return 0;
 size_t n=strlen(path);
 if(n>=2&&(!strcmp(path+n-2,"/.")||(n>=3&&!strcmp(path+n-3,"/.."))))return 0;
 for(size_t i=1;i<n;i++){
  unsigned char c=(unsigned char)path[i];
  if(!(isalnum(c)||c=='/'||c=='_'||c=='-'||c=='.'||c=='~'))return 0;
 }
 /* Even a GET may cause destructive side effects on a poorly designed API.
    Reject obvious lifecycle, authentication, financial and admin endpoints. */
 static const char *deny[]={"logout","signout","login","password","token","secret","delete",
 "remove","reset","revoke","disable","shutdown","admin","purchase","payment","billing",
 "transaction","withdraw","transfer","refund","unsubscribe","session","credential"};
 char lower[RM_MAX_PATH];for(size_t i=0;i<n;i++)lower[i]=(char)tolower((unsigned char)path[i]);lower[n]=0;
 for(size_t i=0;i<sizeof(deny)/sizeof(deny[0]);i++)if(strstr(lower,deny[i]))return 0;
 return 1;
}
int rm_github_lookup(const char *domain,char *url,size_t cap,char *error,size_t errcap){
 rm_target target={0};
 if(rm_normalize(domain,&target,error,errcap))return -1;
 if(strcmp(target.url+8+strlen(target.host),"/"))
  return rm_error(error,errcap,"Enter one domain, not a path, to open GitHub lookup");
 int n=snprintf(url,cap,"https://github.com/search?q=%s&type=repositories",target.host);
 if(n<0||(size_t)n>=cap)return rm_error(error,errcap,"GitHub lookup URL buffer too small");
 if(error&&errcap)error[0]=0;
 return 0;
}
int rm_verify_plan(const rm_catalog *catalog,const char *domain,rm_verification *plan,
                   char *error,size_t cap){
 if(!catalog||!plan)return rm_error(error,cap,"Existing atlas and verification plan required");
 rm_target target={0};
 if(rm_normalize(domain,&target,error,cap))return -1;
 if(strcmp(target.url+8+strlen(target.host),"/"))
  return rm_error(error,cap,"Verification target must be a single domain without a path");
 memset(plan,0,sizeof(*plan));
 plan->target=target;
 plan->source_bound= catalog->target.host[0] && !strcmp(catalog->target.host,target.host);
 for(size_t i=0;i<catalog->operation_count && plan->count<RM_MAX_VERIFY_CHECKS;i++){
  const rm_operation *op=&catalog->operations[i];
  /* No parameters, no mutations, no guessing, and no undefined output contract. */
  if(strcmp(op->method,"GET")||!ok_path(op->path)||op->expected_status==0)continue;
  if(op->expected_status!=200&&op->expected_status!=204)continue;
  rm_check *item=&plan->checks[plan->count++];
  snprintf(item->method,sizeof(item->method),"GET");
  snprintf(item->path,sizeof(item->path),"%s",op->path);
  snprintf(item->source_sha256,sizeof(item->source_sha256),"%s",op->source_sha256);
  snprintf(item->expected_content_type,sizeof(item->expected_content_type),"%s",op->expected_content_type);
  snprintf(item->expected_json_type,sizeof(item->expected_json_type),"%s",op->expected_json_type);
  item->expected_status=op->expected_status;
  if(!plan->source_bound && op->source_url[0]){
   rm_target src={0};
   if(!rm_normalize(op->source_url,&src,NULL,0)&&!strcmp(src.host,target.host))
    plan->source_bound=1;
  }
 }
 if(!plan->count)
  return rm_error(error,cap,"No eligible documented GET with explicit 200/204 response; import a matching OpenAPI 3 document");
 if(error&&cap)error[0]=0;
 return 0;
}
