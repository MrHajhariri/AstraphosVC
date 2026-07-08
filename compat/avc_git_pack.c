#define _POSIX_C_SOURCE 200809L

#include "compat/avc_git_pack.h"

#include "compression/avc_compress.h"
#include "hashing/avc_hash.h"
#include "utils/avc_fs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define GIT_PACK_SIGNATURE "\xd8\xec\x0c\xd1"
#define GIT_PACK_IDX_SIGNATURE "\xff\x74\x4f\x63"

static uint32_t read_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t read_be64(const unsigned char *p) {
    return ((uint64_t)read_be32(p) << 32) | (uint64_t)read_be32(p + 4);
}

static int oid_cmp(const avc_oid a, const avc_oid b) {
    return memcmp(a, b, 20);
}

static int entry_cmp(const void *a, const void *b) {
    const avc_git_pack_entry *ea = (const avc_git_pack_entry *)a;
    const avc_git_pack_entry *eb = (const avc_git_pack_entry *)b;
    return oid_cmp(ea->oid, eb->oid);
}

static int parse_varint(const unsigned char *data, size_t max_size,
                        uint64_t *value, size_t *consumed) {
    *value = 0;
    *consumed = 0;
    for (size_t i = 0; i < max_size && i < 9; i++) {
        *value = ((*value) << 7) | (data[i] & 0x7F);
        (*consumed)++;
        if (!(data[i] & 0x80)) {
            return 0;
        }
    }
    return -1;
}

static int parse_offset_varint(const unsigned char *data, size_t max_size,
                               uint64_t *value, size_t *consumed) {
    *value = 0;
    *consumed = 0;
    if (max_size < 1) return -1;
    if (data[0] & 0x80) {
        *value = data[0] & 0x7F;
        *consumed = 1;
        return 0;
    }
    if (max_size < 2) return -1;
    *value = ((uint64_t)(data[0] & 0x7F)) << 8;
    *value |= data[1];
    *consumed = 2;
    return 0;
}

