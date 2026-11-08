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
  /* A documentation URL alone is not proof of the API server origin.
     source_bound comes only from the catalog's explicit bound target. */
 }
 if(!plan->count)
  return rm_error(error,cap,"No eligible documented GET with explicit 200/204 response; import a matching OpenAPI 3 document");
 if(error&&cap)error[0]=0;
 return 0;
}


/* A local, inspectable audit file; does not record response bodies, secrets
 * or cookies. Each result is tied to its declared expectation, not a guess. */
int rm_verify_report(const rm_verification *plan,const char *path,
                     char *error,size_t cap){
 if(!plan||!path||!path[0]||!plan->target.host[0]||
    plan->count>RM_MAX_VERIFY_CHECKS)
  return rm_error(error,cap,"valid verification plan and output path required");
 size_t length=strlen(path);
 if(length>=4000)return rm_error(error,cap,"report filename too long");
 char tmp[4096];
 if(snprintf(tmp,sizeof(tmp),"%s.tmp",path)>=(int)sizeof(tmp))
  return rm_error(error,cap,"report filename too long");
 FILE *f=fopen(tmp,"wb");
 if(!f)return rm_error(error,cap,"cannot create verification report");
 int failure=0;
 if(fputs("{\n \"kind\":\"restmore-contract-verification\",\n \"version\":1,\n \"domain\":",f)==EOF)failure=1;
 if(!failure&&rm_json_escape(f,plan->target.host))failure=1;
 if(!failure&&fprintf(f,",\n \"github_review_acknowledged\":%s,\n \"explicit_authorization\":%s,\n \"source_bound\":%s,\n \"checks\":[\n",
    plan->github_reviewed?"true":"false",plan->authorized?"true":"false",
    plan->source_bound?"true":"false")<0)failure=1;
 for(size_t i=0;!failure&&i<plan->count;i++){
  const rm_check *c=&plan->checks[i];
  if(i&&fputs(",\n",f)==EOF)failure=1;
  if(!failure&&fputs("  {\"method\":",f)==EOF)failure=1;
  if(!failure&&rm_json_escape(f,c->method))failure=1;
  if(!failure&&fputs(",\"path\":",f)==EOF)failure=1;
  if(!failure&&rm_json_escape(f,c->path))failure=1;
  if(!failure&&fprintf(f,",\"expected_status\":%d,\"observed_status\":%d,\"outcome\":%d,\"expected_content_type\":",
                         c->expected_status,c->actual_status,c->outcome)<0)failure=1;
  if(!failure&&rm_json_escape(f,c->expected_content_type))failure=1;
  if(!failure&&fputs(",\"observed_content_type\":",f)==EOF)failure=1;
  if(!failure&&rm_json_escape(f,c->observed_content_type))failure=1;
  if(!failure&&fputs(",\"expected_json_type\":",f)==EOF)failure=1;
  if(!failure&&rm_json_escape(f,c->expected_json_type))failure=1;
  if(!failure&&fputs(",\"source_sha256\":",f)==EOF)failure=1;
  if(!failure&&rm_json_escape(f,c->source_sha256))failure=1;
  if(!failure&&fputc('}',f)==EOF)failure=1;
 }
 if(!failure&&fputs("\n ],\n \"note\":\"Status, MIME and root JSON syntax only; not full schema or endpoint completeness proof.\"\n}\n",f)==EOF)failure=1;
 if(fclose(f))failure=1;
 if(failure){remove(tmp);return rm_error(error,cap,"verification report write failed");}
#ifdef _WIN32
 remove(path);
#endif
 if(rename(tmp,path)){remove(tmp);return rm_error(error,cap,"cannot install verification report");}
 if(error&&cap)error[0]=0;
 return 0;
}
