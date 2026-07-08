#define _POSIX_C_SOURCE 200809L

#include "packfiles/avc_pack.h"

#include "compression/avc_compress.h"
#include "hashing/avc_hash.h"
#include "utils/avc_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_be32(unsigned char *buf, uint32_t val) {
    buf[0] = (unsigned char)(val >> 24);
    buf[1] = (unsigned char)(val >> 16);
    buf[2] = (unsigned char)(val >> 8);
    buf[3] = (unsigned char)(val);
    return 4;
}

static uint32_t read_be32(const unsigned char *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           (uint32_t)buf[3];
}

static int write_be64(unsigned char *buf, uint64_t val) {
    for (int i = 7; i >= 0; i--) {
        buf[i] = (unsigned char)(val & 0xff);
        val >>= 8;
    }
    return 8;
}

static uint64_t read_be64(const unsigned char *buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val = (val << 8) | buf[i];
    }
    return val;
}

static int write_varint(unsigned char *buf, size_t val) {
    int i = 0;
    while (val > 0x7f) {
        buf[i++] = (unsigned char)((val & 0x7f) | 0x80);
        val >>= 7;
    }
    buf[i++] = (unsigned char)(val & 0x7f);
    return i;
}

static int read_varint(const unsigned char *buf, size_t *out) {
    size_t val = 0;
    int shift = 0, i = 0;
    unsigned char b;
    do {
        b = buf[i++];
        val |= (size_t)(b & 0x7f) << shift;
        shift += 7;
    } while (b & 0x80);
    *out = val;
    return i;
}

typedef struct pack_idx_entry {
    avc_oid oid;
    uint64_t offset;
} pack_idx_entry;

static int idx_entry_cmp(const void *a, const void *b) {
    return memcmp(((const pack_idx_entry *)a)->oid,
                  ((const pack_idx_entry *)b)->oid, 20);
}

avc_status avc_pack_write(avc_odb *odb,
                          const avc_oid *oids, int oid_count,
                          const char *pack_path, const char *idx_path,
                          avc_error *error) {
    if (odb == NULL || oids == NULL || oid_count <= 0 ||
        pack_path == NULL || idx_path == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "invalid arguments");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    size_t pack_cap = 65536;
    size_t pack_len = 0;
    unsigned char *pack_buf = (unsigned char *)malloc(pack_cap);
    if (pack_buf == NULL) return AVC_ERR_NO_MEMORY;

    pack_idx_entry *idx_entries = (pack_idx_entry *)malloc(
        (size_t)oid_count * sizeof(pack_idx_entry));
    if (idx_entries == NULL) { free(pack_buf); return AVC_ERR_NO_MEMORY; }

    memcpy(pack_buf, AVC_PACK_SIGNATURE, 7);
    pack_len += 7;
    pack_buf[pack_len++] = (unsigned char)AVC_PACK_VERSION;
    pack_len += (size_t)write_be32(pack_buf + pack_len, (uint32_t)oid_count);

    for (int i = 0; i < oid_count; i++) {
        memcpy(idx_entries[i].oid, oids[i], 20);
        idx_entries[i].offset = pack_len;

        void *payload = NULL;
        size_t payload_size = 0;
        avc_object_type type;
        avc_status status = avc_odb_read(odb, oids[i], &type, &payload,
                                          &payload_size, error);
        if (status != AVC_OK) {
            free(pack_buf);
            free(idx_entries);
            return status;
        }

        unsigned char type_byte;
        switch (type) {
        case AVC_OBJECT_COMMIT: type_byte = 0; break;
        case AVC_OBJECT_TREE:   type_byte = 1; break;
        case AVC_OBJECT_BLOB:   type_byte = 2; break;
        case AVC_OBJECT_TAG:    type_byte = 3; break;
        default:
            free(payload);
            free(pack_buf);
            free(idx_entries);
            return AVC_ERR_PARSE;
        }

        if (pack_len + 64 > pack_cap) {
            pack_cap *= 2;
            unsigned char *nb = (unsigned char *)realloc(pack_buf, pack_cap);
            if (nb == NULL) { free(payload); free(pack_buf); free(idx_entries); return AVC_ERR_NO_MEMORY; }
            pack_buf = nb;
        }

        pack_buf[pack_len++] = type_byte;
        pack_len += (size_t)write_varint(pack_buf + pack_len, payload_size);

        void *compressed = NULL;
        size_t compressed_size = 0;
        status = avc_compress((const unsigned char *)payload, payload_size,
                               &compressed, &compressed_size, error);
        free(payload);
        if (status != AVC_OK) {
            free(pack_buf);
            free(idx_entries);
            return status;
        }

        while (pack_len + compressed_size + 64 > pack_cap) {
            pack_cap *= 2;
            unsigned char *nb = (unsigned char *)realloc(pack_buf, pack_cap);
            if (nb == NULL) {
                free(compressed);
                free(pack_buf);
                free(idx_entries);
                return AVC_ERR_NO_MEMORY;
            }
            pack_buf = nb;
        }

        memcpy(pack_buf + pack_len, compressed, compressed_size);
        pack_len += compressed_size;
        free(compressed);
    }

    avc_sha1_ctx ctx;
    avc_sha1_init(&ctx);
    avc_sha1_update(&ctx, pack_buf, pack_len);
    avc_sha1_final(&ctx, pack_buf + pack_len);
    pack_len += 20;

    avc_status status = avc_fs_write_file(pack_path, (const char *)pack_buf,
                                           pack_len, error);
    if (status != AVC_OK) {
        free(pack_buf);
        free(idx_entries);
        return status;
    }

    qsort(idx_entries, (size_t)oid_count, sizeof(pack_idx_entry),
          idx_entry_cmp);

    size_t idx_cap = 4096;
    size_t idx_len = 0;
    unsigned char *idx_buf = (unsigned char *)malloc(idx_cap);
    if (idx_buf == NULL) { free(pack_buf); free(idx_entries); return AVC_ERR_NO_MEMORY; }

    memcpy(idx_buf, "AVCPACKidx1", 11);
    idx_len += 11;
    idx_len += (size_t)write_be32(idx_buf + idx_len, (uint32_t)oid_count);

    for (int i = 0; i < oid_count; i++) {
        while (idx_len + 28 > idx_cap) {
            idx_cap *= 2;
            unsigned char *nb = (unsigned char *)realloc(idx_buf, idx_cap);
            if (nb == NULL) { free(pack_buf); free(idx_buf); free(idx_entries); return AVC_ERR_NO_MEMORY; }
            idx_buf = nb;
        }
        memcpy(idx_buf + idx_len, idx_entries[i].oid, 20);
        idx_len += 20;
        idx_len += (size_t)write_be64(idx_buf + idx_len, idx_entries[i].offset);
    }

    avc_sha1_init(&ctx);
    avc_sha1_update(&ctx, idx_buf, idx_len);
    avc_sha1_final(&ctx, idx_buf + idx_len);
    idx_len += 20;

    status = avc_fs_write_file(idx_path, (const char *)idx_buf,
                                idx_len, error);
    free(pack_buf);
    free(idx_buf);
    free(idx_entries);
    return status;
}

