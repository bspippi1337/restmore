#ifndef RESTMORE_H
#define RESTMORE_H
#include <stddef.h>
#define RM_MAX_ENDPOINTS 48
#define RM_PATH_SIZE 256
typedef struct { char path[RM_PATH_SIZE]; unsigned status; char method[12]; } rm_endpoint;
typedef struct { char origin[512]; rm_endpoint endpoints[RM_MAX_ENDPOINTS]; size_t count; unsigned requests; char error[256]; } rm_atlas;
int rm_scan(const char *url, unsigned max_requests, rm_atlas *out);
int rm_export(const rm_atlas *atlas, const char *filename);
int rm_add(rm_atlas *atlas, const char *path, const char *method);
#endif
