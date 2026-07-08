#ifndef AVC_REPOSITORY_H
#define AVC_REPOSITORY_H

#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AVC_REPOSITORY_FORMAT_VERSION 1
#define AVC_DEFAULT_BRANCH "main"

typedef struct avc_repository {
    char *worktree_path;
    char *metadata_path;
} avc_repository;

void avc_repository_free(avc_repository *repository);
avc_status avc_repository_init(const char *worktree_path, const char *initial_branch, avc_repository *repository, avc_error *error);
avc_status avc_repository_open(const char *worktree_path, avc_repository *repository, avc_error *error);
avc_status avc_repository_discover(const char *start_path, avc_repository *repository, avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
