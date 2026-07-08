#define _POSIX_C_SOURCE 200809L

#include "diff/avc_diff.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct line_range {
    const char **lines;
    int count;
} line_range;

static line_range split_lines(const unsigned char *data, size_t size) {
    line_range r;
    r.lines = NULL;
    r.count = 0;

    if (size == 0) return r;

    int cap = 64;
    r.lines = (const char **)malloc((size_t)cap * sizeof(const char *));
    if (r.lines == NULL) return r;

    size_t pos = 0;
    while (pos < size) {
        if (r.count >= cap) {
            cap *= 2;
            const char **new_lines = (const char **)realloc(
                r.lines, (size_t)cap * sizeof(const char *));
            if (new_lines == NULL) {
                free(r.lines);
                r.lines = NULL;
                r.count = 0;
                return r;
            }
            r.lines = new_lines;
        }
        r.lines[r.count++] = (const char *)(data + pos);
        while (pos < size && data[pos] != '\n') pos++;
        if (pos < size) pos++;
    }

    return r;
}

static int line_len(const char *s) {
    const char *nl = strchr(s, '\n');
    return nl ? (int)(nl - s) : (int)strlen(s);
}

static void append(char **buf, size_t *cap, size_t *len,
                   const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) return;

    size_t n = (size_t)needed;
    if (*len + n + 1 > *cap) {
        while (*len + n + 1 > *cap) {
            *cap = *cap == 0 ? 4096 : *cap * 2;
        }
        char *new_buf = (char *)realloc(*buf, *cap);
        if (new_buf == NULL) return;
        *buf = new_buf;
    }

    va_start(ap, fmt);
    vsnprintf(*buf + *len, *cap - *len, fmt, ap);
    va_end(ap);
    *len += n;
}

static void emit_hunk(char **buf, size_t *cap, size_t *len,
                      int a_start, int a_count,
                      int b_start, int b_count,
                      const line_range *a, const line_range *b,
                      int *edits, int edit_count, int offset) {
    append(buf, cap, len,
           "@@ -%d,%d +%d,%d @@\n",
           a_start + 1, a_count, b_start + 1, b_count);

    for (int i = 0; i < edit_count; i++) {
        int idx = offset + i;
        int l = line_len(edits[idx] >= 0 ?
                         b->lines[edits[idx]] :
                         a->lines[~edits[idx]]);
        if (edits[idx] >= 0) {
            append(buf, cap, len, "+%.*s\n", l, b->lines[edits[idx]]);
        } else {
            append(buf, cap, len, "-%.*s\n", l, a->lines[~edits[idx]]);
        }
    }
}

