#ifndef AVC_GIT_COMPAT_H
#define AVC_GIT_COMPAT_H

#include <stddef.h>

#include "objects/avc_oid.h"
#include "objects/avc_object.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avc_git_repo {
    char *gitdir_path;
    char *objects_path;
    char *refs_path;
    char *pack_path;
} avc_git_repo;

void avc_git_repo_init(avc_git_repo *repo);
void avc_git_repo_free(avc_git_repo *repo);

int avc_git_is_git_repo(const char *path);

avc_status avc_git_repo_open(const char *gitdir_path, avc_git_repo *repo,
                             avc_error *error);

avc_status avc_git_read_object(const avc_git_repo *repo, const avc_oid oid,
                               avc_object_type *type,
                               void **payload, size_t *payload_size,
                               avc_error *error);

avc_status avc_git_read_ref(const avc_git_repo *repo, const char *refname,
                            avc_oid oid, avc_error *error);

avc_status avc_git_list_refs(const avc_git_repo *repo,
                             char ***refnames, avc_oid *oids, int *count,
                             avc_error *error);

avc_status avc_git_resolve_ref(const avc_git_repo *repo, const char *refname,
                               avc_oid oid, avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
