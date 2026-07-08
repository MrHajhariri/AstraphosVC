#include "repository/avc_repository.h"

#include "config/avc_config.h"
#include "utils/avc_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_string(const char *value) {
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);
    if (copy != NULL) {
        memcpy(copy, value, length);
    }
    return copy;
}

static avc_status ensure_dir_joined(const char *base, const char *relative, avc_error *error) {
    char *path = avc_fs_join(base, relative);
    if (path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory creating repository path");
        return AVC_ERR_NO_MEMORY;
    }
    avc_status status = avc_fs_mkdir_p(path, error);
    free(path);
    return status;
}

static avc_status write_text_joined(const char *base, const char *relative, const char *text, avc_error *error) {
    char *path = avc_fs_join(base, relative);
    if (path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory creating repository file path");
        return AVC_ERR_NO_MEMORY;
    }
    avc_status status = avc_fs_write_file(path, text, strlen(text), error);
    free(path);
    return status;
}

void avc_repository_free(avc_repository *repository) {
    if (repository == NULL) {
        return;
    }
    free(repository->worktree_path);
    free(repository->metadata_path);
    free(repository->objects_path);
    repository->worktree_path = NULL;
    repository->metadata_path = NULL;
    repository->objects_path = NULL;
}

avc_status avc_repository_init(const char *worktree_path, const char *initial_branch, avc_repository *repository, avc_error *error) {
    if (worktree_path == NULL || repository == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "repository init requires a worktree path");
        return AVC_ERR_INVALID_ARGUMENT;
    }
    const char *branch = initial_branch == NULL || initial_branch[0] == '\0' ? AVC_DEFAULT_BRANCH : initial_branch;

    avc_status status = avc_fs_mkdir_p(worktree_path, error);
    if (status != AVC_OK) {
        return status;
    }

    char *metadata = avc_fs_join(worktree_path, AVC_REPOSITORY_DIR);
    if (metadata == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory creating metadata path");
        return AVC_ERR_NO_MEMORY;
    }

    const char *dirs[] = {
        "objects", "objects/tmp",
        "refs", "refs/heads", "refs/tags", "logs"
    };
    status = avc_fs_mkdir_p(metadata, error);
    for (size_t i = 0; status == AVC_OK && i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        status = ensure_dir_joined(metadata, dirs[i], error);
    }
    if (status != AVC_OK) {
        free(metadata);
        return status;
    }

    char head[256];
    snprintf(head, sizeof(head), "ref: refs/heads/%s\n", branch);
    status = write_text_joined(metadata, "HEAD", head, error);
    if (status != AVC_OK) {
        free(metadata);
        return status;
    }

    avc_config config;
    avc_config_init(&config);
    status = avc_config_set(&config, "core", "repositoryformatversion", "1", error);
    if (status == AVC_OK) {
        status = avc_config_set(&config, "core", "filemode", "true", error);
    }
    if (status == AVC_OK) {
        status = avc_config_set(&config, "core", "bare", "false", error);
    }
    if (status == AVC_OK) {
        status = avc_config_set(&config, "core", "primarymetadata", ".avc", error);
    }
    if (status == AVC_OK) {
        char *config_path = avc_fs_join(metadata, "config");
        if (config_path == NULL) {
            status = AVC_ERR_NO_MEMORY;
            avc_error_set(error, status, "out of memory creating config path");
        } else {
            status = avc_config_write(&config, config_path, error);
            free(config_path);
        }
    }
    avc_config_free(&config);
    if (status != AVC_OK) {
        free(metadata);
        return status;
    }

    repository->worktree_path = dup_string(worktree_path);
    repository->metadata_path = metadata;
    repository->objects_path = avc_fs_join(metadata, "objects");
    if (repository->worktree_path == NULL || repository->objects_path == NULL) {
        avc_repository_free(repository);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory storing repository paths");
        return AVC_ERR_NO_MEMORY;
    }
    return AVC_OK;
}

avc_status avc_repository_open(const char *worktree_path, avc_repository *repository, avc_error *error) {
    if (worktree_path == NULL || repository == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "repository open requires a worktree path");
        return AVC_ERR_INVALID_ARGUMENT;
    }
    char *metadata = avc_fs_join(worktree_path, AVC_REPOSITORY_DIR);
    if (metadata == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory creating metadata path");
        return AVC_ERR_NO_MEMORY;
    }
    if (!avc_fs_is_directory(metadata)) {
        avc_error_setf(error, AVC_ERR_NOT_FOUND, "not an AstraphosVC repository: %s", worktree_path);
        free(metadata);
        return AVC_ERR_NOT_FOUND;
    }

    char *config_path = avc_fs_join(metadata, "config");
    if (config_path == NULL) {
        free(metadata);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory creating config path");
        return AVC_ERR_NO_MEMORY;
    }
    if (!avc_fs_is_regular_file(config_path)) {
        avc_error_set(error, AVC_ERR_NOT_FOUND, "repository config is missing");
        free(config_path);
        free(metadata);
        return AVC_ERR_NOT_FOUND;
    }
    free(config_path);

    repository->worktree_path = dup_string(worktree_path);
    repository->metadata_path = metadata;
    repository->objects_path = avc_fs_join(metadata, "objects");
    if (repository->worktree_path == NULL || repository->objects_path == NULL) {
        avc_repository_free(repository);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory opening repository");
        return AVC_ERR_NO_MEMORY;
    }
    return AVC_OK;
}

avc_status avc_repository_discover(const char *start_path, avc_repository *repository, avc_error *error) {
    if (repository == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "repository discover requires output storage");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *current = start_path == NULL ? avc_fs_current_directory(error) : dup_string(start_path);
    if (current == NULL) {
        return error != NULL ? error->code : AVC_ERR_NO_MEMORY;
    }

    while (current != NULL) {
        char *metadata = avc_fs_join(current, AVC_REPOSITORY_DIR);
        if (metadata == NULL) {
            free(current);
            avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory discovering repository");
            return AVC_ERR_NO_MEMORY;
        }
        if (avc_fs_is_directory(metadata)) {
            free(metadata);
            avc_status status = avc_repository_open(current, repository, error);
            free(current);
            return status;
        }
        free(metadata);

        char *parent = avc_fs_parent(current);
        if (parent == NULL || strcmp(parent, current) == 0) {
            free(parent);
            break;
        }
        free(current);
        current = parent;
    }

    free(current);
    avc_error_set(error, AVC_ERR_NOT_FOUND, "no AstraphosVC repository found");
    return AVC_ERR_NOT_FOUND;
}