char *avc_diff_blobs(const unsigned char *a, size_t a_size,
                     const unsigned char *b, size_t b_size,
                     const char *a_path, const char *b_path) {
    line_range la = split_lines(a, a_size);
    line_range lb = split_lines(b, b_size);

    if (la.lines == NULL && a_size > 0) return NULL;
    if (lb.lines == NULL && b_size > 0) { free(la.lines); return NULL; }

    int M = la.count;
    int N = lb.count;

    int **lcs = (int **)calloc((size_t)(M + 1), sizeof(int *));
    if (lcs == NULL) { free(la.lines); free(lb.lines); return NULL; }
    for (int i = 0; i <= M; i++) {
        lcs[i] = (int *)calloc((size_t)(N + 1), sizeof(int));
        if (lcs[i] == NULL) {
            for (int j = 0; j < i; j++) free(lcs[j]);
            free(lcs);
            free(la.lines); free(lb.lines);
            return NULL;
        }
    }

    for (int i = 1; i <= M; i++) {
        int la_len = line_len(la.lines[i - 1]);
        for (int j = 1; j <= N; j++) {
            int lb_len = line_len(lb.lines[j - 1]);
            if (la_len == lb_len &&
                memcmp(la.lines[i - 1], lb.lines[j - 1],
                       (size_t)la_len) == 0) {
                lcs[i][j] = lcs[i - 1][j - 1] + 1;
            } else if (lcs[i - 1][j] >= lcs[i][j - 1]) {
                lcs[i][j] = lcs[i - 1][j];
            } else {
                lcs[i][j] = lcs[i][j - 1];
            }
        }
    }

    int edit_cap = M + N + 2;
    int *edits = (int *)malloc((size_t)edit_cap * sizeof(int));
    if (edits == NULL) {
        for (int i = 0; i <= M; i++) free(lcs[i]);
        free(lcs); free(la.lines); free(lb.lines);
        return NULL;
    }
    int edit_count = 0;

    {
        int i = M, j = N;
        while (i > 0 || j > 0) {
            if (i > 0 && j > 0 &&
                line_len(la.lines[i - 1]) == line_len(lb.lines[j - 1]) &&
                memcmp(la.lines[i - 1], lb.lines[j - 1],
                       (size_t)line_len(la.lines[i - 1])) == 0) {
                i--; j--;
            } else if (j > 0 && (i == 0 || lcs[i][j - 1] >= lcs[i - 1][j])) {
                edits[edit_count++] = j - 1;
                j--;
            } else {
                edits[edit_count++] = ~(i - 1);
                i--;
            }
        }
    }

    for (int i = 0; i <= M; i++) free(lcs[i]);
    free(lcs);

    for (int i = 0; i < edit_count / 2; i++) {
        int tmp = edits[i];
        edits[i] = edits[edit_count - 1 - i];
        edits[edit_count - 1 - i] = tmp;
    }

    char *buf = NULL;
    size_t cap = 0, len = 0;

    append(&buf, &cap, &len, "--- %s\n+++ %s\n",
           a_path ? a_path : "a", b_path ? b_path : "b");

    int ai = 0, bi = 0, ei = 0;
    while (ei < edit_count) {
        int hunk_start_a = ai;
        int hunk_start_b = bi;
        int hunk_ei = ei;

        while (ei < edit_count && edit_count - ei > 0) {
            int e = edits[ei];
            if (e >= 0) { bi++; ei++; }
            else { ai++; ei++; }

            int ctx = 0;
            while (ei < edit_count) {
                e = edits[ei];
                if (e >= 0) { bi++; ei++; }
                else { ai++; ei++; }
                ctx++;
                if (ctx >= 3) break;
            }
        }

        int a_count = ai - hunk_start_a;
        int b_count = bi - hunk_start_b;
        int edit_in_hunk = ei - hunk_ei;

        emit_hunk(&buf, &cap, &len,
                  hunk_start_a, a_count,
                  hunk_start_b, b_count,
                  &la, &lb, edits, edit_in_hunk, hunk_ei);
    }

    free(edits);
    free(la.lines);
    free(lb.lines);

    if (buf != NULL) {
        buf[len] = '\0';
    }
    return buf;
}

static int tree_entry_name_cmp(const void *a, const void *b) {
    return strcmp(((const avc_diff_file *)a)->path,
                  ((const avc_diff_file *)b)->path);
}

