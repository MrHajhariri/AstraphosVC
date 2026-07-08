#define _POSIX_C_SOURCE 200809L

#include "refs/avc_refs.h"

#include "utils/avc_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

avc_status avc_refs_read_head(const char *metadata_path, char **ref_or_oid,
                              int *is_symbolic, avc_error *error) {
    if (metadata_path == NULL || ref_or_oid == NULL ||
        is_symbolic == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "refs_read_head received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *head_path = avc_fs_join(metadata_path, "HEAD");
    if (head_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory reading HEAD");
        return AVC_ERR_NO_MEMORY;
    }

    char *content = NULL;
    size_t size = 0;
    avc_status status = avc_fs_read_file(head_path, &content, &size, error);
    free(head_path);
    if (status != AVC_OK) {
        return status;
    }

    while (size > 0 && (content[size - 1] == '\n' ||
                        content[size - 1] == '\r')) {
        content[--size] = '\0';
    }

    if (strncmp(content, "ref: ", 5) == 0) {
        *is_symbolic = 1;
        *ref_or_oid = strdup(content + 5);
        if (*ref_or_oid == NULL) {
            free(content);
            avc_error_set(error, AVC_ERR_NO_MEMORY,
                          "out of memory reading HEAD ref");
            return AVC_ERR_NO_MEMORY;
        }
    } else {
        *is_symbolic = 0;
        *ref_or_oid = strdup(content);
        if (*ref_or_oid == NULL) {
            free(content);
            avc_error_set(error, AVC_ERR_NO_MEMORY,
                          "out of memory reading HEAD oid");
            return AVC_ERR_NO_MEMORY;
        }
    }

    free(content);
    return AVC_OK;
}

avc_status avc_refs_write_head_ref(const char *metadata_path, const char *ref,
                                   avc_error *error) {
    if (metadata_path == NULL || ref == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "refs_write_head_ref received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *head_path = avc_fs_join(metadata_path, "HEAD");
    if (head_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory writing HEAD");
        return AVC_ERR_NO_MEMORY;
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "ref: %s\n", ref);

    avc_status status = avc_fs_write_file(head_path, buf, strlen(buf),
                                           error);
    free(head_path);
    return status;
}

avc_status avc_refs_write_head_oid(const char *metadata_path,
                                   const avc_oid oid, avc_error *error) {
    if (metadata_path == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "refs_write_head_oid received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *head_path = avc_fs_join(metadata_path, "HEAD");
    if (head_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory writing HEAD");
        return AVC_ERR_NO_MEMORY;
    }

    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(oid, hex);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s\n", hex);

    avc_status status = avc_fs_write_file(head_path, buf, strlen(buf),
                                           error);
    free(head_path);
    return status;
}

avc_status avc_refs_read_ref(const char *metadata_path, const char *refname,
                             avc_oid oid, avc_error *error) {
    if (metadata_path == NULL || refname == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "refs_read_ref received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *ref_path = avc_fs_join(metadata_path, refname);
    if (ref_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory reading ref");
        return AVC_ERR_NO_MEMORY;
    }

    char *content = NULL;
    size_t size = 0;
    avc_status status = avc_fs_read_file(ref_path, &content, &size, error);
    free(ref_path);
    if (status != AVC_OK) {
        return status;
    }

    while (size > 0 && (content[size - 1] == '\n' ||
                        content[size - 1] == '\r')) {
        content[--size] = '\0';
    }

    if (strlen(content) != 40) {
        free(content);
        avc_error_set(error, AVC_ERR_PARSE, "invalid ref content");
        return AVC_ERR_PARSE;
    }

    if (avc_oid_parse(content, oid) != 0) {
        free(content);
        avc_error_set(error, AVC_ERR_PARSE, "invalid ref OID");
        return AVC_ERR_PARSE;
    }

    free(content);
    return AVC_OK;
}

avc_status avc_refs_write_ref(const char *metadata_path, const char *refname,
                              const avc_oid oid, avc_error *error) {
    if (metadata_path == NULL || refname == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "refs_write_ref received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *ref_path = avc_fs_join(metadata_path, refname);
    if (ref_path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory writing ref");
        return AVC_ERR_NO_MEMORY;
    }

    char *dir = avc_fs_parent(ref_path);
    if (dir != NULL) {
        avc_fs_mkdir_p(dir, error);
        free(dir);
    }

    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(oid, hex);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s\n", hex);

    avc_status status = avc_fs_write_file(ref_path, buf, strlen(buf),
                                           error);
    free(ref_path);
    return status;
}

avc_status avc_refs_resolve_head(const char *metadata_path, avc_oid out,
                                 avc_error *error) {
    char *ref_or_oid = NULL;
    int is_symbolic = 0;

    avc_status status = avc_refs_read_head(metadata_path, &ref_or_oid,
                                            &is_symbolic, error);
    if (status != AVC_OK) {
        return status;
    }

    if (!is_symbolic) {
        if (strlen(ref_or_oid) != 40 ||
            avc_oid_parse(ref_or_oid, out) != 0) {
            free(ref_or_oid);
            avc_error_set(error, AVC_ERR_PARSE, "invalid HEAD OID");
            return AVC_ERR_PARSE;
        }
        free(ref_or_oid);
        return AVC_OK;
    }

    status = avc_refs_read_ref(metadata_path, ref_or_oid, out, error);
    free(ref_or_oid);
    return status;
}
