#include "restmore.h"
#include <assert.h>
#include <string.h>
int main(void){
 rm_atlas a={0};
 assert(rm_add(&a,"/v1/items","GET")==1);
 assert(rm_add(&a,"/v1/items","GET")==0);
 assert(rm_add(&a,"//evil.example/","GET")==0);
 assert(rm_add(&a,"/v1/items?q=1","GET")==0);
 assert(a.count==1);
 assert(strcmp(a.endpoints[0].path,"/v1/items")==0);
 return 0;
}
