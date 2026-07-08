#define _POSIX_C_SOURCE 200809L

#include "index/avc_index.h"

#include "hashing/avc_hash.h"
#include "utils/avc_fs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void avc_index_init(avc_index *index) {
    if (index == NULL) return;
    index->entries = NULL;
    index->count = 0;
    index->capacity = 0;
    index->modified = 0;
}

void avc_index_free(avc_index *index) {
    if (index == NULL) return;
    for (size_t i = 0; i < index->count; i++) {
        free(index->entries[i].path);
    }
    free(index->entries);
    avc_index_init(index);
}

static size_t search_entry(const avc_index *index, const char *path) {
    size_t lo = 0, hi = index->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = strcmp(index->entries[mid].path, path);
        if (c < 0) {
            lo = mid + 1;
        } else if (c > 0) {
            hi = mid;
        } else {
            return mid;
        }
    }
    return lo;
}

avc_index_entry *avc_index_find(const avc_index *index, const char *path) {
    size_t i = search_entry(index, path);
    if (i < index->count && strcmp(index->entries[i].path, path) == 0) {
        return &index->entries[i];
    }
    return NULL;
}

avc_status avc_index_load(avc_index *index, const char *path,
                          avc_error *error) {
    avc_index_init(index);

    void *data = NULL;
    size_t size = 0;
    avc_status status = avc_fs_read_file(path, (char **)&data, &size, error);
    if (status != AVC_OK) {
        if (status == AVC_ERR_NOT_FOUND) {
            index->modified = 1;
            return AVC_OK;
        }
        return status;
    }

    unsigned char *buf = (unsigned char *)data;
    size_t remaining = size;

    if (remaining < 12) {
        free(data);
        avc_error_set(error, AVC_ERR_PARSE, "index file too small");
        return AVC_ERR_PARSE;
    }

    uint32_t signature = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                         ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
    if (signature != AVC_INDEX_SIGNATURE) {
        free(data);
        avc_error_set(error, AVC_ERR_PARSE, "bad index signature");
        return AVC_ERR_PARSE;
    }

    uint32_t version = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                       ((uint32_t)buf[6] << 8) | (uint32_t)buf[7];
    if (version != 2 && version != 3) {
        free(data);
        avc_error_setf(error, AVC_ERR_UNSUPPORTED,
                       "unsupported index version %u", version);
        return AVC_ERR_UNSUPPORTED;
    }

    uint32_t entry_count = ((uint32_t)buf[8] << 24) |
                           ((uint32_t)buf[9] << 16) |
                           ((uint32_t)buf[10] << 8) | (uint32_t)buf[11];

    remaining -= 12;
    buf += 12;

    for (uint32_t i = 0; i < entry_count; i++) {
        if (remaining < 62) {
            free(data);
            avc_error_set(error, AVC_ERR_PARSE,
                          "truncated index entry");
            return AVC_ERR_PARSE;
        }

        avc_index_entry entry;
        memset(&entry, 0, sizeof(entry));

        entry.ctime_sec = ((uint32_t)buf[0] << 24) |
                          ((uint32_t)buf[1] << 16) |
                          ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
        entry.mtime_sec = ((uint32_t)buf[8] << 24) |
                          ((uint32_t)buf[9] << 16) |
                          ((uint32_t)buf[10] << 8) | (uint32_t)buf[11];
        entry.dev = ((uint32_t)buf[16] << 24) |
                    ((uint32_t)buf[17] << 16) |
                    ((uint32_t)buf[18] << 8) | (uint32_t)buf[19];
        entry.ino = ((uint32_t)buf[20] << 24) |
                    ((uint32_t)buf[21] << 16) |
                    ((uint32_t)buf[22] << 8) | (uint32_t)buf[23];
        entry.mode = ((uint32_t)buf[24] << 24) |
                     ((uint32_t)buf[25] << 16) |
                     ((uint32_t)buf[26] << 8) | (uint32_t)buf[27];
        entry.uid = ((uint32_t)buf[28] << 24) |
                    ((uint32_t)buf[29] << 16) |
                    ((uint32_t)buf[30] << 8) | (uint32_t)buf[31];
        entry.gid = ((uint32_t)buf[32] << 24) |
                    ((uint32_t)buf[33] << 16) |
                    ((uint32_t)buf[34] << 8) | (uint32_t)buf[35];
        entry.size = ((uint32_t)buf[36] << 24) |
                     ((uint32_t)buf[37] << 16) |
                     ((uint32_t)buf[38] << 8) | (uint32_t)buf[39];
        memcpy(entry.oid, buf + 40, 20);

        uint16_t flags = ((uint16_t)buf[60] << 8) | (uint16_t)buf[61];
        entry.flags_valid = (flags & 0x8000) ? 1 : 0;
        uint16_t path_len = flags & 0x0FFF;

        buf += 62;
        remaining -= 62;

        if (path_len >= 0x0FFF) {
            free(data);
            avc_error_set(error, AVC_ERR_UNSUPPORTED,
                          "long index paths not supported");
            return AVC_ERR_UNSUPPORTED;
        }

        if (remaining < path_len) {
            free(data);
            avc_error_set(error, AVC_ERR_PARSE,
                          "truncated index path");
            return AVC_ERR_PARSE;
        }

        entry.path = (char *)malloc((size_t)path_len + 1);
        if (entry.path == NULL) {
            free(data);
            avc_error_set(error, AVC_ERR_NO_MEMORY,
                          "out of memory reading index");
            return AVC_ERR_NO_MEMORY;
        }
        memcpy(entry.path, buf, path_len);
        entry.path[path_len] = '\0';

        size_t entry_size = (size_t)path_len;
        size_t padding = (8 - ((62 + entry_size) % 8)) % 8;
        buf += entry_size + padding;
        remaining -= entry_size + padding;

        status = avc_index_add(index, entry.path, entry.mode,
                                entry.oid, error);
        free(entry.path);
        if (status != AVC_OK) {
            free(data);
            return status;
        }
    }

    free(data);
    return AVC_OK;
}

