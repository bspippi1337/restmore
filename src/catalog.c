#include "internal.h"
#include <stdlib.h>
int rm_json_escape(FILE *f,const char *s){
 if(fputc('"',f)==EOF)return -1;
 for(const unsigned char *p=(const unsigned char*)(s?s:"");*p;p++){
   if(*p=='"'||*p=='\\'){if(fputc('\\',f)==EOF||fputc(*p,f)==EOF)return -1;}
   else if(*p<0x20){if(fprintf(f,"\\u%04x",*p)<0)return -1;}
   else if(fputc(*p,f)==EOF)return -1;
 }
 return fputc('"',f)==EOF?-1:0;
}
int rm_catalog_save(const rm_catalog *cat,const char *dir,char *error,size_t cap){
 if(!cat||!dir||!dir[0])return rm_error(error,cap,"catalog and directory required");
 char file[4096],tmp[4096];
 if(snprintf(file,sizeof(file),"%s/library.json",dir)>=(int)sizeof(file) ||
    snprintf(tmp,sizeof(tmp),"%s/.library.tmp",dir)>=(int)sizeof(tmp))
   return rm_error(error,cap,"output path too long");
 FILE *f=fopen(tmp,"wb");
 if(!f)return rm_error(error,cap,"cannot create temporary catalog");
 int ok=0;
 if(fprintf(f,"{\n  \"schema_version\":1,\n  \"generator\":\"restmore/%s\",\n  \"target\":{\"host\":",RM_VERSION)<0)ok=-1;
 if(!ok&&rm_json_escape(f,cat->target.host))ok=-1;
 if(!ok&&fputs(",\"seed_url\":",f)==EOF)ok=-1;
 if(!ok&&rm_json_escape(f,cat->target.url))ok=-1;
 if(!ok&&fprintf(f,"},\n  \"document_count\":%zu,\n  \"operation_count\":%zu,\n  \"documents\":[\n",cat->document_count,cat->operation_count)<0)ok=-1;
 for(size_t i=0;!ok&&i<cat->document_count;i++){
   const rm_document *d=&cat->documents[i];
   if(i&&fputs(",\n",f)==EOF)ok=-1;
   if(!ok&&fputs("    {\"url\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,d->url))ok=-1;
   if(!ok&&fputs(",\"sha256\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,d->sha256))ok=-1;
   if(!ok&&fputs(",\"content_type\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,d->content_type))ok=-1;
   if(!ok&&fputs(",\"etag\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,d->etag))ok=-1;
   if(!ok&&fputs(",\"last_modified\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,d->last_modified))ok=-1;
   if(!ok&&fprintf(f,",\"bytes\":%zu,\"depth\":%u,\"status\":%d,\"response_status\":%d}",d->bytes,d->depth,d->status,d->response_status?d->response_status:d->status)<0)ok=-1;
 }
 if(!ok&&fputs("\n  ],\n  \"operations\":[\n",f)==EOF)ok=-1;
 for(size_t i=0;!ok&&i<cat->operation_count;i++){
   const rm_operation *op=&cat->operations[i];
   if(i&&fputs(",\n",f)==EOF)ok=-1;
   if(!ok&&fputs("    {\"method\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,op->method))ok=-1;
   if(!ok&&fputs(",\"path\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,op->path))ok=-1;
   if(!ok&&fputs(",\"summary\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,op->summary))ok=-1;
   if(!ok&&fputs(",\"operation_id\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,op->operation_id))ok=-1;
   if(!ok&&fputs(",\"source_url\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,op->source_url))ok=-1;
   if(!ok&&fputs(",\"source_sha256\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,op->source_sha256))ok=-1;
   if(!ok&&fprintf(f,",\"expected_status\":%d,\"expected_content_type\":",op->expected_status)<0)ok=-1;
   if(!ok&&rm_json_escape(f,op->expected_content_type))ok=-1;
   if(!ok&&fputs(",\"expected_json_type\":",f)==EOF)ok=-1;
   if(!ok&&rm_json_escape(f,op->expected_json_type))ok=-1;
   if(!ok&&fputc('}',f)==EOF)ok=-1;
 }
 if(!ok&&fputs("\n  ]\n}\n",f)==EOF)ok=-1;
 if(fclose(f))ok=-1;
 if(ok){remove(tmp);return rm_error(error,cap,"cannot write catalog");}
#ifdef _WIN32
 remove(file);
#endif
 if(rename(tmp,file)){remove(tmp);return rm_error(error,cap,"cannot install catalog");}
 if(rm_export_postman(cat,dir,error,cap)||rm_export_mermaid(cat,dir,error,cap)||
    rm_export_svg(cat,dir,error,cap)||rm_export_openapi(cat,dir,error,cap))return -1;
 if(error&&cap)error[0]=0;
 return 0;
}