avc_status avc_git_pack_open(avc_git_pack *pack, const char *pack_path,
                             avc_error *error) {
    if (pack == NULL || pack_path == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "pack open received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    memset(pack, 0, sizeof(*pack));
    pack->fd_pack = -1;
    pack->fd_idx = -1;

    size_t plen = strlen(pack_path);
    if (plen < 5 || strcmp(pack_path + plen - 5, ".pack") != 0) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "pack path must end in .pack");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *base = strdup(pack_path);
    if (base == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }
    base[plen - 5] = '\0';

    size_t idx_len = strlen(base) + 5;
    pack->idx_path = (char *)malloc(idx_len);
    if (pack->idx_path == NULL) {
        free(base);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }
    snprintf(pack->idx_path, idx_len, "%s.idx", base);

    pack->pack_path = strdup(pack_path);
    if (pack->pack_path == NULL) {
        free(base);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }
    free(base);

    struct stat st;
    if (stat(pack->pack_path, &st) != 0) {
        avc_error_setf(error, AVC_ERR_NOT_FOUND, "pack file not found: %s", pack_path);
        return AVC_ERR_NOT_FOUND;
    }
    pack->pack_size = (size_t)st.st_size;

    pack->fd_pack = open(pack->pack_path, O_RDONLY);
    if (pack->fd_pack < 0) {
        avc_error_setf(error, AVC_ERR_IO, "cannot open pack: %s", strerror(errno));
        return AVC_ERR_IO;
    }

    pack->pack_data = (unsigned char *)mmap(NULL, pack->pack_size, PROT_READ,
                                            MAP_PRIVATE, pack->fd_pack, 0);
    if (pack->pack_data == MAP_FAILED) {
        pack->pack_data = NULL;
        close(pack->fd_pack);
        pack->fd_pack = -1;
        avc_error_setf(error, AVC_ERR_IO, "cannot mmap pack: %s", strerror(errno));
        return AVC_ERR_IO;
    }

    if (pack->pack_size < 12 ||
        memcmp(pack->pack_data, "PACK", 4) != 0) {
        avc_error_set(error, AVC_ERR_PARSE, "invalid pack signature");
        return AVC_ERR_PARSE;
    }

    uint32_t version = read_be32(pack->pack_data + 4);
    if (version != 2 && version != 3) {
        avc_error_set(error, AVC_ERR_PARSE, "unsupported pack version");
        return AVC_ERR_PARSE;
    }

    pack->object_count = read_be32(pack->pack_data + 8);

    pack->fd_idx = open(pack->idx_path, O_RDONLY);
    if (pack->fd_idx < 0) {
        avc_error_setf(error, AVC_ERR_NOT_FOUND, "pack index not found: %s",
                       pack->idx_path);
        return AVC_ERR_NOT_FOUND;
    }

    struct stat idx_st;
    if (fstat(pack->fd_idx, &idx_st) != 0) {
        avc_error_setf(error, AVC_ERR_IO, "cannot stat idx: %s", strerror(errno));
        return AVC_ERR_IO;
    }

    size_t idx_size = (size_t)idx_st.st_size;
    unsigned char *idx_data = (unsigned char *)mmap(NULL, idx_size, PROT_READ,
                                                     MAP_PRIVATE, pack->fd_idx, 0);
    if (idx_data == MAP_FAILED) {
        avc_error_setf(error, AVC_ERR_IO, "cannot mmap idx: %s", strerror(errno));
        return AVC_ERR_IO;
    }

    if (idx_size < 8 || memcmp(idx_data, GIT_PACK_IDX_SIGNATURE, 4) != 0) {
        munmap(idx_data, idx_size);
        avc_error_set(error, AVC_ERR_PARSE, "invalid pack index signature");
        return AVC_ERR_PARSE;
    }

    uint32_t idx_version = read_be32(idx_data + 4);
    if (idx_version != 2) {
        munmap(idx_data, idx_size);
        avc_error_set(error, AVC_ERR_PARSE, "unsupported pack index version");
        return AVC_ERR_PARSE;
    }

    unsigned char *fanout = idx_data + 8;
    uint32_t last_fanout = read_be32(fanout + 255 * 4);
    if (last_fanout != pack->object_count) {
        munmap(idx_data, idx_size);
        avc_error_set(error, AVC_ERR_PARSE, "pack index object count mismatch");
        return AVC_ERR_PARSE;
    }

    unsigned char *oid_data = fanout + 256 * 4;
    unsigned char *crc_data = oid_data + (size_t)pack->object_count * 20;
    unsigned char *offset_data = crc_data + (size_t)pack->object_count * 4;

    int has_large_offsets = 0;
    for (uint32_t i = 0; i < pack->object_count; i++) {
        uint32_t off = read_be32(offset_data + i * 4);
        if (off & 0x80000000) {
            has_large_offsets = 1;
            break;
        }
    }

    unsigned char *large_offset_data = NULL;
    if (has_large_offsets) {
        large_offset_data = offset_data + (size_t)pack->object_count * 4;
    }

    pack->entries = (avc_git_pack_entry *)calloc(pack->object_count,
                                                  sizeof(avc_git_pack_entry));
    if (pack->entries == NULL) {
        munmap(idx_data, idx_size);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < pack->object_count; i++) {
        memcpy(pack->entries[i].oid, oid_data + (size_t)i * 20, 20);
        pack->entries[i].crc32 = read_be32(crc_data + (size_t)i * 4);
        uint32_t off = read_be32(offset_data + (size_t)i * 4);
        if (off & 0x80000000) {
            uint32_t large_idx = off & 0x7FFFFFFF;
            pack->entries[i].offset = read_be64(large_offset_data + (size_t)large_idx * 8);
        } else {
            pack->entries[i].offset = off;
        }
    }

    qsort(pack->entries, pack->object_count, sizeof(avc_git_pack_entry), entry_cmp);

    munmap(idx_data, idx_size);
    return AVC_OK;
}

