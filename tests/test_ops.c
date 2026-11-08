/* OPS 42: no-network operational brief contract and evidence regression. */
#include "restmore/restmore.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_report(const char *path){
 FILE *f=fopen(path,"rb");assert(f);
 assert(!fseek(f,0,SEEK_END));
 long n=ftell(f);assert(n>100 && n<1000000);
 rewind(f);
 char *out=calloc((size_t)n+1,1);assert(out);
 assert(fread(out,1,(size_t)n,f)==(size_t)n);
 assert(!fclose(f));
 return out;
}
int main(void){
 const char *spec=
  "{\"openapi\":\"3.1.0\","
  "\"servers\":[{\"url\":\"https://api.example.org\"}],"
  "\"paths\":{"
  "\"/v1/people\":{\"get\":{\"operationId\":\"listPeople\","
  "\"summary\":\"List people\",\"responses\":{"
  "\"200\":{\"content\":{\"application/json\":{"
  "\"schema\":{\"type\":\"array\"}}}}}},"
  "\"post\":{\"operationId\":\"createPerson\",\"responses\":{\"201\":{}}}"
  "},"
  "\"/v1/health\":{\"get\":{\"operationId\":\"health\","
  "\"responses\":{\"204\":{}}}}}}";
 char sha[65],error[256]={0};
 rm_sha256(spec,strlen(spec),sha);
 rm_catalog *cat=calloc(1,sizeof(*cat));assert(cat);
 assert(rm_openapi_import(cat,"sample-fixture.json",sha,spec,strlen(spec),
                          error,sizeof(error))==3);
 assert(cat->operation_count==3);
 rm_document *doc=&cat->documents[cat->document_count++];
 snprintf(doc->url,sizeof(doc->url),"https://api.example.org/openapi.json");
 snprintf(doc->sha256,sizeof(doc->sha256),"%s",sha);
 snprintf(doc->content_type,sizeof(doc->content_type),"application/json");
 doc->bytes=strlen(spec);doc->status=200;doc->response_status=200;

 rm_ops_summary first={0},second={0},mutated={0};
 assert(rm_ops_profile(cat,&first,error,sizeof(error))==0);
 assert(first.documents==1&&first.operations==3);
 assert(first.get_count==2&&first.post_count==1);
 assert(first.declared_success_responses==2);
 assert(first.document_hashes_recorded==1);
 assert(strlen(first.atlas_fingerprint_sha256)==64);
 assert(rm_ops_profile(cat,&second,error,sizeof(error))==0);
 assert(!strcmp(first.atlas_fingerprint_sha256,second.atlas_fingerprint_sha256));

 const char *report="restmore-ops-brief-test.json";
 assert(rm_ops_brief_write(cat,report,error,sizeof(error))==0);
 char *bytes=read_report(report);
 assert(rm_json_valid(bytes,strlen(bytes)));
 assert(strstr(bytes,"\"mission_state\":\"REVIEW_REQUIRED\"")!=NULL);
 assert(strstr(bytes,"\"target_execution\":false")!=NULL);
 assert(strstr(bytes,"\"authorized_by_this_export\":false")!=NULL);
 assert(strstr(bytes,"\"github_review_required\":true")!=NULL);
 assert(strstr(bytes,"\"hashes_reverified_by_this_export\":false")!=NULL);
 assert(strstr(bytes,"\"source_sha256\":")!=NULL);
 assert(strstr(bytes,"/v1/people")!=NULL);
 assert(strstr(bytes,"\"GET\":2")!=NULL);
 assert(strstr(bytes,first.atlas_fingerprint_sha256)!=NULL);
 char *saved=malloc(strlen(bytes)+1);assert(saved);strcpy(saved,bytes);
 free(bytes);
 /* Repeated exports of identical source data are byte-for-byte reproducible. */
 assert(rm_ops_brief_write(cat,report,error,sizeof(error))==0);
 bytes=read_report(report);
 assert(!strcmp(saved,bytes));free(saved);free(bytes);
 /* Any changed contract status must change the local source fingerprint. */
 cat->operations[0].expected_status=418;
 assert(rm_ops_profile(cat,&mutated,error,sizeof(error))==0);
 assert(strcmp(first.atlas_fingerprint_sha256,mutated.atlas_fingerprint_sha256));
 cat->operations[0].expected_status=200;
 /* No output is allowed for a corrupt overcapacity catalog. */
 cat->operation_count=RM_MAX_OPERATIONS+1;
 assert(rm_ops_brief_write(cat,report,error,sizeof(error))!=0);
 cat->operation_count=3;
 assert(rm_ops_brief_write(cat,"",error,sizeof(error))!=0);
 assert(!remove(report));
 free(cat);
 puts("RESTMORE OPS 42: deterministic, permission-neutral offline mission brief PASS");
 return 0;
}