avc_status avc_pack_read(avc_odb *odb,
                         const char *pack_path, const char *idx_path,
                         avc_error *error) {
    if (odb == NULL || pack_path == NULL || idx_path == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "invalid arguments");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *idx_data = NULL;
    size_t idx_size = 0;
    avc_status status = avc_fs_read_file(idx_path, &idx_data, &idx_size,
                                          error);
    if (status != AVC_OK) return status;

    if (idx_size < 15 || memcmp(idx_data, "AVCPACKidx1", 11) != 0) {
        free(idx_data);
        avc_error_set(error, AVC_ERR_PARSE, "invalid pack index");
        return AVC_ERR_PARSE;
    }

    int count = (int)read_be32((const unsigned char *)idx_data + 11);
    if (count <= 0) {
        free(idx_data);
        return AVC_OK;
    }

    size_t entry_size = 28;
    size_t idx_entry_start = 15;

    char *pack_data = NULL;
    size_t pack_size = 0;
    status = avc_fs_read_file(pack_path, &pack_data, &pack_size, error);
    if (status != AVC_OK) { free(idx_data); return status; }

    if (pack_size < 12 ||
        memcmp(pack_data, AVC_PACK_SIGNATURE, 7) != 0) {
        free(idx_data);
        free(pack_data);
        avc_error_set(error, AVC_ERR_PARSE, "invalid pack file");
        return AVC_ERR_PARSE;
    }

    for (int i = 0; i < count; i++) {
        size_t off = idx_entry_start + (size_t)i * entry_size;
        if (off + 28 > idx_size) {
            free(idx_data); free(pack_data);
            avc_error_set(error, AVC_ERR_PARSE, "truncated index");
            return AVC_ERR_PARSE;
        }

        const unsigned char *oid_data = (const unsigned char *)idx_data + off;
        uint64_t pack_offset = read_be64((const unsigned char *)idx_data + off + 20);

        if (pack_offset + 2 > pack_size) {
            free(idx_data); free(pack_data);
            avc_error_set(error, AVC_ERR_PARSE, "truncated pack");
            return AVC_ERR_PARSE;
        }

        avc_oid oid;
        memcpy(oid, oid_data, 20);

        if (avc_odb_exists(odb, oid)) continue;

        const unsigned char *p = (const unsigned char *)pack_data + pack_offset;
        unsigned char type_byte = *p++;
        avc_object_type type;
        switch (type_byte) {
        case 0: type = AVC_OBJECT_COMMIT; break;
        case 1: type = AVC_OBJECT_TREE;   break;
        case 2: type = AVC_OBJECT_BLOB;   break;
        case 3: type = AVC_OBJECT_TAG;    break;
        default:
            free(idx_data); free(pack_data);
            avc_error_set(error, AVC_ERR_PARSE, "unknown pack object type");
            return AVC_ERR_PARSE;
        }

        size_t payload_size = 0;
        int vi = read_varint(p, &payload_size);
        p += vi;

        size_t compressed_size = pack_size - (size_t)(p - (const unsigned char *)pack_data) - 20;
        void *decompressed = NULL;
        size_t decompressed_size = 0;
        status = avc_decompress(p, compressed_size, &decompressed,
                                 &decompressed_size, error);
        if (status != AVC_OK) {
            free(idx_data); free(pack_data);
            return status;
        }

        avc_odb_write(odb, type, decompressed, decompressed_size,
                      oid, error);
        free(decompressed);
    }

    free(idx_data);
    free(pack_data);
    return AVC_OK;
}