void avc_git_pack_close(avc_git_pack *pack) {
    if (pack == NULL) return;
    if (pack->pack_data != NULL && pack->pack_size > 0) {
        munmap(pack->pack_data, pack->pack_size);
    }
    if (pack->fd_pack >= 0) close(pack->fd_pack);
    if (pack->fd_idx >= 0) close(pack->fd_idx);
    free(pack->pack_path);
    free(pack->idx_path);
    free(pack->entries);
    memset(pack, 0, sizeof(*pack));
    pack->fd_pack = -1;
    pack->fd_idx = -1;
}

static int read_pack_object_raw(const unsigned char *data, size_t size,
                                avc_git_pack_object_type *type,
                                unsigned char **zlib_data, size_t *zlib_size,
                                uint64_t *consumed) {
    if (size < 1) return -1;

    uint64_t obj_size = 0;
    unsigned char b = data[0];
    *type = (avc_git_pack_object_type)((b >> 4) & 7);
    obj_size = b & 0x0F;
    size_t pos = 1;

    int shift = 4;
    while (pos < size && (data[pos - 1] & 0x80)) {
        if (pos >= size) return -1;
        obj_size |= ((uint64_t)(data[pos] & 0x7F)) << shift;
        shift += 7;
        pos++;
    }

    if (pos > size) return -1;

    *consumed = pos;
    *zlib_data = (unsigned char *)(data + pos);
    *zlib_size = size - pos;
    return 0;
}

static avc_status resolve_delta(const unsigned char *delta_data,
                                size_t delta_size,
                                const unsigned char *base_data,
                                size_t base_size,
                                void **result, size_t *result_size,
                                avc_error *error) {
    size_t pos = 0;

    if (pos >= delta_size) {
        avc_error_set(error, AVC_ERR_PARSE, "truncated delta header");
        return AVC_ERR_PARSE;
    }

    uint64_t src_size = 0;
    size_t consumed = 0;
    if (parse_varint(delta_data + pos, delta_size - pos, &src_size, &consumed) != 0) {
        avc_error_set(error, AVC_ERR_PARSE, "bad delta source size varint");
        return AVC_ERR_PARSE;
    }
    pos += consumed;

    if ((size_t)src_size != base_size) {
        avc_error_set(error, AVC_ERR_PARSE, "delta source size mismatch");
        return AVC_ERR_PARSE;
    }

    if (pos >= delta_size) {
        avc_error_set(error, AVC_ERR_PARSE, "truncated delta header");
        return AVC_ERR_PARSE;
    }

    uint64_t dst_size = 0;
    if (parse_varint(delta_data + pos, delta_size - pos, &dst_size, &consumed) != 0) {
        avc_error_set(error, AVC_ERR_PARSE, "bad delta target size varint");
        return AVC_ERR_PARSE;
    }
    pos += consumed;

    unsigned char *dst = (unsigned char *)calloc(1, (size_t)dst_size + 1);
    if (dst == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory for delta result");
        return AVC_ERR_NO_MEMORY;
    }
    size_t dst_pos = 0;

    while (pos < delta_size) {
        unsigned char op = delta_data[pos++];
        if (op & 0x80) {
            uint64_t offset = 0;
            uint64_t copy_size = 0;

            if (op & 0x01) { offset = delta_data[pos++]; }
            if (op & 0x02) { offset |= ((uint64_t)delta_data[pos++]) << 8; }
            if (op & 0x04) { offset |= ((uint64_t)delta_data[pos++]) << 16; }
            if (op & 0x08) { offset |= ((uint64_t)delta_data[pos++]) << 24; }

            if (op & 0x10) { copy_size = delta_data[pos++]; }
            if (op & 0x20) { copy_size |= ((uint64_t)delta_data[pos++]) << 8; }
            if (op & 0x40) { copy_size |= ((uint64_t)delta_data[pos++]) << 16; }

            if (copy_size == 0) copy_size = 0x10000;

            if (offset + copy_size > src_size ||
                dst_pos + copy_size > dst_size) {
                free(dst);
                avc_error_set(error, AVC_ERR_PARSE, "delta copy out of bounds");
                return AVC_ERR_PARSE;
            }
            memcpy(dst + dst_pos, base_data + offset, (size_t)copy_size);
            dst_pos += (size_t)copy_size;
        } else {
            size_t insert_size = op & 0x7F;
            if (pos + insert_size > delta_size ||
                dst_pos + insert_size > dst_size) {
                free(dst);
                avc_error_set(error, AVC_ERR_PARSE, "delta insert out of bounds");
                return AVC_ERR_PARSE;
            }
            memcpy(dst + dst_pos, delta_data + pos, insert_size);
            pos += insert_size;
            dst_pos += insert_size;
        }
    }

    if (dst_pos != (size_t)dst_size) {
        free(dst);
        avc_error_set(error, AVC_ERR_PARSE, "delta size mismatch");
        return AVC_ERR_PARSE;
    }

    *result = dst;
    *result_size = (size_t)dst_size;
    return AVC_OK;
}

