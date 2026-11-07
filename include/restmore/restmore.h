#ifndef RESTMORE_H
#define RESTMORE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RM_VERSION "0.1.0"
#define RM_MAX_DOCUMENTS 25
#define RM_MAX_OPERATIONS 2048
#define RM_MAX_URL 2048
#define RM_MAX_PATH 1024
#define RM_MAX_DOCUMENT_BYTES (2u * 1024u * 1024u)
typedef struct { char host[254]; char url[RM_MAX_URL]; } rm_target;
typedef struct {
  char url[RM_MAX_URL];
  char sha256[65];
  char content_type[96];
  size_t bytes;
  unsigned depth;
  int status;
} rm_document;
typedef struct {
  char method[8];
  char path[RM_MAX_PATH];
  char summary[256];
  char operation_id[128];
  char source_url[RM_MAX_URL];
  char source_sha256[65];
} rm_operation;
typedef struct {
  rm_target target;
  size_t document_count, operation_count;
  rm_document documents[RM_MAX_DOCUMENTS];
  rm_operation operations[RM_MAX_OPERATIONS];
} rm_catalog;
typedef struct {
  const char *input;
  const char *output_dir;
  const char *user_agent;
  unsigned max_pages; /* 1..25; defaults to 12 */
  unsigned max_depth; /* 0..3 */
  unsigned delay_ms; /* at least 2000, higher robots.txt delay wins */
  int execute; /* preview by default */
  int authorized; /* required independently of execute */
} rm_options;
typedef void (*rm_progress)(const char *event, const char *detail, void *context);
int rm_normalize(const char *input, rm_target *out, char *error, size_t error_cap);
void rm_sha256(const void *input, size_t length, char hex[65]);
int rm_openapi_import(rm_catalog *catalog, const char *source_url, const char *sha256,
                      const char *json, size_t length, char *error, size_t error_cap);
int rm_catalog_import_json(rm_catalog *catalog,const char *json,size_t length,char *error,size_t error_cap);
int rm_catalog_save(const rm_catalog *catalog, const char *directory, char *error, size_t error_cap);
int rm_crawl(const rm_options *options, rm_catalog *catalog, rm_progress progress,
             void *context, char *error, size_t error_cap);
int rm_make_dir(const char *path);
#ifdef __cplusplus
}
#endif
#endif
