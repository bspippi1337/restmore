#include "restmore/restmore.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h> /* Keep regression calls active in Release. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
 char sha[65],err[256]={0};
 rm_sha256("",0,sha);
 assert(!strcmp(sha,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
 rm_sha256("abc",3,sha);
 assert(!strcmp(sha,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
 rm_target target={0};
 assert(rm_normalize("docs.example.org",&target,err,sizeof(err))==0);
 assert(!strcmp(target.url,"https://docs.example.org/"));
 assert(rm_normalize("https://docs.example.org/reference/openapi.json",&target,err,sizeof(err))==0);
 assert(rm_normalize("http://docs.example.org",&target,err,sizeof(err))!=0);
 assert(rm_normalize("127.0.0.1",&target,err,sizeof(err))!=0);
 assert(rm_normalize("localhost",&target,err,sizeof(err))!=0);
 assert(rm_normalize("private.internal",&target,err,sizeof(err))!=0);
 assert(rm_normalize("docs.example.org:8443",&target,err,sizeof(err))!=0);
 assert(rm_normalize("https://user@docs.example.org",&target,err,sizeof(err))!=0);
 assert(rm_normalize("docs.example.org/docs?token=x",&target,err,sizeof(err))!=0);
 assert(rm_normalize("docs.example.org/../admin",&target,err,sizeof(err))!=0);
 assert(rm_normalize("docs.example.org/%2e%2e",&target,err,sizeof(err))!=0);
 const char *json="{\"openapi\":\"3.1.0\",\"info\":{\"title\":\"Atlas\",\"version\":\"1\"},"
                  "\"paths\":{\"/v1/people\":{\"get\":{\"summary\":\"List people\",\"operationId\":\"listPeople\"},"
                  "\"post\":{\"summary\":\"Create person\"}},\"/v1/people/{id}\":{\"delete\":{\"summary\":\"Remove person\"}}}}";
 rm_catalog *cat=(rm_catalog *)calloc(1,sizeof(*cat));assert(cat);
 assert(rm_openapi_import(cat,"file:fixture.json",sha,json,strlen(json),err,sizeof(err))==3);
 assert(cat->operation_count==3);
 assert(!strcmp(cat->operations[0].method,"GET"));
 assert(!strcmp(cat->operations[0].path,"/v1/people"));
 assert(!strcmp(cat->operations[0].summary,"List people"));
 assert(!strcmp(cat->operations[2].method,"DELETE"));
 const char *bad="{\"openapi\":\"2.0\",\"paths\":{}}";
 assert(rm_openapi_import(cat,"","",bad,strlen(bad),err,sizeof(err))<0);
 assert(cat->operation_count==3);
 assert(rm_make_dir("restmore-test-out")==0);
 assert(rm_catalog_save(cat,"restmore-test-out",err,sizeof(err))==0);
 FILE *f=fopen("restmore-test-out/library.json","rb");assert(f);
 assert(fseek(f,0,SEEK_END)==0);long n=ftell(f);assert(n>100);fclose(f);
 rm_options opt={0};opt.input="docs.example.org";opt.max_pages=12;opt.max_depth=2;
 assert(rm_crawl(&opt,cat,NULL,NULL,err,sizeof(err))==0);
 assert(cat->document_count==0);
 assert(rm_crawl(&(rm_options){.input="docs.example.org",.execute=1},cat,NULL,NULL,err,sizeof(err))!=0);
 free(cat);
 puts("RESTMORE C11 regression suite: PASS");
 return 0;
}
