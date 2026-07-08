#include "objects/avc_object.h"

#include "compression/avc_compress.h"
#include "hashing/avc_hash.h"
#include "utils/avc_fs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const TYPE_NAMES[] = {"blob", "tree", "commit", "tag"};
#define TYPE_NAMES_COUNT 4

const char *avc_object_type_name(avc_object_type type) {
    if ((size_t)type >= TYPE_NAMES_COUNT) {
        return NULL;
    }
    return TYPE_NAMES[type];
}

int avc_object_type_from_name(const char *name, avc_object_type *out) {
    for (int i = 0; i < (int)TYPE_NAMES_COUNT; i++) {
        if (strcmp(name, TYPE_NAMES[i]) == 0) {
            *out = (avc_object_type)i;
            return 0;
        }
    }
    return -1;
}

void avc_odb_init(avc_odb *odb) {
    if (odb == NULL) return;
    odb->objects_path = NULL;
    odb->fd_lock = -1;
}

void avc_odb_close(avc_odb *odb) {
    if (odb == NULL) return;
    free(odb->objects_path);
    avc_odb_init(odb);
}

avc_status avc_odb_open(avc_odb *odb, const char *objects_path,
                        avc_error *error) {
    if (odb == NULL || objects_path == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "odb open requires objects path");
        return AVC_ERR_INVALID_ARGUMENT;
    }
    if (!avc_fs_is_directory(objects_path)) {
        avc_error_setf(error, AVC_ERR_NOT_FOUND,
                       "objects directory not found: %s", objects_path);
        return AVC_ERR_NOT_FOUND;
    }
    char *path = avc_fs_join(objects_path, "");
    if (path == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory opening odb");
        return AVC_ERR_NO_MEMORY;
    }
    avc_odb_close(odb);
    odb->objects_path = path;
    return AVC_OK;
}

static void oid_to_path_segment(const avc_oid oid, char *dir, size_t dir_size,
                                char *file, size_t file_size) {
    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(oid, hex);
    char saved = hex[2];
    hex[2] = '\0';
    snprintf(dir, dir_size, "%s", hex);
    hex[2] = saved;
    snprintf(file, file_size, "%s", hex + 2);
}

static avc_status oid_to_full_path(const avc_odb *odb, const avc_oid oid,
                                   char *path, size_t path_size) {
    char dir[3], file[39];
    oid_to_path_segment(oid, dir, sizeof(dir), file, sizeof(file));
    snprintf(path, path_size, "%s/%s/%s", odb->objects_path, dir, file);
    return AVC_OK;
}

avc_status avc_odb_write(avc_odb *odb, avc_object_type type,
                         const void *payload, size_t payload_size,
                         avc_oid out, avc_error *error) {
    if (odb == NULL || payload == NULL || out == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "odb write received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    const char *type_str = avc_object_type_name(type);
    if (type_str == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "invalid object type");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    int header_len = snprintf(NULL, 0, "%s %zu", type_str, payload_size);
    if (header_len < 0) {
        avc_error_set(error, AVC_ERR_IO, "failed to format object header");
        return AVC_ERR_IO;
    }

    size_t canonical_size = (size_t)header_len + 1 + payload_size;
    unsigned char *canonical = (unsigned char *)malloc(canonical_size);
    if (canonical == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory creating canonical object");
        return AVC_ERR_NO_MEMORY;
    }

    snprintf((char *)canonical, canonical_size, "%s %zu", type_str,
             payload_size);
    canonical[(size_t)header_len] = '\0';
    memcpy(canonical + (size_t)header_len + 1, payload, payload_size);

    avc_sha1_ctx hash_ctx;
    avc_sha1_init(&hash_ctx);
    avc_sha1_update(&hash_ctx, canonical, canonical_size);
    avc_sha1_final(&hash_ctx, out);

    void *compressed = NULL;
    size_t compressed_size = 0;
    avc_status status = avc_compress(canonical, canonical_size,
                                      &compressed, &compressed_size, error);
    free(canonical);
    if (status != AVC_OK) {
        return status;
    }

    char dir[3], file[39];
    oid_to_path_segment(out, dir, sizeof(dir), file, sizeof(file));

    char *obj_dir = avc_fs_join(odb->objects_path, dir);
    if (obj_dir == NULL) {
        free(compressed);
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory creating object dir path");
        return AVC_ERR_NO_MEMORY;
    }

    status = avc_fs_mkdir_p(obj_dir, error);
    if (status != AVC_OK) {
        free(obj_dir);
        free(compressed);
        return status;
    }

    char *final_path = avc_fs_join(obj_dir, file);
    free(obj_dir);
    if (final_path == NULL) {
        free(compressed);
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory creating object file path");
        return AVC_ERR_NO_MEMORY;
    }

    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp/obj_%u_%u",
             odb->objects_path, (unsigned)rand(), (unsigned)clock());

    status = avc_fs_write_file(tmp_path, compressed, compressed_size, error);
    free(compressed);
    if (status != AVC_OK) {
        free(final_path);
        return status;
    }

    if (rename(tmp_path, final_path) != 0) {
        remove(tmp_path);
        free(final_path);
        avc_error_setf(error, AVC_ERR_IO, "failed to rename object file: %s", strerror(errno));
        return AVC_ERR_IO;
    }

    free(final_path);
    return AVC_OK;
}

