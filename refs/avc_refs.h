#ifndef AVC_REFS_H
#define AVC_REFS_H

#include "objects/avc_oid.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

avc_status avc_refs_read_head(const char *metadata_path, char **ref_or_oid,
                              int *is_symbolic, avc_error *error);

avc_status avc_refs_write_head_ref(const char *metadata_path, const char *ref,
                                   avc_error *error);

avc_status avc_refs_write_head_oid(const char *metadata_path,
                                   const avc_oid oid, avc_error *error);

avc_status avc_refs_read_ref(const char *metadata_path, const char *refname,
                             avc_oid oid, avc_error *error);

avc_status avc_refs_write_ref(const char *metadata_path, const char *refname,
                              const avc_oid oid, avc_error *error);

avc_status avc_refs_resolve_head(const char *metadata_path, avc_oid out,
                                 avc_error *error);

avc_status avc_refs_current_branch(const char *metadata_path, char **branch,
                                   avc_error *error);

avc_status avc_refs_list_branches(const char *metadata_path,
                                  char ***branches, int *count,
                                  avc_error *error);

avc_status avc_refs_delete_ref(const char *metadata_path, const char *refname,
                               avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
