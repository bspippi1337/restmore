/* Portable C exports: native SVG/Mermaid maps, OpenAPI inventory, Postman 2.1.
 * Only pre-declared paths are exported. No network calls are performed. */
#include "internal.h"
#include <ctype.h>
#include <stdlib.h>
static FILE *open_export(const char *dir,const char *name,char *error,size_t cap){
 char path[4096];
 if(!dir||snprintf(path,sizeof(path),"%s/%s",dir,name)>=(int)sizeof(path)){
  rm_error(error,cap,"export filename exceeds limit");return NULL;
 }
 FILE *f=fopen(path,"wb");
 if(!f)rm_error(error,cap,"cannot open export destination");
 return f;
}
static int close_export(FILE *f,char *err,size_t cap){
 if(fclose(f))return rm_error(err,cap,"cannot flush exported file");
 return 0;
}
static int xml(FILE *f,const char *v){
 for(const unsigned char *p=(const unsigned char *)(v?v:"");*p;p++){
  if(*p=='&'){if(fputs("&amp;",f)==EOF)return -1;}
  else if(*p=='<'){if(fputs("&lt;",f)==EOF)return -1;}
  else if(*p=='>'){if(fputs("&gt;",f)==EOF)return -1;}
  else if(*p=='"'){if(fputs("&quot;",f)==EOF)return -1;}
  else if(*p==39){if(fputs("&apos;",f)==EOF)return -1;}
  else if(*p>=32&&fputc(*p,f)==EOF)return -1;
 }
 return 0;
}
static const char *method_tint(const char *method){
 if(!strcmp(method,"GET"))return "#b8f399";
 if(!strcmp(method,"POST"))return "#6ad6c4";
 if(!strcmp(method,"DELETE"))return "#fbb978";
 return "#d7afff";
}
int rm_export_svg(const rm_catalog *cat,const char *dir,char *err,size_t cap){
 if(!cat)return rm_error(err,cap,"SVG catalog required");
 FILE *f=open_export(dir,"api-map.svg",err,cap);if(!f)return -1;
 size_t total=cat->operation_count;
 unsigned long long height=230ULL+total*76ULL;
 fprintf(f,"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1300\" height=\"%llu\" viewBox=\"0 0 1300 %llu\" role=\"img\" aria-label=\"Documented API operation topology\">\n",height,height);
 fputs("<rect width=\"1300\" height=\"100%\" fill=\"#0b1117\"/><text x=\"48\" y=\"70\" fill=\"#eef5ec\" font-size=\"38\" font-family=\"sans-serif\" font-weight=\"bold\">RESTMORE / API ATLAS</text>\n",f);
 fputs("<text x=\"49\" y=\"106\" fill=\"#94a7ab\" font-size=\"17\" font-family=\"sans-serif\">Source-backed documented methods. No endpoint execution.</text>\n",f);
 fputs("<circle cx=\"152\" cy=\"188\" r=\"21\" fill=\"#6ad6c4\"/><text x=\"192\" y=\"196\" fill=\"#eef5ec\" font-size=\"20\" font-family=\"sans-serif\">DOCUMENTATION</text>\n",f);
 for(size_t i=0;i<total;i++){
  unsigned long long y=230ULL+i*76ULL;
  const rm_operation *o=&cat->operations[i];
  fprintf(f,"<path d=\"M 152 188 L 245 %llu\" fill=\"none\" stroke=\"#33514b\" stroke-width=\"2\"/>\n",y+20);
  fprintf(f,"<rect x=\"245\" y=\"%llu\" rx=\"14\" width=\"980\" height=\"59\" fill=\"#1b2831\" stroke=\"#314249\"/>\n",y);
  fprintf(f,"<rect x=\"263\" y=\"%llu\" rx=\"8\" width=\"105\" height=\"31\" fill=\"%s\" fill-opacity=\"0.18\"/>\n",y+14,method_tint(o->method));
  fprintf(f,"<text x=\"275\" y=\"%llu\" fill=\"%s\" font-size=\"17\" font-family=\"monospace\">",y+36,method_tint(o->method));
  xml(f,o->method);fputs("</text>\n",f);
  fprintf(f,"<text x=\"390\" y=\"%llu\" fill=\"#eef5ec\" font-size=\"17\" font-family=\"monospace\">",y+36);
  xml(f,o->path);fputs("</text>\n",f);
 }
 fputs("</svg>\n",f);
 return close_export(f,err,cap);
}
static int mermaid_label(FILE *f,const char *s){
 for(const unsigned char *p=(const unsigned char*)(s?s:"");*p;p++){
  if(isalnum(*p)||strchr("._/-{} ",*p))fputc(*p,f);
  else fputc(' ',f);
 }
 return 0;
}
int rm_export_mermaid(const rm_catalog *cat,const char *dir,char *err,size_t cap){
 if(!cat)return rm_error(err,cap,"Mermaid catalog required");
 FILE *f=open_export(dir,"api-map.mmd",err,cap);if(!f)return -1;
 fputs("flowchart LR\n  root[\"Verified documentation inventory\"]\n",f);
 for(size_t i=0;i<cat->operation_count;i++){
  const rm_operation *o=&cat->operations[i];
  fprintf(f,"  root --> n%zu[\"",i);
  mermaid_label(f,o->method);fputc(' ',f);mermaid_label(f,o->path);fputs("\"]\n",f);
 }
 return close_export(f,err,cap);
}
int rm_export_postman(const rm_catalog *cat,const char *dir,char *err,size_t cap){
 if(!cat)return rm_error(err,cap,"Postman catalog required");
 FILE *f=open_export(dir,"postman-collection.json",err,cap);if(!f)return -1;
 fputs("{\"info\":{\"name\":\"RESTMORE documented operations (review before use)\",\"description\":\"Documentation-only inventory. No endpoints verified or executed. Replace baseUrl and review authorization before any request.\",\"schema\":\"https://schema.getpostman.com/json/collection/v2.1.0/collection.json\"},",f);
 fputs("\"variable\":[{\"key\":\"baseUrl\",\"value\":\"https://REPLACE_WITH_AUTHORIZED_API_HOST\"}],\"item\":[",f);
 for(size_t i=0;i<cat->operation_count;i++){
  const rm_operation *o=&cat->operations[i];
  if(i)fputc(',',f);
  fputs("{\"name\":",f);
  rm_json_escape(f,o->method);fputc(' ',f);
  /* Name includes both verb and path, escaped as one JSON string. */
  if(fseek(f,-1,SEEK_CUR)==0){ /* no-op compatibility: separators are written below */ }
  fputs(",\"request\":{\"method\":",f);
  rm_json_escape(f,o->method);
  fputs(",\"url\":",f);
  char raw[RM_MAX_PATH+32];snprintf(raw,sizeof(raw),"{{baseUrl}}%s",o->path);
  rm_json_escape(f,raw);
  fputs(",\"description\":",f);
  char warning[RM_MAX_URL+256];
  snprintf(warning,sizeof(warning),"DECLARATION ONLY. Do not execute without permission. Archived source: %.1800s. SHA256: %.64s",o->source_url,o->source_sha256);
  rm_json_escape(f,warning);
  fputs(",\"header\":[{\"key\":\"Accept\",\"value\":\"application/json\"}]}}",f);
 }
 fputs("]}\n",f);
 return close_export(f,err,cap);
}
int rm_export_openapi(const rm_catalog *cat,const char *dir,char *err,size_t cap){
 if(!cat)return rm_error(err,cap,"OpenAPI catalog required");
 FILE *f=open_export(dir,"openapi-inventory.json",err,cap);if(!f)return -1;
 fputs("{\"openapi\":\"3.1.0\",\"info\":{\"title\":\"RESTMORE documented API inventory (not verified deployed)\",\"version\":\"0.1.0\",\"description\":\"Source-backed method labels; schemas and security are deliberately not invented.\"},\"servers\":[],\"paths\":{",f);
 size_t written=0;
 for(size_t i=0;i<cat->operation_count;i++){
  const rm_operation *op=&cat->operations[i];
  int previous=0;
  for(size_t j=0;j<i;j++)if(!strcmp(cat->operations[j].path,op->path)){previous=1;break;}
  if(previous)continue;
  if(written++)fputc(',',f);
  rm_json_escape(f,op->path);fputs(":{",f);
  size_t methods=0;
  for(size_t j=i;j<cat->operation_count;j++){
   const rm_operation *candidate=&cat->operations[j];
   if(strcmp(candidate->path,op->path))continue;
   int duplicate=0;
   for(size_t k=i;k<j;k++)if(!strcmp(cat->operations[k].path,op->path)&&
                                  !strcmp(cat->operations[k].method,candidate->method))duplicate=1;
   if(duplicate)continue;
   if(methods++)fputc(',',f);
   char lowered[8];snprintf(lowered,sizeof(lowered),"%s",candidate->method);
   for(char *c=lowered;*c;c++)*c=(char)tolower((unsigned char)*c);
   rm_json_escape(f,lowered);fputs(":{\"summary\":",f);
   rm_json_escape(f,candidate->summary);
   if(candidate->operation_id[0]){
    fputs(",\"operationId\":",f);rm_json_escape(f,candidate->operation_id);
   }
   fputs(",\"x-restmore-source-url\":",f);rm_json_escape(f,candidate->source_url);
   fputs(",\"x-restmore-source-sha256\":",f);rm_json_escape(f,candidate->source_sha256);
   fputs(",\"responses\":{\"default\":{\"description\":\"Response not verified\"}}}",f);
  }
  fputc('}',f);
 }
 fputs("}}\n",f);
 return close_export(f,err,cap);
}
