#ifndef AVC_OBJECT_H
#define AVC_OBJECT_H

#include <stddef.h>

#include "objects/avc_oid.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum avc_object_type {
    AVC_OBJECT_BLOB = 0,
    AVC_OBJECT_TREE,
    AVC_OBJECT_COMMIT,
    AVC_OBJECT_TAG
} avc_object_type;

const char *avc_object_type_name(avc_object_type type);
int avc_object_type_from_name(const char *name, avc_object_type *out);

typedef struct avc_odb {
    char *objects_path;
    int fd_lock;
} avc_odb;

void avc_odb_init(avc_odb *odb);
void avc_odb_close(avc_odb *odb);

avc_status avc_odb_open(avc_odb *odb, const char *objects_path, avc_error *error);

avc_status avc_odb_write(avc_odb *odb, avc_object_type type,
                         const void *payload, size_t payload_size,
                         avc_oid out, avc_error *error);

avc_status avc_odb_read(avc_odb *odb, const avc_oid oid,
                        avc_object_type *type,
                        void **payload, size_t *payload_size,
                        avc_error *error);

int avc_odb_exists(avc_odb *odb, const avc_oid oid);

avc_status avc_odb_write_blob(avc_odb *odb, const void *data, size_t size,
                              avc_oid out, avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