avc_status avc_odb_read(avc_odb *odb, const avc_oid oid,
                        avc_object_type *type,
                        void **payload, size_t *payload_size,
                        avc_error *error) {
    if (odb == NULL || type == NULL || payload == NULL ||
        payload_size == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "odb read received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char path[512];
    oid_to_full_path(odb, oid, path, sizeof(path));

    void *compressed = NULL;
    size_t compressed_size = 0;
    avc_status status = avc_fs_read_file(path, (char **)&compressed,
                                          &compressed_size, error);
    if (status != AVC_OK) {
        return status;
    }

    void *decompressed = NULL;
    size_t decompressed_size = 0;
    status = avc_decompress(compressed, compressed_size,
                            &decompressed, &decompressed_size, error);
    free(compressed);
    if (status != AVC_OK) {
        return status;
    }

    unsigned char *bytes = (unsigned char *)decompressed;
    size_t remaining = decompressed_size;

    char type_buf[32];
    size_t ti = 0;

    while (ti < sizeof(type_buf) - 1 && ti < remaining &&
           bytes[ti] != ' ') {
        type_buf[ti] = (char)bytes[ti];
        ti++;
    }
    if (ti >= remaining || bytes[ti] != ' ') {
        free(decompressed);
        avc_error_set(error, AVC_ERR_PARSE, "malformed object header");
        return AVC_ERR_PARSE;
    }
    type_buf[ti] = '\0';

    if (avc_object_type_from_name(type_buf, type) != 0) {
        free(decompressed);
        avc_error_setf(error, AVC_ERR_PARSE,
                       "unknown object type: %s", type_buf);
        return AVC_ERR_PARSE;
    }

    ti++;
    size_t declared_size = 0;
    while (ti < remaining && bytes[ti] >= '0' && bytes[ti] <= '9') {
        declared_size = declared_size * 10 + (size_t)(bytes[ti] - '0');
        ti++;
    }
    if (ti >= remaining || bytes[ti] != '\0') {
        free(decompressed);
        avc_error_set(error, AVC_ERR_PARSE,
                      "malformed object size in header");
        return AVC_ERR_PARSE;
    }
    ti++;

    size_t actual_size = remaining - ti;
    if (actual_size != declared_size) {
        free(decompressed);
        avc_error_set(error, AVC_ERR_PARSE,
                      "object size mismatch");
        return AVC_ERR_PARSE;
    }

    unsigned char *payload_buf = (unsigned char *)malloc(declared_size + 1);
    if (payload_buf == NULL) {
        free(decompressed);
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory reading object payload");
        return AVC_ERR_NO_MEMORY;
    }
    memcpy(payload_buf, bytes + ti, declared_size);
    payload_buf[declared_size] = '\0';

    free(decompressed);
    *payload = payload_buf;
    *payload_size = declared_size;
    return AVC_OK;
}

int avc_odb_exists(avc_odb *odb, const avc_oid oid) {
    if (odb == NULL) return 0;
    char path[512];
    oid_to_full_path(odb, oid, path, sizeof(path));
    return avc_fs_path_exists(path) ? 1 : 0;
}

avc_status avc_odb_write_blob(avc_odb *odb, const void *data, size_t size,
                              avc_oid out, avc_error *error) {
    return avc_odb_write(odb, AVC_OBJECT_BLOB, data, size, out, error);
}
