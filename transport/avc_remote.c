#define _POSIX_C_SOURCE 200809L

#include "transport/avc_remote.h"

#include "commits/avc_commit.h"
#include "config/avc_config.h"
#include "utils/avc_fs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

avc_status avc_remote_list(const char *metadata_path,
                           avc_remote **remotes, int *count,
                           avc_error *error) {
    char *config_path = avc_fs_join(metadata_path, "config");
    if (config_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    avc_config config;
    avc_config_init(&config);
    avc_status status = avc_config_load(&config, config_path, error);
    free(config_path);
    if (status != AVC_OK) {
        *remotes = NULL;
        *count = 0;
        return AVC_OK;
    }

    int cap = 8, cnt = 0;
    avc_remote *list = (avc_remote *)malloc(
        (size_t)cap * sizeof(avc_remote));
    if (list == NULL) {
        avc_config_free(&config);
        return AVC_ERR_NO_MEMORY;
    }

    for (size_t i = 0; i < config.count; i++) {
        if (strcmp(config.entries[i].section, "remote") != 0) continue;

        if (cnt >= cap) {
            cap *= 2;
            avc_remote *nl = (avc_remote *)realloc(
                list, (size_t)cap * sizeof(avc_remote));
            if (nl == NULL) {
                for (int j = 0; j < cnt; j++) {
                    free(list[j].name);
                    free(list[j].url);
                }
                free(list);
                avc_config_free(&config);
                return AVC_ERR_NO_MEMORY;
            }
            list = nl;
        }

        list[cnt].name = strdup(config.entries[i].key);
        list[cnt].url = strdup(config.entries[i].value);
        if (list[cnt].name == NULL || list[cnt].url == NULL) {
            for (int j = 0; j < cnt; j++) {
                free(list[j].name);
                free(list[j].url);
            }
            free(list);
            avc_config_free(&config);
            return AVC_ERR_NO_MEMORY;
        }
        cnt++;
    }

    avc_config_free(&config);
    *remotes = list;
    *count = cnt;
    return AVC_OK;
}

void avc_remote_list_free(avc_remote *remotes, int count) {
    if (remotes == NULL) return;
    for (int i = 0; i < count; i++) {
        free(remotes[i].name);
        free(remotes[i].url);
    }
    free(remotes);
}

avc_status avc_remote_add(const char *metadata_path,
                          const char *name, const char *url,
                          avc_error *error) {
    char *config_path = avc_fs_join(metadata_path, "config");
    if (config_path == NULL) return AVC_ERR_NO_MEMORY;

    avc_config config;
    avc_config_init(&config);
    avc_config_load(&config, config_path, error);
    avc_error_clear(error);

    avc_status status = avc_config_set(&config, "remote", name, url, error);
    if (status != AVC_OK) {
        avc_config_free(&config);
        free(config_path);
        return status;
    }

    status = avc_config_write(&config, config_path, error);
    avc_config_free(&config);
    free(config_path);
    return status;
}

avc_status avc_remote_remove(const char *metadata_path,
                             const char *name,
                             avc_error *error) {
    char *config_path = avc_fs_join(metadata_path, "config");
    if (config_path == NULL) return AVC_ERR_NO_MEMORY;

    avc_config config;
    avc_config_init(&config);
    avc_status status = avc_config_load(&config, config_path, error);
    if (status != AVC_OK) {
        free(config_path);
        return status;
    }

    int found = 0;
    for (size_t i = 0; i < config.count; i++) {
        if (strcmp(config.entries[i].section, "remote") == 0 &&
            strcmp(config.entries[i].key, name) == 0) {
            free(config.entries[i].section);
            free(config.entries[i].key);
            free(config.entries[i].value);
            config.entries[i] = config.entries[config.count - 1];
            config.count--;
            found = 1;
            break;
        }
    }

    if (!found) {
        avc_config_free(&config);
        free(config_path);
        avc_error_set(error, AVC_ERR_NOT_FOUND, "remote not found");
        return AVC_ERR_NOT_FOUND;
    }

    status = avc_config_write(&config, config_path, error);
    avc_config_free(&config);
    free(config_path);
    return status;
}

static avc_status ensure_objects_dir(const char *objects_path,
                                     avc_error *error) {
    if (!avc_fs_is_directory(objects_path)) {
        return avc_fs_mkdir_p(objects_path, error);
    }
    return AVC_OK;
}

static int copy_object_file(const char *src_objects, const char *dst_objects,
                            const char *dir, const char *file,
                            avc_error *error) {
    char src_path[512], dst_path[512], dst_dir[512];
    snprintf(src_path, sizeof(src_path), "%s/%s/%s", src_objects, dir, file);
    snprintf(dst_dir, sizeof(dst_dir), "%s/%s", dst_objects, dir);
    snprintf(dst_path, sizeof(dst_path), "%s/%s/%s", dst_objects, dir, file);

    if (avc_fs_path_exists(dst_path)) return 0;

    struct stat st;
    if (stat(src_path, &st) != 0) return 0;

    avc_fs_mkdir_p(dst_dir, error);

    char *content = NULL;
    size_t size = 0;
    avc_status status = avc_fs_read_file(src_path, &content, &size, error);
    if (status != AVC_OK) return -1;

    status = avc_fs_write_file(dst_path, content, size, error);
    free(content);
    return status == AVC_OK ? 1 : -1;
}

static avc_status copy_objects(const char *src_objects,
                               const char *dst_objects,
                               avc_error *error) {
    avc_status status = ensure_objects_dir(dst_objects, error);
    if (status != AVC_OK) return status;

    DIR *dir = opendir(src_objects);
    if (dir == NULL) return AVC_OK;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char dir_path[512];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", src_objects,
                 entry->d_name);
        struct stat st;
        if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        DIR *sub = opendir(dir_path);
        if (sub == NULL) continue;

        struct dirent *file_entry;
        while ((file_entry = readdir(sub)) != NULL) {
            if (file_entry->d_name[0] == '.') continue;
            int ret = copy_object_file(src_objects, dst_objects,
                                       entry->d_name, file_entry->d_name,
                                       error);
            if (ret < 0) {
                closedir(sub);
                closedir(dir);
                return AVC_ERR_IO;
            }
        }
        closedir(sub);
    }

    closedir(dir);
    return AVC_OK;
}