static uint32_t write_be32(uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v >> 24);
    b[1] = (unsigned char)(v >> 16);
    b[2] = (unsigned char)(v >> 8);
    b[3] = (unsigned char)(v);
    return *(uint32_t *)b;
}

avc_status avc_index_write(avc_index *index, const char *path,
                           avc_error *error) {
    if (index == NULL || path == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "index write requires path");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    size_t entry_size_sum = 0;
    for (size_t i = 0; i < index->count; i++) {
        size_t path_len = strlen(index->entries[i].path) + 1;
        size_t this_entry = 62 + path_len;
        size_t padding = (8 - (this_entry % 8)) % 8;
        entry_size_sum += this_entry + padding;
    }

    size_t total = 12 + entry_size_sum + 20;
    unsigned char *buf = (unsigned char *)malloc(total);
    if (buf == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory writing index");
        return AVC_ERR_NO_MEMORY;
    }

    unsigned char *p = buf;

    *p++ = 'D'; *p++ = 'I'; *p++ = 'R'; *p++ = 'C';

    uint32_t net_version = write_be32(2);
    memcpy(p, &net_version, 4); p += 4;

    uint32_t net_count = write_be32((uint32_t)index->count);
    memcpy(p, &net_count, 4); p += 4;

    for (size_t i = 0; i < index->count; i++) {
        const avc_index_entry *e = &index->entries[i];

        uint32_t v;

        v = write_be32(e->ctime_sec); memcpy(p, &v, 4); p += 4;
        v = 0; memcpy(p, &v, 4); p += 4;

        v = write_be32(e->mtime_sec); memcpy(p, &v, 4); p += 4;
        v = 0; memcpy(p, &v, 4); p += 4;

        v = write_be32(e->dev);  memcpy(p, &v, 4); p += 4;
        v = write_be32(e->ino);  memcpy(p, &v, 4); p += 4;
        v = write_be32(e->mode); memcpy(p, &v, 4); p += 4;
        v = write_be32(e->uid);  memcpy(p, &v, 4); p += 4;
        v = write_be32(e->gid);  memcpy(p, &v, 4); p += 4;
        v = write_be32(e->size); memcpy(p, &v, 4); p += 4;

        memcpy(p, e->oid, 20); p += 20;

        size_t path_len = strlen(e->path) + 1;
        uint16_t flags = (uint16_t)(path_len & 0x0FFF);
        if (e->flags_valid) {
            flags |= 0x8000;
        }
        *p++ = (unsigned char)(flags >> 8);
        *p++ = (unsigned char)(flags);

        memcpy(p, e->path, path_len); p += path_len;

        size_t this_entry = 62 + path_len;
        size_t padding = (8 - (this_entry % 8)) % 8;
        memset(p, 0, padding); p += padding;
    }

    size_t before_checksum = (size_t)(p - buf);

    avc_sha1_ctx ctx;
    avc_sha1_init(&ctx);
    avc_sha1_update(&ctx, buf, before_checksum);
    avc_sha1_final(&ctx, (unsigned char *)p);
    p += 20;

    avc_status status = avc_fs_write_file(path, buf, (size_t)(p - buf),
                                           error);
    free(buf);
    if (status == AVC_OK) {
        index->modified = 0;
    }
    return status;
}

avc_status avc_index_add(avc_index *index, const char *path,
                         uint32_t mode, const avc_oid oid,
                         avc_error *error) {
    if (index == NULL || path == NULL || path[0] == '\0') {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "index add requires path");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    size_t pos = search_entry(index, path);
    if (pos < index->count && strcmp(index->entries[pos].path, path) == 0) {
        memcpy(index->entries[pos].oid, oid, 20);
        index->entries[pos].mode = mode;
        index->modified = 1;
        return AVC_OK;
    }

    if (index->count == index->capacity) {
        size_t next_cap = index->capacity == 0 ? 16 : index->capacity * 2;
        avc_index_entry *entries = (avc_index_entry *)realloc(
            index->entries, next_cap * sizeof(avc_index_entry));
        if (entries == NULL) {
            avc_error_set(error, AVC_ERR_NO_MEMORY,
                          "out of memory growing index");
            return AVC_ERR_NO_MEMORY;
        }
        index->entries = entries;
        index->capacity = next_cap;
    }

    if (pos < index->count) {
        memmove(&index->entries[pos + 1], &index->entries[pos],
                (index->count - pos) * sizeof(avc_index_entry));
    }

    char *copy = strdup(path);
    if (copy == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory adding index entry");
        return AVC_ERR_NO_MEMORY;
    }

    avc_index_entry *entry = &index->entries[pos];
    memset(entry, 0, sizeof(*entry));
    entry->path = copy;
    entry->mode = mode;
    memcpy(entry->oid, oid, 20);
    index->count++;
    index->modified = 1;
    return AVC_OK;
}

avc_status avc_index_remove(avc_index *index, const char *path,
                            avc_error *error) {
    size_t pos = search_entry(index, path);
    if (pos >= index->count || strcmp(index->entries[pos].path, path) != 0) {
        avc_error_set(error, AVC_ERR_NOT_FOUND, "path not in index");
        return AVC_ERR_NOT_FOUND;
    }

    free(index->entries[pos].path);
    if (pos + 1 < index->count) {
        memmove(&index->entries[pos], &index->entries[pos + 1],
                (index->count - pos - 1) * sizeof(avc_index_entry));
    }
    index->count--;
    index->modified = 1;
    return AVC_OK;
}