static avc_status read_decompressed_object(const avc_git_pack *pack,
                                           uint64_t offset,
                                           avc_git_pack_object_type *type,
                                           void **payload, size_t *payload_size,
                                           avc_error *error) {
    if (offset >= pack->pack_size) {
        avc_error_set(error, AVC_ERR_PARSE, "object offset out of bounds");
        return AVC_ERR_PARSE;
    }

    const unsigned char *obj_start = pack->pack_data + offset;
    size_t remaining = pack->pack_size - (size_t)offset;

    avc_git_pack_object_type obj_type;
    unsigned char *zlib_data = NULL;
    size_t zlib_size = 0;
    uint64_t consumed = 0;

    if (read_pack_object_raw(obj_start, remaining, &obj_type,
                              &zlib_data, &zlib_size, &consumed) != 0) {
        avc_error_set(error, AVC_ERR_PARSE, "failed to read pack object header");
        return AVC_ERR_PARSE;
    }

    switch (obj_type) {
    case AVC_GIT_OBJ_COMMIT:
    case AVC_GIT_OBJ_TREE:
    case AVC_GIT_OBJ_BLOB:
    case AVC_GIT_OBJ_TAG: {
        void *decompressed = NULL;
        size_t decompressed_size = 0;
        avc_status status = avc_decompress(zlib_data, zlib_size,
                                            &decompressed, &decompressed_size,
                                            error);
        if (status != AVC_OK) return status;

        *type = (avc_git_pack_object_type)(obj_type - 1);
        *payload = decompressed;
        *payload_size = decompressed_size;
        return AVC_OK;
    }

    case AVC_GIT_OBJ_OFS_DELTA: {
        uint64_t base_offset_rel = 0;
        size_t ofs_consumed = 0;
        if (parse_offset_varint(zlib_data, zlib_size,
                                &base_offset_rel, &ofs_consumed) != 0) {
            avc_error_set(error, AVC_ERR_PARSE, "bad ofs_delta base offset");
            return AVC_ERR_PARSE;
        }

        if (base_offset_rel > offset) {
            avc_error_set(error, AVC_ERR_PARSE, "ofs_delta offset underflow");
            return AVC_ERR_PARSE;
        }
        uint64_t base_offset = offset - base_offset_rel;

        avc_git_pack_object_type base_type;
        void *base_data = NULL;
        size_t base_size = 0;
        avc_status status = read_decompressed_object(pack, base_offset,
                                                      &base_type, &base_data,
                                                      &base_size, error);
        if (status != AVC_OK) return status;

        unsigned char *delta_start = zlib_data + ofs_consumed;
        size_t delta_compressed_size = zlib_size - ofs_consumed;

        void *delta_decompressed = NULL;
        size_t delta_size = 0;
        status = avc_decompress(delta_start, delta_compressed_size,
                                &delta_decompressed, &delta_size, error);
        if (status != AVC_OK) {
            free(base_data);
            return status;
        }

        void *result = NULL;
        size_t result_size = 0;
        status = resolve_delta((const unsigned char *)delta_decompressed,
                               delta_size,
                               (const unsigned char *)base_data, base_size,
                               &result, &result_size, error);
        free(delta_decompressed);
        free(base_data);
        if (status != AVC_OK) return status;

        *type = base_type;
        *payload = result;
        *payload_size = result_size;
        return AVC_OK;
    }

    case AVC_GIT_OBJ_REF_DELTA: {
        if (zlib_size < 20) {
            avc_error_set(error, AVC_ERR_PARSE, "truncated ref_delta header");
            return AVC_ERR_PARSE;
        }

        avc_oid base_oid;
        memcpy(base_oid, zlib_data, 20);

        unsigned char *delta_start = zlib_data + 20;
        size_t delta_compressed_size = zlib_size - 20;

        uint32_t base_idx;
        avc_status status = avc_git_pack_find_object(pack, base_oid, &base_idx, error);
        if (status != AVC_OK) return status;

        avc_git_pack_object_type base_type;
        void *base_data = NULL;
        size_t base_size = 0;
        status = read_decompressed_object(pack,
                                          pack->entries[base_idx].offset,
                                          &base_type, &base_data,
                                          &base_size, error);
        if (status != AVC_OK) return status;

        void *delta_decompressed = NULL;
        size_t delta_size = 0;
        status = avc_decompress(delta_start, delta_compressed_size,
                                &delta_decompressed, &delta_size, error);
        if (status != AVC_OK) {
            free(base_data);
            return status;
        }

        void *result = NULL;
        size_t result_size = 0;
        status = resolve_delta((const unsigned char *)delta_decompressed,
                               delta_size,
                               (const unsigned char *)base_data, base_size,
                               &result, &result_size, error);
        free(delta_decompressed);
        free(base_data);
        if (status != AVC_OK) return status;

        *type = base_type;
        *payload = result;
        *payload_size = result_size;
        return AVC_OK;
    }

    default:
        avc_error_setf(error, AVC_ERR_PARSE, "unknown pack object type: %d", obj_type);
        return AVC_ERR_PARSE;
    }
}

avc_status avc_git_pack_find_object(const avc_git_pack *pack,
                                    const avc_oid oid,
                                    uint32_t *index, avc_error *error) {
    if (pack == NULL || oid == NULL || index == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "find_object received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    int lo = 0;
    int hi = (int)pack->object_count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = oid_cmp(oid, pack->entries[mid].oid);
        if (cmp == 0) {
            *index = (uint32_t)mid;
            return AVC_OK;
        } else if (cmp < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    avc_error_set(error, AVC_ERR_NOT_FOUND, "object not found in pack");
    return AVC_ERR_NOT_FOUND;
}

avc_status avc_git_pack_read_object(const avc_git_pack *pack,
                                    uint32_t index,
                                    avc_git_pack_object_type *type,
                                    void **payload, size_t *payload_size,
                                    avc_error *error) {
    if (pack == NULL || type == NULL || payload == NULL ||
        payload_size == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "read_object received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    if (index >= pack->object_count) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "object index out of range");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    avc_git_pack_object_type obj_type;
    avc_status status = read_decompressed_object(pack,
                                                  pack->entries[index].offset,
                                                  &obj_type,
                                                  payload, payload_size,
                                                  error);
    if (status == AVC_OK) {
        *type = obj_type;
    }
    return status;
}