static avc_status scan_and_copy_objects(avc_odb *src_odb,
                                        avc_odb *dst_odb,
                                        const avc_oid commit_oid,
                                        avc_error *error) {
    void *payload = NULL;
    size_t size = 0;
    avc_object_type type;

    avc_oid non_const_commit;
    memcpy(non_const_commit, commit_oid, 20);

    avc_status status = avc_odb_read(src_odb, commit_oid, &type,
                                      &payload, &size, error);
    if (status != AVC_OK) return status;
    if (type != AVC_OBJECT_COMMIT) { free(payload); return AVC_ERR_PARSE; }

    avc_oid tree_oid, parent_ids[16];
    int parent_count = 0;
    avc_commit_parse((const unsigned char *)payload, size,
                      &tree_oid, parent_ids, &parent_count,
                      NULL, 0, NULL, 0, NULL, 0, error);

    if (!avc_odb_exists(dst_odb, non_const_commit)) {
        void *tree_payload = NULL;
        size_t tree_size = 0;
        avc_oid non_const_tree;
        memcpy(non_const_tree, tree_oid, 20);
        status = avc_odb_read(src_odb, tree_oid, &type,
                               &tree_payload, &tree_size, error);
        if (status == AVC_OK && type == AVC_OBJECT_TREE) {
            avc_odb_write(dst_odb, AVC_OBJECT_TREE, tree_payload,
                          tree_size, non_const_tree, error);
            free(tree_payload);
        }

        avc_odb_write(dst_odb, AVC_OBJECT_COMMIT, payload, size,
                      non_const_commit, error);
    }

    free(payload);
    (void)parent_ids;
    (void)parent_count;
    return AVC_OK;
}

