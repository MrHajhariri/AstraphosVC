#ifndef AVC_COMMIT_H
#define AVC_COMMIT_H

#include <time.h>

#include "index/avc_index.h"
#include "objects/avc_oid.h"
#include "objects/avc_object.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avc_signature {
    char name[128];
    char email[256];
    time_t timestamp;
    int tz_offset;
} avc_signature;

avc_status avc_commit_build_tree(avc_odb *odb, avc_index *index,
                                 avc_oid tree_oid, avc_error *error);

avc_status avc_commit_create(avc_odb *odb, const avc_oid tree_oid,
                             const avc_oid *parent_oids, int parent_count,
                             const avc_signature *author,
                             const avc_signature *committer,
                             const char *message, avc_oid out,
                             avc_error *error);

avc_status avc_commit_parse(const unsigned char *payload, size_t size,
                            avc_oid *tree_oid,
                            avc_oid *parent_oids, int *parent_count,
                            char *author, size_t author_size,
                            char *committer, size_t committer_size,
                            char *message, size_t message_size,
                            avc_error *error);

avc_status avc_commit_log(avc_odb *odb, const avc_oid head_oid,
                          int max_count, avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
