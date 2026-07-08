#ifndef AVC_DIFF_H
#define AVC_DIFF_H

#include "objects/avc_oid.h"
#include "objects/avc_object.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum avc_diff_file_status {
    AVC_DIFF_ADDED,
    AVC_DIFF_DELETED,
    AVC_DIFF_MODIFIED,
    AVC_DIFF_UNCHANGED
} avc_diff_file_status;

typedef struct avc_diff_file {
    char *path;
    avc_diff_file_status status;
} avc_diff_file;

char *avc_diff_blobs(const unsigned char *a, size_t a_size,
                     const unsigned char *b, size_t b_size,
                     const char *a_path, const char *b_path);

avc_status avc_diff_trees(avc_odb *odb,
                          const avc_oid a_tree, const avc_oid b_tree,
                          avc_diff_file **files, int *file_count,
                          avc_error *error);

void avc_diff_files_free(avc_diff_file *files, int count);

#ifdef __cplusplus
}
#endif

#endif