avc_status avc_remote_fetch(avc_odb *local_odb,
                            const char *metadata_path,
                            const char *remote_url,
                            const char *remote_name,
                            avc_error *error) {
    char *remote_objects = avc_fs_join(remote_url, ".avc/objects");
    if (remote_objects == NULL) return AVC_ERR_NO_MEMORY;

    avc_status status = copy_objects(remote_objects, local_odb->objects_path,
                                      error);
    if (status != AVC_OK) {
        free(remote_objects);
        return status;
    }

    char *remote_refs = avc_fs_join(remote_url, ".avc/refs/heads");
    if (remote_refs == NULL) { free(remote_objects); return AVC_ERR_NO_MEMORY; }

    DIR *dir = opendir(remote_refs);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            char ref_path[512];
            snprintf(ref_path, sizeof(ref_path), "%s/%s", remote_refs,
                     entry->d_name);

            char *content = NULL;
            size_t size = 0;
            if (avc_fs_read_file(ref_path, &content, &size, error) == AVC_OK) {
                while (size > 0 && (content[size - 1] == '\n' ||
                                    content[size - 1] == '\r')) {
                    content[--size] = '\0';
                }

                if (strlen(content) == 40) {
                    char tracking_ref[512];
                    snprintf(tracking_ref, sizeof(tracking_ref),
                             "refs/remotes/%s/%s", remote_name,
                             entry->d_name);

                    char *tracking_path = avc_fs_join(metadata_path,
                                                       tracking_ref);
                    if (tracking_path != NULL) {
                        char *parent = avc_fs_parent(tracking_path);
                        if (parent != NULL) {
                            avc_fs_mkdir_p(parent, error);
                            free(parent);
                        }
                        avc_fs_write_file(tracking_path, content, size,
                                          error);
                        free(tracking_path);
                    }
                }
                free(content);
            }
        }
        closedir(dir);
    }

    free(remote_refs);
    free(remote_objects);
    return AVC_OK;
}

avc_status avc_remote_push(avc_odb *local_odb,
                           const char *metadata_path,
                           const char *remote_url,
                           const char *remote_name,
                           const char *branch,
                           avc_error *error) {
    (void)remote_name;

    char *remote_objects = avc_fs_join(remote_url, ".avc/objects");
    if (remote_objects == NULL) return AVC_ERR_NO_MEMORY;

    avc_odb remote_odb;
    avc_odb_init(&remote_odb);

    avc_fs_mkdir_p(remote_objects, error);
    avc_error_clear(error);

    avc_status status = avc_odb_open(&remote_odb, remote_objects, error);
    if (status != AVC_OK) {
        free(remote_objects);
        return status;
    }

    char refname[256];
    snprintf(refname, sizeof(refname), "refs/heads/%s", branch);

    char *ref_path = avc_fs_join(metadata_path, refname);
    if (ref_path == NULL) {
        avc_odb_close(&remote_odb);
        free(remote_objects);
        return AVC_ERR_NO_MEMORY;
    }

    char *content = NULL;
    size_t size = 0;
    status = avc_fs_read_file(ref_path, &content, &size, error);
    free(ref_path);
    if (status != AVC_OK) {
        avc_odb_close(&remote_odb);
        free(remote_objects);
        return status;
    }

    while (size > 0 && (content[size - 1] == '\n' ||
                        content[size - 1] == '\r')) {
        content[--size] = '\0';
    }

    avc_oid commit_oid;
    if (avc_oid_parse(content, commit_oid) != 0) {
        free(content);
        avc_odb_close(&remote_odb);
        free(remote_objects);
        avc_error_set(error, AVC_ERR_PARSE, "invalid commit OID");
        return AVC_ERR_PARSE;
    }
    free(content);

    avc_odb_close(&remote_odb);
    avc_odb_init(&remote_odb);
    status = avc_odb_open(&remote_odb, remote_objects, error);
    if (status != AVC_OK) { free(remote_objects); return status; }

    status = scan_and_copy_objects(local_odb, &remote_odb, commit_oid,
                                    error);

    avc_odb_close(&remote_odb);
    free(remote_objects);

    if (status != AVC_OK) return status;

    char *remote_ref_path = avc_fs_join(remote_url, ".avc/refs/heads");
    if (remote_ref_path == NULL) return AVC_ERR_NO_MEMORY;

    avc_fs_mkdir_p(remote_ref_path, error);
    avc_error_clear(error);

    char dst_ref[512];
    snprintf(dst_ref, sizeof(dst_ref), "%s/%s", remote_ref_path, branch);
    free(remote_ref_path);

    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(commit_oid, hex);
    char hex_line[48];
    snprintf(hex_line, sizeof(hex_line), "%s\n", hex);

    status = avc_fs_write_file(dst_ref, hex_line, strlen(hex_line), error);
    return status;
}
