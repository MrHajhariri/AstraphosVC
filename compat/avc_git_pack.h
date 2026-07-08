#ifndef AVC_GIT_PACK_H
#define AVC_GIT_PACK_H

#include <stddef.h>
#include <stdint.h>

#include "objects/avc_oid.h"
#include "objects/avc_object.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum avc_git_pack_object_type {
    AVC_GIT_OBJ_NONE = 0,
    AVC_GIT_OBJ_COMMIT = 1,
    AVC_GIT_OBJ_TREE = 2,
    AVC_GIT_OBJ_BLOB = 3,
    AVC_GIT_OBJ_TAG = 4,
    AVC_GIT_OBJ_OFS_DELTA = 6,
    AVC_GIT_OBJ_REF_DELTA = 7
} avc_git_pack_object_type;

typedef struct avc_git_pack_entry {
    avc_oid oid;
    uint64_t offset;
    uint32_t crc32;
} avc_git_pack_entry;

typedef struct avc_git_pack {
    char *pack_path;
    char *idx_path;
    int fd_pack;
    int fd_idx;
    uint32_t object_count;
    avc_git_pack_entry *entries;
    unsigned char *pack_data;
    size_t pack_size;
} avc_git_pack;

avc_status avc_git_pack_open(avc_git_pack *pack, const char *pack_path,
                             avc_error *error);

void avc_git_pack_close(avc_git_pack *pack);

avc_status avc_git_pack_read_object(const avc_git_pack *pack,
                                    uint32_t index,
                                    avc_git_pack_object_type *type,
                                    void **payload, size_t *payload_size,
                                    avc_error *error);

avc_status avc_git_pack_find_object(const avc_git_pack *pack,
                                    const avc_oid oid,
                                    uint32_t *index, avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
