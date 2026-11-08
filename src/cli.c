#include "restmore/restmore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void usage(FILE *f){
 fprintf(f,
 "RESTMORE %s | portable C11 API documentation atlas\n"
 "Usage:\n"
 "  restmore-cli [--out DIR] DOMAIN_OR_HTTPS_DOC_URL\n"
 "  restmore-cli [--out DIR] DOMAIN_OR_URL --execute --authorized\n"
 "  restmore-cli [--out DIR] --import OPENAPI_3_JSON\n" "  restmore-cli --fusion-import RESTLESS_ARCHIVE [--out DIR]\n"
 "  restmore-cli --fusion-import RESTLESS_ARCHIVE --fusion-search QUERY\n"
 "  restmore-cli --fusion-import BEFORE --fusion-compare AFTER\n"
 "Options: --max-pages 1..25 --max-depth 0..3 --delay-ms >=2000\n"
 "         --user-agent TEXT --out DIR --execute --authorized\n"
 "No network access without BOTH execution and authorization.\n"
 "Only linked documentation is fetched; documented API calls are NEVER executed.\n" "All fusion-import/search/diff commands are OFFLINE and verify SHA-256 evidence.\n",RM_VERSION);
}
static void progress(const char *event,const char *detail,void *ctx){
 (void)ctx;fprintf(stderr,"[%s] %s\n",event,detail);
}
static int import_file(const char *path,const char *output){
 FILE *f=fopen(path,"rb");if(!f){perror(path);return 1;}
 if(fseek(f,0,SEEK_END)){fclose(f);return 1;}
 long n=ftell(f);
 if(n<0||(unsigned long)n>RM_MAX_DOCUMENT_BYTES){fclose(f);fputs("OpenAPI JSON exceeds 2 MiB\n",stderr);return 1;}
 rewind(f);
 char *text=(char *)malloc((size_t)n+1);
 rm_catalog *cat=(rm_catalog *)calloc(1,sizeof(*cat));
 if(!text||!cat){free(text);free(cat);fclose(f);return 1;}
 int ok=fread(text,1,(size_t)n,f)==(size_t)n;fclose(f);
 text[n]=0;
 if(!ok){free(text);free(cat);fputs("read failed\n",stderr);return 1;}
 char digest[65],err[256]={0};rm_sha256(text,(size_t)n,digest);
 int added=rm_openapi_import(cat,path,digest,text,(size_t)n,err,sizeof(err));
 if(added<0){fprintf(stderr,"Import refused: %s\n",err);free(text);free(cat);return 1;}
 rm_document *doc=&cat->documents[cat->document_count++];
 snprintf(doc->url,sizeof(doc->url),"%s",path);
 snprintf(doc->sha256,sizeof(doc->sha256),"%s",digest);
 snprintf(doc->content_type,sizeof(doc->content_type),"application/json");
 doc->bytes=(size_t)n;doc->status=200;
 if(rm_make_dir(output)||rm_catalog_save(cat,output,err,sizeof(err))){
  fprintf(stderr,"Write failed: %s\n",err);free(text);free(cat);return 1;
 }
 printf("Imported %d documented API operations from local OpenAPI 3 JSON.\n",added);
 printf("Catalog: %s/library.json\nSHA-256: %s\n",output,digest);
 free(text);free(cat);return 0;
}

