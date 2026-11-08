/* RESTMORE / OPS 42
 * A deliberately OFFLINE, unclassified operational evidence briefing.
 * No network APIs, no target execution, no assessment of authorization.
 * All mission states are explicit: imported documentation is NOT permission.
 */
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int chain_hash(char digest[65],const char *record){
 size_t n=strlen(record);
 if(n>8192)return -1;
 char buffer[8192+66];
 size_t d=strlen(digest);
 if(d!=64||d+n+1>sizeof(buffer))return -1;
 memcpy(buffer,digest,d);
 memcpy(buffer+d,record,n);
 rm_sha256(buffer,d+n,digest);
 return 0;
}
static int update_fingerprint(char digest[65],const rm_catalog *cat){
 const char prefix[]="RESTMORE-OPS-42/v1";
 rm_sha256(prefix,sizeof(prefix)-1,digest);
 char record[8192];
 int n=snprintf(record,sizeof(record),"SCOPE:%zu:%s\n",
                strlen(cat->target.host),cat->target.host);
 if(n<0||(size_t)n>=sizeof(record)||chain_hash(digest,record))return -1;
 for(size_t i=0;i<cat->document_count;i++){
  const rm_document *d=&cat->documents[i];
  n=snprintf(record,sizeof(record),"D:%zu:%s:%s:%d:%zu\n",
             strlen(d->url),d->url,d->sha256,d->response_status,d->bytes);
  if(n<0||(size_t)n>=sizeof(record)||chain_hash(digest,record))return -1;
 }
 for(size_t i=0;i<cat->operation_count;i++){
  const rm_operation *o=&cat->operations[i];
  n=snprintf(record,sizeof(record),
    "O:%zu:%s:%zu:%s:%s:%s:%d:%s:%s:%s\n",
    strlen(o->method),o->method,strlen(o->path),o->path,
    o->operation_id,o->summary,o->expected_status,
    o->expected_content_type,o->expected_json_type,o->source_sha256);
  if(n<0||(size_t)n>=sizeof(record)||chain_hash(digest,record))return -1;
 }
 return 0;
}
int rm_ops_profile(const rm_catalog *cat,rm_ops_summary *out,
                   char *error,size_t cap){
 if(!cat||!out||cat->document_count>RM_MAX_DOCUMENTS||
    cat->operation_count>RM_MAX_OPERATIONS)
  return rm_error(error,cap,"invalid or overcapacity operational atlas");
 rm_ops_summary s={0};
 s.documents=cat->document_count;
 s.operations=cat->operation_count;
 for(size_t i=0;i<cat->document_count;i++){
  const rm_document *d=&cat->documents[i];
  if(strlen(d->sha256)==64)s.document_hashes_recorded++;
 }
 for(size_t i=0;i<cat->operation_count;i++){
  const rm_operation *op=&cat->operations[i];
  if(!strcmp(op->method,"GET"))s.get_count++;
  else if(!strcmp(op->method,"POST"))s.post_count++;
  else if(!strcmp(op->method,"PUT"))s.put_count++;
  else if(!strcmp(op->method,"PATCH"))s.patch_count++;
  else if(!strcmp(op->method,"DELETE"))s.delete_count++;
  else s.other_count++;
  if(op->expected_status==200||op->expected_status==204)
   s.declared_success_responses++;
 }
 if(update_fingerprint(s.atlas_fingerprint_sha256,cat))
  return rm_error(error,cap,"cannot fingerprint complete operational atlas");
 *out=s;
 if(error&&cap)error[0]=0;
 return 0;
}
int rm_ops_brief_write(const rm_catalog *cat,const char *path,
                       char *error,size_t cap){
 if(!path||!path[0]||strlen(path)>1900)
  return rm_error(error,cap,"operational brief output path invalid");
 rm_ops_summary s={0};
 if(rm_ops_profile(cat,&s,error,cap))return -1;
 char temp[2048];
 int n=snprintf(temp,sizeof(temp),"%s.tmp",path);
 if(n<0||(size_t)n>=sizeof(temp))
  return rm_error(error,cap,"operational brief output path too long");
 FILE *f=fopen(temp,"wb");
 if(!f)return rm_error(error,cap,"cannot create operational brief");
 int bad=0;
 if(fputs("{\n"
          " \"format\":\"restmore-ops-42/v1\",\n"
          " \"designation\":\"UNCLASSIFIED LOCAL WORKSHEET - NOT GOVERNMENT CLASSIFICATION\",\n"
          " \"mode\":\"OFFLINE_SOURCE_BRIEF\",\n"
          " \"target_execution\":false,\n"
          " \"permission_conferred\":false,\n"
          " \"mission_state\":\"REVIEW_REQUIRED\",\n"
          " \"approval_gates\":{\"github_review_required\":true,"
          "\"manual_authorization_required\":true,"
          "\"authorized_by_this_export\":false},\n"
          " \"scope\":{\"documented_server_host\":",f)==EOF)bad=1;
 if(!bad&&rm_json_escape(f,cat->target.host))bad=1;
 if(!bad&&fprintf(f,",\"host_bound\":%s,\"documents\":%zu,"
                      "\"declared_operations\":%zu},\n"
                      " \"evidence\":{\"atlas_fingerprint_sha256\":",
                cat->target.host[0]?"true":"false",
                s.documents,s.operations)<0)bad=1;
 if(!bad&&rm_json_escape(f,s.atlas_fingerprint_sha256))bad=1;
 if(!bad&&fprintf(f,",\"source_hashes_recorded\":%zu,"
                      "\"hashes_reverified_by_this_export\":false},\n"
                      " \"methods\":{\"GET\":%zu,\"POST\":%zu,\"PUT\":%zu,"
                      "\"PATCH\":%zu,\"DELETE\":%zu,\"OTHER\":%zu},\n"
                      " \"declared_success_responses\":%zu,\n"
                      " \"declared_routes\":[\n",
          s.document_hashes_recorded,s.get_count,s.post_count,s.put_count,
          s.patch_count,s.delete_count,s.other_count,
          s.declared_success_responses)<0)bad=1;
 for(size_t i=0;i<cat->operation_count&&!bad;i++){
  const rm_operation *op=&cat->operations[i];
  if(i&&fputs(",\n",f)==EOF)bad=1;
  if(!bad&&fputs("  {\"method\":",f)==EOF)bad=1;
  if(!bad&&rm_json_escape(f,op->method))bad=1;
  if(!bad&&fputs(",\"path\":",f)==EOF)bad=1;
  if(!bad&&rm_json_escape(f,op->path))bad=1;
  if(!bad&&fprintf(f,",\"declared_success_status\":%d,\"source_sha256\":",
                    op->expected_status)<0)bad=1;
  if(!bad&&rm_json_escape(f,op->source_sha256))bad=1;
  if(!bad&&fputc('}',f)==EOF)bad=1;
 }
 if(!bad&&fputs("\n ],\n"
     " \"interpretation\":\"Inventory from local documentation only. "
     "No service availability, authorization, exploitability or schema "
     "conformance is asserted. SHA-256 is an evidence fingerprint, "
     "not a signature or publisher authentication.\"\n}\n",f)==EOF)bad=1;
 if(fclose(f))bad=1;
 if(bad){remove(temp);return rm_error(error,cap,"operational brief write failed");}
#ifdef _WIN32
 remove(path);
#endif
 if(rename(temp,path)){
  remove(temp);return rm_error(error,cap,"cannot atomically install operational brief");
 }
 if(error&&cap)error[0]=0;
 return 0;
}
