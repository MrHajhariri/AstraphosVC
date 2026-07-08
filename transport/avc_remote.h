#ifndef AVC_REMOTE_H
#define AVC_REMOTE_H

#include "objects/avc_oid.h"
#include "objects/avc_object.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avc_remote {
    char *name;
    char *url;
} avc_remote;

avc_status avc_remote_list(const char *metadata_path,
                           avc_remote **remotes, int *count,
                           avc_error *error);
void avc_remote_list_free(avc_remote *remotes, int count);

avc_status avc_remote_add(const char *metadata_path,
                          const char *name, const char *url,
                          avc_error *error);

avc_status avc_remote_remove(const char *metadata_path,
                             const char *name,
                             avc_error *error);

avc_status avc_remote_fetch(avc_odb *local_odb,
                            const char *metadata_path,
                            const char *remote_url,
                            const char *remote_name,
                            avc_error *error);

avc_status avc_remote_push(avc_odb *local_odb,
                           const char *metadata_path,
                           const char *remote_url,
                           const char *remote_name,
                           const char *branch,
                           avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