static int fusion_run(const char *archive,const char *out,
                      const char *query,const char *after){
 char error[256]={0};
 rm_catalog *catalog=(rm_catalog *)calloc(1,sizeof(*catalog));
 if(!catalog){fputs("Cannot allocate fusion catalog\n",stderr);return 1;}
 rm_fusion_stats stats={0};
 if(rm_restless_import(archive,catalog,&stats,error,sizeof(error))){
  fprintf(stderr,"RESTMORE refused RESTLESS archive: %s\n",error);
  free(catalog);return 1;
 }
 fprintf(stdout,"RESTLESS Botanical evidence verified: %zu records (%zu revalidations),"
         " %zu unique documents, %zu OpenAPI sources, %zu operations.\n",
         stats.manifest_records,stats.revalidated,stats.documents,
         stats.openapi_sources,stats.operations);
 fprintf(stdout,"Catalog API host: %s\n",catalog->target.host);
 if(after){
  rm_catalog *second=(rm_catalog *)calloc(1,sizeof(*second));
  if(!second){free(catalog);return 1;}
  if(rm_restless_import(after,second,NULL,error,sizeof(error))){
   fprintf(stderr,"RESTMORE refused comparison archive: %s\n",error);
   free(second);free(catalog);return 1;
  }
  rm_fusion_diff_report d={0};
  if(rm_fusion_diff(catalog,second,&d)){
   fputs("Cannot compute catalog diff\n",stderr);
   free(second);free(catalog);return 1;
  }
  printf("Contract delta: +%zu added / -%zu removed / %zu changed /"
         " %zu unchanged operations.\n",d.added,d.removed,d.modified,d.unchanged);
  free(second);
 }
 if(query){
  rm_fusion_hit hits[20]={0};
  size_t found=rm_fusion_search(catalog,query,hits,20);
  printf("Source-backed search \"%s\": %zu matches shown\n",query,found);
  for(size_t i=0;i<found;i++){
   const rm_operation *op=&catalog->operations[hits[i].operation_index];
   printf("  [%u] %s %s  %s\n       SOURCE: %s\n       SHA256: %s\n",
          hits[i].score,op->method,op->path,op->summary,
          op->source_url,op->source_sha256);
  }
 }
 if(!out||!*out||rm_make_dir(out)||rm_catalog_save(catalog,out,error,sizeof(error))){
  fprintf(stderr,"RESTMORE fusion output failed: %s\n",error[0]?error:"output directory not writable");
  free(catalog);return 1;
 }
 printf("RESTMORE atlas: %s/library.json\n",out);
 free(catalog);return 0;
}
int main(int argc,char **argv){
 rm_options opt={0};opt.output_dir=".restmore";opt.max_pages=12;opt.max_depth=2;opt.delay_ms=2000;
 const char *file=NULL,*domain=NULL,*fusion_dir=NULL,*fusion_search=NULL,*fusion_compare=NULL;
 for(int i=1;i<argc;i++){
  const char *arg=argv[i];
  if(!strcmp(arg,"--help")||!strcmp(arg,"-h")){usage(stdout);return 0;}
  if(!strcmp(arg,"--version")){printf("restmore %s\n",RM_VERSION);return 0;}
  if(!strcmp(arg,"--execute")){opt.execute=1;continue;}
  if(!strcmp(arg,"--authorized")){opt.authorized=1;continue;}
  if(!strcmp(arg,"--fusion-import")||!strcmp(arg,"--fusion-search")||
     !strcmp(arg,"--fusion-compare")||!strcmp(arg,"--import")||
     !strcmp(arg,"--out")||!strcmp(arg,"--user-agent")||
     !strcmp(arg,"--max-pages")||!strcmp(arg,"--max-depth")||!strcmp(arg,"--delay-ms")){
   if(++i>=argc){fprintf(stderr,"Missing value for %s\n",arg);return 2;}
   const char *val=argv[i];
   if(!strcmp(arg,"--import"))file=val;
   else if(!strcmp(arg,"--fusion-import"))fusion_dir=val;
   else if(!strcmp(arg,"--fusion-search"))fusion_search=val;
   else if(!strcmp(arg,"--fusion-compare"))fusion_compare=val;
   else if(!strcmp(arg,"--out"))opt.output_dir=val;
   else if(!strcmp(arg,"--user-agent"))opt.user_agent=val;
   else {
    char *end=NULL;unsigned long num=strtoul(val,&end,10);
    if(!*val||*end||num>60000){fprintf(stderr,"Invalid limit: %s\n",val);return 2;}
    if(!strcmp(arg,"--max-pages"))opt.max_pages=(unsigned)num;
    else if(!strcmp(arg,"--max-depth"))opt.max_depth=(unsigned)num;
    else opt.delay_ms=(unsigned)num;
   }
   continue;
  }
  if(arg[0]=='-'){fprintf(stderr,"Unknown argument: %s\n",arg);return 2;}
  if(domain){fputs("Only one target permitted\n",stderr);return 2;}
  domain=arg;
 }
 if(fusion_dir){
  if(file||domain||opt.execute||opt.authorized){
   fputs("Fusion import is offline; do not combine it with target/domain, --import, --execute or --authorized\n",stderr);
   return 2;
  }
  return fusion_run(fusion_dir,opt.output_dir,fusion_search,fusion_compare);
 }
 if(fusion_search||fusion_compare){
  fputs("--fusion-search and --fusion-compare require --fusion-import DIR\n",stderr);return 2;
 }
 if(file){if(domain||opt.execute){fputs("--import is offline and takes no domain or --execute\n",stderr);return 2;}return import_file(file,opt.output_dir);}
 if(!domain){usage(stderr);return 2;}
 opt.input=domain;
 rm_catalog *cat=(rm_catalog *)calloc(1,sizeof(*cat));if(!cat)return 1;
 char error[256]={0};
 int status=rm_crawl(&opt,cat,progress,NULL,error,sizeof(error));
 if(status)fprintf(stderr,"RESTMORE refused: %s\n",error);
 else if(!opt.execute)printf("Preview: %s\nHost: %s\nZero network requests. Pass --execute --authorized only with permission.\n",
                           cat->target.url,cat->target.host);
 else printf("Archived %zu documents; indexed %zu documented operations.\n",
             cat->document_count,cat->operation_count);
 free(cat);
 return status?1:0;
}