static int parse_tree_flat(avc_odb *odb, const avc_oid tree_oid,
                           avc_diff_file **files, int *count,
                           avc_error *error) {
    if (avc_oid_is_zero(tree_oid)) {
        *files = NULL;
        *count = 0;
        return AVC_OK;
    }

    void *payload = NULL;
    size_t size = 0;
    avc_object_type type;
    avc_status status = avc_odb_read(odb, tree_oid, &type, &payload,
                                      &size, error);
    if (status != AVC_OK) return status;
    if (type != AVC_OBJECT_TREE) {
        free(payload);
        return AVC_ERR_PARSE;
    }

    int cap = 64, cnt = 0;
    avc_diff_file *list = (avc_diff_file *)malloc(
        (size_t)cap * sizeof(avc_diff_file));
    if (list == NULL) { free(payload); return AVC_ERR_NO_MEMORY; }

    const unsigned char *p = (const unsigned char *)payload;
    const unsigned char *end = p + size;
    while (p < end) {
        while (p < end && *p != ' ') p++;
        if (p >= end) break;
        p++;
        const unsigned char *name_start = p;
        while (p < end && *p != '\0') p++;
        if (p >= end) break;

        size_t name_len = (size_t)(p - name_start);
        p++;
        p += 20;

        char *slash = (char *)memchr(name_start, '/', name_len);
        if (slash != NULL) continue;

        if (cnt >= cap) {
            cap *= 2;
            avc_diff_file *nl = (avc_diff_file *)realloc(
                list, (size_t)cap * sizeof(avc_diff_file));
            if (nl == NULL) {
                for (int i = 0; i < cnt; i++) free(list[i].path);
                free(list); free(payload);
                return AVC_ERR_NO_MEMORY;
            }
            list = nl;
        }

        list[cnt].path = (char *)malloc(name_len + 1);
        if (list[cnt].path == NULL) {
            for (int i = 0; i < cnt; i++) free(list[i].path);
            free(list); free(payload);
            return AVC_ERR_NO_MEMORY;
        }
        memcpy(list[cnt].path, name_start, name_len);
        list[cnt].path[name_len] = '\0';
        list[cnt].status = AVC_DIFF_UNCHANGED;
        cnt++;
    }

    free(payload);
    qsort(list, (size_t)cnt, sizeof(avc_diff_file),
          tree_entry_name_cmp);
    *files = list;
    *count = cnt;
    return AVC_OK;
}

avc_status avc_diff_trees(avc_odb *odb,
                          const avc_oid a_tree, const avc_oid b_tree,
                          avc_diff_file **files, int *file_count,
                          avc_error *error) {
    avc_diff_file *a_files = NULL, *b_files = NULL;
    int a_cnt = 0, b_cnt = 0;

    avc_status status = parse_tree_flat(odb, a_tree, &a_files, &a_cnt, error);
    if (status != AVC_OK) return status;
    status = parse_tree_flat(odb, b_tree, &b_files, &b_cnt, error);
    if (status != AVC_OK) { free(a_files); return status; }

    int max = a_cnt + b_cnt;
    avc_diff_file *result = (avc_diff_file *)malloc(
        (size_t)max * sizeof(avc_diff_file));
    if (result == NULL) {
        free(a_files); free(b_files);
        return AVC_ERR_NO_MEMORY;
    }

    int ri = 0, ai = 0, bi = 0;
    while (ai < a_cnt || bi < b_cnt) {
        const char *na = ai < a_cnt ? a_files[ai].path : NULL;
        const char *nb = bi < b_cnt ? b_files[bi].path : NULL;
        int cmp = 0;
        if (na == NULL) cmp = 1;
        else if (nb == NULL) cmp = -1;
        else cmp = strcmp(na, nb);

        if (cmp == 0) {
            result[ri].path = strdup(na);
            result[ri].status = AVC_DIFF_UNCHANGED;
            ai++; bi++;
        } else if (cmp < 0) {
            result[ri].path = strdup(na);
            result[ri].status = AVC_DIFF_DELETED;
            ai++;
        } else {
            result[ri].path = strdup(nb);
            result[ri].status = AVC_DIFF_ADDED;
            bi++;
        }
        if (result[ri].path == NULL) {
            for (int j = 0; j < ri; j++) free(result[j].path);
            free(result); free(a_files); free(b_files);
            return AVC_ERR_NO_MEMORY;
        }
        ri++;
    }

    free(a_files);
    free(b_files);
    *files = result;
    *file_count = ri;
    return AVC_OK;
}

void avc_diff_files_free(avc_diff_file *files, int count) {
    if (files == NULL) return;
    for (int i = 0; i < count; i++) free(files[i].path);
    free(files);
}
