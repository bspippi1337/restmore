#include "internal.h"
#include <string.h>
int rm_crawl(const rm_options *options,rm_catalog *catalog,rm_progress progress,
             void *context,char *error,size_t error_cap){
 if(!options||!options->input||!catalog)return rm_error(error,error_cap,"configuration required");
 memset(catalog,0,sizeof(*catalog));
 if(rm_normalize(options->input,&catalog->target,error,error_cap))return -1;
 if(options->execute)return rm_error(error,error_cap,"HTTPS crawler not built: configure RESTMORE_NETWORK=ON with libcurl");
 if(progress)progress("preview",catalog->target.url,context);
 return 0;
}

int rm_verify_run(rm_verification *plan,rm_progress progress,void *context,char *error,size_t cap){
 (void)progress;(void)context;
 if(!plan||!plan->authorized)return rm_error(error,cap,"Explicit Authorize required for this domain");
 return rm_error(error,cap,"Verification HTTPS disabled: build with RESTMORE_NETWORK=ON");
}
