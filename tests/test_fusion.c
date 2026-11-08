/* RESTLESS 42 -> RESTMORE Botanical integration regression tests.
 * No network, no fuzzing, no elevated privileges, no external fixtures. */
#include "restmore/restmore.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_bytes(const char *file,const char *bytes){
 FILE *f=fopen(file,"wb");assert(f);
 assert(fwrite(bytes,1,strlen(bytes),f)==strlen(bytes));
 assert(!fclose(f));
}
static void manifest(const char *dir,const char *hash,const char *archive_path){
 char file[4096];snprintf(file,sizeof(file),"%s/index.jsonl",dir);
 FILE *f=fopen(file,"wb");assert(f);
 assert(fprintf(f,
  "{\"url\":\"https://docs.example.org/openapi.json\","
  "\"source\":\"seed\",\"sha256\":\"%s\","
  "\"archive_path\":\"%s\","
  "\"content_type\":\"application/json\",\"status\":200}\n",
  hash,archive_path)>0);
 assert(!fclose(f));
}
int main(void){
 const char *dir="restmore-fusion-fixture";
 const char *doc=
  "{\"openapi\":\"3.1.0\",\"info\":{\"title\":\"Fusion fixture\"},"
  "\"servers\":[{\"url\":\"https://api.example.org\"}],"
  "\"paths\":{\"/v1/pets\":{\"get\":{\"operationId\":\"listPets\","
  "\"summary\":\"List documented pets\","
  "\"responses\":{\"200\":{\"content\":{\"application/json\":{"
  "\"schema\":{\"type\":\"array\"}}}}}}}}}";
 assert(rm_make_dir(dir)==0);
 char archives[4096];snprintf(archives,sizeof(archives),"%s/sha256",dir);
 assert(rm_make_dir(archives)==0);
 char sha[65],file[4096],rel[128],err[256]={0};rm_sha256(doc,strlen(doc),sha);
 snprintf(file,sizeof(file),"%s/%s",archives,sha);
 snprintf(rel,sizeof(rel),"sha256/%s",sha);
 write_bytes(file,doc);manifest(dir,sha,rel);
 rm_catalog *cat=calloc(1,sizeof(*cat)),*other=calloc(1,sizeof(*other));
 assert(cat&&other);
 rm_fusion_stats stats={0};
 assert(rm_restless_import(dir,cat,&stats,err,sizeof(err))==0);
 assert(stats.manifest_records==1&&stats.documents==1);
 assert(stats.openapi_sources==1&&stats.operations==1);
 assert(cat->document_count==1&&cat->operation_count==1);
 /* C crawler provenance is compatible with RESTLESS Botanical manifests.
    Repeated records are revalidations and latest URL entry wins. */
 snprintf(cat->documents[0].etag,sizeof(cat->documents[0].etag),"\"rev1\"");
 snprintf(cat->documents[0].last_modified,sizeof(cat->documents[0].last_modified),
          "Wed, 21 Oct 2015 07:28:00 GMT");
 assert(rm_fusion_append_manifest(dir,&cat->documents[0],err,sizeof(err))==0);
 rm_fusion_stats refresh={0};
 assert(rm_restless_import(dir,other,&refresh,err,sizeof(err))==0);
 assert(refresh.manifest_records==2);
 assert(refresh.revalidated==1 && refresh.documents==1);
 rm_document good_cache={0};char *good_bytes=NULL;size_t good_n=0;
 assert(rm_fusion_cache_get(dir,"https://docs.example.org/openapi.json",
                            &good_cache,&good_bytes,&good_n,err,sizeof(err))==1);
 assert(good_n==strlen(doc) && !memcmp(good_bytes,doc,good_n));
 assert(!strcmp(good_cache.sha256,sha));
 assert(!strcmp(good_cache.etag,"\"rev1\""));
 assert(!strcmp(good_cache.last_modified,"Wed, 21 Oct 2015 07:28:00 GMT"));
 free(good_bytes);
 assert(other->operation_count==1);
 assert(!strcmp(other->documents[0].etag,"\"rev1\""));
 assert(!strcmp(cat->target.host,"api.example.org"));
 assert(!strcmp(cat->operations[0].method,"GET"));
 assert(!strcmp(cat->operations[0].path,"/v1/pets"));
 assert(!strcmp(cat->operations[0].source_sha256,sha));
 assert(cat->operations[0].expected_status==200);
 assert(!strcmp(cat->operations[0].expected_json_type,"array"));

 rm_fusion_hit hits[4]={0};
 assert(rm_fusion_search(cat,"list pets",hits,4)==1);
 assert(hits[0].operation_index==0&&hits[0].score>0);
 assert(rm_fusion_search(cat,"absent phrase",hits,4)==0);
 assert(rm_fusion_search(cat,"",hits,4)==0);
 assert(rm_restless_import(dir,other,NULL,err,sizeof(err))==0);
 rm_fusion_diff_report delta={0};
 assert(rm_fusion_diff(cat,other,&delta)==0);
 assert(delta.added==0&&delta.removed==0&&delta.modified==0&&delta.unchanged==1);
 other->operations[0].expected_status=204;
 assert(rm_fusion_diff(cat,other,&delta)==0);
 assert(delta.modified==1);

 /* Never trust the manifest: the expected SHA must match exact bytes. */
 write_bytes(file,"tampered content");
 assert(rm_restless_import(dir,other,NULL,err,sizeof(err))!=0);
 assert(strstr(err,"SHA-256 mismatch")!=NULL);
 assert(other->operation_count==1); /* transactional: no partial mutation */
 rm_document cached={0};char *cached_bytes=NULL;size_t cached_size=0;
 assert(rm_fusion_cache_get(dir,"https://docs.example.org/openapi.json",
                           &cached,&cached_bytes,&cached_size,err,sizeof(err))==-1);
 assert(cached_bytes==NULL);
 assert(strstr(err,"SHA-256")!=NULL);
 write_bytes(file,doc);

 /* Archive path traversal and cross-host references must be rejected. */
 manifest(dir,sha,"../../private-key");
 assert(rm_restless_import(dir,other,NULL,err,sizeof(err))!=0);
 assert(strstr(err,"archive path")!=NULL);
 manifest(dir,sha,rel);
 const char *othertext=
  "{\"url\":\"https://unrelated.example.org/openapi.json\","
  "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
  "\"archive_path\":\"sha256/0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
  "\"content_type\":\"application/json\"}\n";
 char index[4096];snprintf(index,sizeof(index),"%s/index.jsonl",dir);
 FILE *out=fopen(index,"ab");assert(out);
 assert(fwrite(othertext,1,strlen(othertext),out)==strlen(othertext));assert(!fclose(out));
 assert(rm_restless_import(dir,other,NULL,err,sizeof(err))!=0);
 assert(strstr(err,"mixes hosts")!=NULL);
 manifest(dir,sha,rel);

 /* Empty or malformed JSONL cannot be trusted as a source of new paths. */
 write_bytes(index,"{\"url\":\"https://docs.example.org/openapi.json\",\"oops\":}\n");
 assert(rm_restless_import(dir,other,NULL,err,sizeof(err))!=0);
 assert(strstr(err,"invalid JSON")!=NULL);

 /* Adversarial metadata: nested fake URLs cannot override the top-level URL. */
 FILE *nested=fopen(index,"wb");assert(nested);
 assert(fprintf(nested,"{\"metadata\":{\"url\":\"https://adversary.example.org\"},"
  "\"url\":\"https://docs.example.org/openapi.json\","
  "\"sha256\":\"%s\",\"archive_path\":\"sha256/%s\","
  "\"content_type\":\"application/json\"}\n",sha,sha)>0);
 assert(!fclose(nested));
 assert(rm_restless_import(dir,other,NULL,err,sizeof(err))==0);
 assert(other->operation_count==1);
 /* Duplicate JSON keys are valid in some parsers but ambiguous as evidence.
    RESTMORE rejects a manifest that supplies two source URLs. */
 FILE *duplicates=fopen(index,"wb");assert(duplicates);
 assert(fprintf(duplicates,"{\"url\":\"https://docs.example.org/openapi.json\","
  "\"url\":\"https://adversary.example.org/openapi.json\","
  "\"sha256\":\"%s\",\"archive_path\":\"sha256/%s\","
  "\"content_type\":\"application/json\"}\n",sha,sha)>0);
 assert(!fclose(duplicates));
 assert(rm_restless_import(dir,other,NULL,err,sizeof(err))!=0);
 assert(strstr(err,"metadata")!=NULL);

 free(other);free(cat);
 assert(remove(index)==0);
 assert(remove(file)==0);
 /* No rmdir needed: CI test workspace is ephemeral. */
 puts("RESTMORE / RESTLESS Botanical fusion C11: PASS");
 return 0;
}
