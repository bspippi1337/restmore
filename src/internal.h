#ifndef RESTMORE_INTERNAL_H
#define RESTMORE_INTERNAL_H
#include "restmore/restmore.h"
#include <stdio.h>
#include <string.h>
static inline int rm_error(char *err,size_t cap,const char *message){
 if(err&&cap)snprintf(err,cap,"%s",message);
 return -1;
}
int rm_json_escape(FILE *file,const char *text);
#endif
