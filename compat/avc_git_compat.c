#define _POSIX_C_SOURCE 200809L

#include "compat/avc_git_compat.h"
#include "compat/avc_git_pack.h"

#include "compression/avc_compress.h"
#include "objects/avc_object.h"
#include "utils/avc_fs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void avc_git_repo_init(avc_git_repo *repo) {
    if (repo == NULL) return;
    memset(repo, 0, sizeof(*repo));
}

void avc_git_repo_free(avc_git_repo *repo) {
    if (repo == NULL) return;
    free(repo->gitdir_path);
    free(repo->objects_path);
    free(repo->refs_path);
    free(repo->pack_path);
    avc_git_repo_init(repo);
}

int avc_git_is_git_repo(const char *path) {
    if (path == NULL) return 0;
    char *head_path = avc_fs_join(path, "HEAD");
    if (head_path == NULL) return 0;
    int exists = avc_fs_is_regular_file(head_path);
    free(head_path);
    return exists ? 1 : 0;
}

avc_status avc_git_repo_open(const char *gitdir_path, avc_git_repo *repo,
                             avc_error *error) {
    if (gitdir_path == NULL || repo == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "git_repo_open received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    if (!avc_git_is_git_repo(gitdir_path)) {
        avc_error_setf(error, AVC_ERR_NOT_FOUND,
                       "not a git repository: %s", gitdir_path);
        return AVC_ERR_NOT_FOUND;
    }

    avc_git_repo_free(repo);

    repo->gitdir_path = strdup(gitdir_path);
    if (repo->gitdir_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    repo->objects_path = avc_fs_join(gitdir_path, "objects");
    if (repo->objects_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    repo->refs_path = avc_fs_join(gitdir_path, "refs");
    if (repo->refs_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    repo->pack_path = avc_fs_join(repo->objects_path, "pack");
    if (repo->pack_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    return AVC_OK;
}

avc_status avc_git_read_object(const avc_git_repo *repo, const avc_oid oid,
                               avc_object_type *type,
                               void **payload, size_t *payload_size,
                               avc_error *error) {
    if (repo == NULL || oid == NULL || type == NULL ||
        payload == NULL || payload_size == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "git_read_object received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(oid, hex);

    char dir[3], file[39];
    memcpy(dir, hex, 2);
    dir[2] = '\0';
    memcpy(file, hex + 2, 38);
    file[38] = '\0';

    char *obj_path = avc_fs_join(repo->objects_path, dir);
    if (obj_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }
    char *full_path = avc_fs_join(obj_path, file);
    free(obj_path);
    if (full_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    if (!avc_fs_is_regular_file(full_path)) {
        free(full_path);

        DIR *pack_dir = opendir(repo->pack_path);
        if (pack_dir == NULL) {
            avc_error_set(error, AVC_ERR_NOT_FOUND, "object not found");
            return AVC_ERR_NOT_FOUND;
        }

        struct dirent *entry;
        avc_git_pack pack;
        avc_status status = AVC_ERR_NOT_FOUND;

        while ((entry = readdir(pack_dir)) != NULL) {
            size_t len = strlen(entry->d_name);
            if (len > 5 && strcmp(entry->d_name + len - 5, ".pack") == 0) {
                char *pack_path = avc_fs_join(repo->pack_path, entry->d_name);
                if (pack_path == NULL) continue;

                avc_status open_status = avc_git_pack_open(&pack, pack_path,
                                                            error);
                free(pack_path);
                if (open_status != AVC_OK) continue;

                uint32_t idx;
                if (avc_git_pack_find_object(&pack, oid, &idx, error) == AVC_OK) {
                    avc_git_pack_object_type ptype;
                    status = avc_git_pack_read_object(&pack, idx, &ptype,
                                                       payload, payload_size,
                                                       error);
                    if (status == AVC_OK) {
                        *type = (avc_object_type)ptype;
                    }
                    avc_git_pack_close(&pack);
                    break;
                }
                avc_git_pack_close(&pack);
            }
        }
        closedir(pack_dir);
        return status;
    }
    free(full_path);

    avc_odb odb;
    avc_odb_init(&odb);
    avc_status status = avc_odb_open(&odb, repo->objects_path, error);
    if (status != AVC_OK) return status;

    status = avc_odb_read(&odb, oid, type, payload, payload_size, error);
    avc_odb_close(&odb);
    return status;
}

static int read_packed_refs(const char *packed_refs_path, const char *refname,
                            avc_oid oid) {
    char *content = NULL;
    size_t size = 0;
    avc_error error;
    avc_error_clear(&error);

    if (avc_fs_read_file(packed_refs_path, &content, &size, &error) != AVC_OK) {
        return -1;
    }

    char *line = content;
    char *end = content + size;
    int result = -1;

    while (line < end) {
        char *nl = memchr(line, '\n', (size_t)(end - line));
        if (nl == NULL) nl = end;

        size_t line_len = (size_t)(nl - line);
        if (line_len >= 41 && line[40] == ' ') {
            size_t refname_len = line_len - 41;
            if (refname_len > 1 && line[41] == '^') {
                line = nl + 1;
                continue;
            }

            if (refname_len == strlen(refname) &&
                memcmp(line + 41, refname, refname_len) == 0) {
                char hex[41];
                memcpy(hex, line, 40);
                hex[40] = '\0';
                if (avc_oid_parse(hex, oid) == 0) {
                    result = 0;
                }
                break;
            }
        }

        line = nl + 1;
    }

    free(content);
    return result;
}

avc_status avc_git_read_ref(const avc_git_repo *repo, const char *refname,
                            avc_oid oid, avc_error *error) {
    if (repo == NULL || refname == NULL || oid == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "git_read_ref received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *ref_path = avc_fs_join(repo->gitdir_path, refname);
    if (ref_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    if (avc_fs_is_regular_file(ref_path)) {
        char *content = NULL;
        size_t size = 0;
        avc_status status = avc_fs_read_file(ref_path, &content, &size, error);
        free(ref_path);
        if (status != AVC_OK) return status;

        while (size > 0 && (content[size - 1] == '\n' ||
                            content[size - 1] == '\r')) {
            content[--size] = '\0';
        }

        if (strncmp(content, "ref: ", 5) == 0) {
            char *target = content + 5;
            size_t tlen = strlen(target);
            if (tlen > 0) {
                avc_error_clear(error);
                avc_status st = avc_git_read_ref(repo, target, oid, error);
                free(content);
                return st;
            }
            free(content);
            avc_error_set(error, AVC_ERR_PARSE, "empty symbolic ref");
            return AVC_ERR_PARSE;
        }

        if (strlen(content) != 40 ||
            avc_oid_parse(content, oid) != 0) {
            free(content);
            avc_error_set(error, AVC_ERR_PARSE, "invalid ref content");
            return AVC_ERR_PARSE;
        }
        free(content);
        return AVC_OK;
    }
    free(ref_path);

    char *packed_refs = avc_fs_join(repo->gitdir_path, "packed-refs");
    if (packed_refs == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    if (read_packed_refs(packed_refs, refname, oid) == 0) {
        free(packed_refs);
        return AVC_OK;
    }
    free(packed_refs);

    avc_error_setf(error, AVC_ERR_NOT_FOUND, "ref not found: %s", refname);
    return AVC_ERR_NOT_FOUND;
}

avc_status avc_git_list_refs(const avc_git_repo *repo,
                             char ***refnames, avc_oid *oids, int *count,
                             avc_error *error) {
    if (repo == NULL || refnames == NULL || count == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "git_list_refs received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    int cap = 64;
    int cnt = 0;
    char **names = (char **)calloc((size_t)cap, sizeof(char *));
    avc_oid *oid_list = (avc_oid *)calloc((size_t)cap, sizeof(avc_oid));
    if (names == NULL || oid_list == NULL) {
        free(names);
        free(oid_list);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    char *packed_refs_path = avc_fs_join(repo->gitdir_path, "packed-refs");
    if (packed_refs_path != NULL && avc_fs_is_regular_file(packed_refs_path)) {
        char *content = NULL;
        size_t size = 0;
        avc_error err;
        avc_error_clear(&err);
        if (avc_fs_read_file(packed_refs_path, &content, &size, &err) == AVC_OK) {
            char *line = content;
            char *end = content + size;
            while (line < end) {
                char *nl = memchr(line, '\n', (size_t)(end - line));
                if (nl == NULL) nl = end;
                size_t line_len = (size_t)(nl - line);
                if (line_len >= 41 && line[40] == ' ' && line[41] != '^') {
                    char hex[41];
                    memcpy(hex, line, 40);
                    hex[40] = '\0';
                    const char *rname = line + 41;
                    size_t rlen = line_len - 41;

                    if (cnt >= cap) {
                        int new_cap = cap * 2;
                        char **saved_n = names;
                        avc_oid *saved_o = oid_list;
                        names = (char **)malloc((size_t)new_cap * sizeof(char *));
                        oid_list = (avc_oid *)malloc((size_t)new_cap * sizeof(avc_oid));
                        if (names == NULL || oid_list == NULL) {
                            free(content);
                            free(packed_refs_path);
                            for (int i = 0; i < cnt; i++) free(saved_n[i]);
                            free(saved_n);
                            free(saved_o);
                            free(names);
                            free(oid_list);
                            avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
                            return AVC_ERR_NO_MEMORY;
                        }
                        memcpy(names, saved_n, (size_t)cnt * sizeof(char *));
                        memcpy(oid_list, saved_o, (size_t)cnt * sizeof(avc_oid));
                        free(saved_n);
                        free(saved_o);
                        cap = new_cap;
                    }

                    names[cnt] = (char *)malloc(rlen + 1);
                    if (names[cnt] != NULL) {
                        memcpy(names[cnt], rname, rlen);
                        names[cnt][rlen] = '\0';
                        avc_oid_parse(hex, oid_list[cnt]);
                        cnt++;
                    }
                }
                line = nl + 1;
            }
            free(content);
        }
    }
    free(packed_refs_path);

    *refnames = names;
    if (oids != NULL) {
        memcpy(oids, oid_list, (size_t)cnt * sizeof(avc_oid));
    }
    free(oid_list);
    *count = cnt;
    return AVC_OK;
}

avc_status avc_git_resolve_ref(const avc_git_repo *repo, const char *refname,
                               avc_oid oid, avc_error *error) {
    if (repo == NULL || refname == NULL || oid == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "git_resolve_ref received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    if (strcmp(refname, "HEAD") == 0) {
        char *head_path = avc_fs_join(repo->gitdir_path, "HEAD");
        if (head_path == NULL) {
            avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
            return AVC_ERR_NO_MEMORY;
        }

        char *content = NULL;
        size_t size = 0;
        avc_status status = avc_fs_read_file(head_path, &content, &size, error);
        free(head_path);
        if (status != AVC_OK) return status;

        while (size > 0 && (content[size - 1] == '\n' ||
                            content[size - 1] == '\r')) {
            content[--size] = '\0';
        }

        if (strncmp(content, "ref: ", 5) == 0) {
            char *target = content + 5;
            status = avc_git_read_ref(repo, target, oid, error);
            free(content);
            return status;
        }

        if (strlen(content) == 40 &&
            avc_oid_parse(content, oid) == 0) {
            free(content);
            return AVC_OK;
        }
        free(content);
        avc_error_set(error, AVC_ERR_PARSE, "invalid HEAD");
        return AVC_ERR_PARSE;
    }

    return avc_git_read_ref(repo, refname, oid, error);
}
