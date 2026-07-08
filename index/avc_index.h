#ifndef AVC_INDEX_H
#define AVC_INDEX_H

#include <stddef.h>
#include <stdint.h>

#include "objects/avc_oid.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AVC_INDEX_SIGNATURE 0x44495243U

typedef struct avc_index_entry {
    avc_oid oid;
    uint32_t mode;
    uint32_t dev;
    uint32_t ino;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t ctime_sec;
    uint32_t mtime_sec;
    int flags_valid;
    char *path;
} avc_index_entry;

typedef struct avc_index {
    avc_index_entry *entries;
    size_t count;
    size_t capacity;
    int modified;
} avc_index;

void avc_index_init(avc_index *index);
void avc_index_free(avc_index *index);

avc_status avc_index_load(avc_index *index, const char *path, avc_error *error);
avc_status avc_index_write(avc_index *index, const char *path, avc_error *error);

avc_status avc_index_add(avc_index *index, const char *path,
                         uint32_t mode, const avc_oid oid,
                         avc_error *error);

avc_status avc_index_remove(avc_index *index, const char *path,
                            avc_error *error);

avc_index_entry *avc_index_find(const avc_index *index, const char *path);

#ifdef __cplusplus
}
#endif

#endif
