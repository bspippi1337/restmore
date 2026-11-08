#ifndef RESTMORE_H
#define RESTMORE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RM_VERSION "0.1.0"
#define RM_MAX_DOCUMENTS 256
#define RM_MAX_OPERATIONS 4096
#define RM_MAX_URL 2048
#define RM_MAX_PATH 1024
#define RM_MAX_DOCUMENT_BYTES (2u * 1024u * 1024u)
#define RM_MAX_VERIFY_CHECKS RM_MAX_OPERATIONS /* full documented atlas; user chooses coverage */
typedef struct { char host[254]; char url[RM_MAX_URL]; } rm_target;
typedef struct {
  char url[RM_MAX_URL];
  char sha256[65];
  char content_type[96];
  size_t bytes;
  unsigned depth;
  int status;
  int response_status; /* HTTP 200 or cache revalidation 304; zero means status */
  char etag[256], last_modified[96]; /* bounded conditional headers */
} rm_document;
typedef struct {
  char method[8];
  char path[RM_MAX_PATH];
  char summary[256];
  char operation_id[128];
  char source_url[RM_MAX_URL];
  char source_sha256[65];
  int expected_status; /* OpenAPI 200/204 response; 0 means no testable response contract */
  char expected_content_type[96]; /* optional declared response MIME */
  char expected_json_type[16]; /* shallow OpenAPI schema root type, if declared */
} rm_operation;
typedef struct {
  rm_target target;
  size_t document_count, operation_count;
  rm_document documents[RM_MAX_DOCUMENTS];
  rm_operation operations[RM_MAX_OPERATIONS];
} rm_catalog;
typedef struct {
  char method[8],path[RM_MAX_PATH],source_sha256[65];
  int expected_status,actual_status;
  char expected_content_type[96],observed_content_type[96];
  char expected_json_type[16];
  int outcome; /* 0 pending, 1 verified status+type+JSON, 2 status-only,
                  3 mismatch, 4 transport/error, 5 skipped */
} rm_check;
typedef struct {
  rm_target target;
  size_t count;
  int authorized; /* set ONLY after explicit human action for exactly this target */
  int github_reviewed; /* set ONLY when user confirms manually inspecting GitHub link */
  int source_bound; /* imported catalog matched this domain */
  rm_check checks[RM_MAX_VERIFY_CHECKS];
} rm_verification;
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

/* RESTLESS Botanical <-> RESTMORE native C11 source interchange.
 * All operations below are OFFLINE; only SHA-256-verified archived OpenAPI
 * documents may contribute API operations. Nothing is guessed or executed. */
typedef struct {
 size_t manifest_records, revalidated, documents, openapi_sources, operations;
} rm_fusion_stats;
typedef struct { size_t operation_index; unsigned score; } rm_fusion_hit;
typedef struct { size_t added, removed, modified, unchanged; } rm_fusion_diff_report;
int rm_restless_import(const char *archive_directory,rm_catalog *catalog,
                       rm_fusion_stats *stats,char *error,size_t error_cap);
/* Append interoperable JSONL record after source bytes exist on disk. */
int rm_fusion_append_manifest(const char *directory,const rm_document *doc,
                              char *error,size_t error_cap);
/* Load a SHA-256-verified cached source by exact URL; caller frees *body.
 * Return 1 when cached, 0 when absent, -1 on corruption / malformed cache.
 * Never does a network request. */
int rm_fusion_cache_get(const char *directory,const char *url,rm_document *record,
                        char **body,size_t *length,char *error,size_t error_cap);
size_t rm_fusion_search(const rm_catalog *catalog,const char *query,
                        rm_fusion_hit *hits,size_t capacity);
int rm_fusion_diff(const rm_catalog *before,const rm_catalog *after,
                   rm_fusion_diff_report *result);

int rm_export_postman(const rm_catalog *catalog,const char *directory,char *error,size_t error_cap);
int rm_export_mermaid(const rm_catalog *catalog,const char *directory,char *error,size_t error_cap);
int rm_export_svg(const rm_catalog *catalog,const char *directory,char *error,size_t error_cap);
int rm_export_openapi(const rm_catalog *catalog,const char *directory,char *error,size_t error_cap);
int rm_catalog_save(const rm_catalog *catalog, const char *directory, char *error, size_t error_cap);
int rm_crawl(const rm_options *options, rm_catalog *catalog, rm_progress progress,
             void *context, char *error, size_t error_cap);
int rm_make_dir(const char *path);
/* Offline-only plan: formats GitHub repo SEARCH, never assumes a repo belongs to a host. */
int rm_github_lookup(const char *domain,char *url,size_t cap,char *error,size_t error_cap);
int rm_verify_plan(const rm_catalog *catalog,const char *domain,rm_verification *plan,
                   char *error,size_t error_cap);
int rm_verify_plan_limit(const rm_catalog *catalog,const char *domain,
                         rm_verification *plan,size_t max_checks,
                         char *error,size_t error_cap);
/* max_checks=0 means all eligible documented GET operations in this catalog.
   The caller must still display the plan and require separate approval. */

int rm_verify_run(rm_verification *plan,rm_progress progress,void *context,
                  char *error,size_t error_cap);
/* Save exact-plan results and outcomes; never exports bodies or authentication. */
int rm_verify_report(const rm_verification *plan,const char *path,
                     char *error,size_t error_cap);
/* Strict syntax validation only; does not implement JSON Schema. */
int rm_json_valid(const char *text,size_t length);
int rm_json_root_is(const char *text,size_t length,const char *expected_type);
#ifdef __cplusplus
}
#endif
#endif
