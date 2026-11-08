#include "restmore/restmore.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 char error[256]={0},github[RM_MAX_URL]={0};
 assert(rm_github_lookup("api.example.org",github,sizeof(github),error,sizeof(error))==0);
 assert(!strcmp(github,"https://github.com/search?q=api.example.org&type=repositories"));
 assert(rm_github_lookup("https://api.example.org/private",github,sizeof(github),error,sizeof(error))!=0);
 assert(rm_github_lookup("127.0.0.1",github,sizeof(github),error,sizeof(error))!=0);
 const char *fixture="{\"openapi\":\"3.1.0\",\"servers\":[{\"url\":\"https://api.example.org\"}],\"paths\":{"
 "\"/v1/pets\":{\"get\":{\"operationId\":\"listPets\",\"responses\":{\"200\":{\"content\":{\"application/json\":{\"schema\":{\"type\":\"array\"}}}}}}},"
 "\"/v1/ping\":{\"get\":{\"responses\":{\"204\":{\"description\":\"empty\"}}}},"
 "\"/v1/pets/{id}\":{\"get\":{\"responses\":{\"200\":{}}}},"
 "\"/v1/logout\":{\"get\":{\"responses\":{\"200\":{}}}},"
 "\"/v1/unknown\":{\"get\":{}},"
 "\"/v1/pets/create\":{\"post\":{\"responses\":{\"200\":{}}}}}}";
 rm_catalog *cat=calloc(1,sizeof(*cat));
 rm_verification *plan=calloc(1,sizeof(*plan));
 assert(cat&&plan);
 char sha[65];rm_sha256(fixture,strlen(fixture),sha);
 assert(rm_openapi_import(cat,"file:fixture.json",sha,fixture,strlen(fixture),error,sizeof(error))==6);
 assert(cat->operation_count==6);
 assert(cat->operations[0].expected_status==200);
 assert(!strcmp(cat->operations[0].expected_content_type,"application/json"));
 assert(!strcmp(cat->operations[0].expected_json_type,"array"));
 assert(rm_json_root_is("[1,2,3]",7,"array")==1);
 assert(rm_json_root_is("{\"ok\":true}",11,"array")==0);
 assert(rm_json_root_is("{\"ok\":true}",11,"object")==1);
 assert(cat->operations[1].expected_status==204);
 assert(rm_verify_plan(cat,"api.example.org",plan,error,sizeof(error))==0);
 assert(plan->count==2);
 assert(plan->authorized==0);
 assert(plan->source_bound==1);
 assert(!strcmp(plan->checks[0].path,"/v1/pets"));
 assert(plan->checks[0].expected_status==200);
 assert(!strcmp(plan->checks[0].expected_content_type,"application/json"));
 assert(!strcmp(plan->checks[1].path,"/v1/ping"));
 assert(plan->checks[1].expected_status==204);
 assert(rm_verify_run(plan,NULL,NULL,error,sizeof(error))!=0); /* no network */
 /* Even an explicit authorization token cannot test an unbound sample atlas. */
 cat->target.host[0]=0;cat->target.url[0]=0;
 assert(rm_verify_plan(cat,"api.example.org",plan,error,sizeof(error))==0);
 assert(plan->source_bound==0);
 plan->authorized=1;
 assert(rm_verify_run(plan,NULL,NULL,error,sizeof(error))!=0);
 assert(strstr(error,"not bound")!=NULL);
 assert(rm_normalize("api.example.org",&cat->target,error,sizeof(error))==0);
 assert(rm_verify_plan(cat,"api.example.org",plan,error,sizeof(error))==0);
 assert(plan->source_bound==1&&plan->authorized==0);
 assert(rm_json_valid("{\"ok\":true}",11)==1);
 assert(rm_json_valid("{\"ok\":NaN}",10)==0);
 /* local catalog roundtrip retains response status and MIME */
 assert(rm_make_dir("restmore-verify-test-out")==0);
 assert(rm_catalog_save(cat,"restmore-verify-test-out",error,sizeof(error))==0);
 FILE *f=fopen("restmore-verify-test-out/library.json","rb");assert(f);
 assert(fseek(f,0,SEEK_END)==0);long n=ftell(f);assert(n>0&&n<1000000);
 rewind(f);char *bytes=malloc((size_t)n+1);assert(bytes);
 assert(fread(bytes,1,(size_t)n,f)==(size_t)n);fclose(f);bytes[n]=0;
 rm_catalog *loaded=calloc(1,sizeof(*loaded));assert(loaded);
 assert(rm_catalog_import_json(loaded,bytes,(size_t)n,error,sizeof(error))==6);
 assert(loaded->operations[0].expected_status==200);
 assert(!strcmp(loaded->operations[0].expected_content_type,"application/json"));
 assert(!strcmp(loaded->operations[0].expected_json_type,"array"));
 free(bytes);free(loaded);free(plan);free(cat);
 puts("RESTMORE authorized verification planning: PASS");
 return 0;
}
