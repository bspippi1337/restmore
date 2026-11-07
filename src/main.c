#include "restmore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc<2||!strcmp(argv[1],"--help")){
  puts("RESTMORE v0.1 | restmore https://host [--max 16] [--out atlas.json]");
  return argc<2?2:0;
 }
 const char *out="atlas.json";unsigned max=16;
 for(int i=2;i<argc;i++){
  if(!strcmp(argv[i],"--out")&&i+1<argc)out=argv[++i];
  else if(!strcmp(argv[i],"--max")&&i+1<argc){char *end=NULL;unsigned long n=strtoul(argv[++i],&end,10);if(!end||*end||n>64||n<1){fputs("Invalid --max\n",stderr);return 2;}max=(unsigned)n;}
  else{fprintf(stderr,"Unknown argument: %s\n",argv[i]);return 2;}
 }
 rm_atlas atlas;
 if(rm_scan(argv[1],max,&atlas)){fprintf(stderr,"Scan error: %s\n",atlas.error);return 1;}
 for(size_t i=0;i<atlas.count;i++)printf("%-5s %3u %s\n",atlas.endpoints[i].method,atlas.endpoints[i].status,atlas.endpoints[i].path);
 if(rm_export(&atlas,out)){perror(out);return 1;}
 printf("Mapped %zu paths with %u requests; wrote %s\n",atlas.count,atlas.requests,out);
 return 0;
}
