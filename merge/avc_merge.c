#define _POSIX_C_SOURCE 200809L

#include "merge/avc_merge.h"

#include "commits/avc_commit.h"
#include "hashing/avc_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct tree_entry {
    char *name;
    avc_oid oid;
    uint32_t mode;
} tree_entry;

static void tree_entry_free(tree_entry *e) {
    free(e->name);
}

static int tree_entry_cmp(const void *a, const void *b) {
    return strcmp(((const tree_entry *)a)->name,
                  ((const tree_entry *)b)->name);
}

static avc_status parse_tree(avc_odb *odb, const avc_oid tree_oid,
                              tree_entry **entries, int *count,
                              avc_error *error) {
    if (avc_oid_is_zero(tree_oid)) {
        *entries = NULL;
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
        avc_error_set(error, AVC_ERR_PARSE, "not a tree object");
        return AVC_ERR_PARSE;
    }

    int cap = 16;
    int cnt = 0;
    tree_entry *list = (tree_entry *)malloc(
        (size_t)cap * sizeof(tree_entry));
    if (list == NULL) {
        free(payload);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    const unsigned char *p = (const unsigned char *)payload;
    const unsigned char *end = p + size;
    while (p < end) {
        const unsigned char *mode_start = p;
        while (p < end && *p != ' ') p++;
        if (p >= end) break;
        const unsigned char *name_start = p + 1;
        while (p < end && *p != '\0') p++;
        if (p >= end) break;

        if (cnt >= cap) {
            cap *= 2;
            tree_entry *new_list = (tree_entry *)realloc(
                list, (size_t)cap * sizeof(tree_entry));
            if (new_list == NULL) {
                for (int i = 0; i < cnt; i++) tree_entry_free(&list[i]);
                free(list);
                free(payload);
                avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
                return AVC_ERR_NO_MEMORY;
            }
            list = new_list;
        }

        tree_entry *e = &list[cnt];
        char mode_buf[16];
        size_t mode_len = (size_t)((const char *)name_start -
                                    (const char *)mode_start - 1);
        if (mode_len >= sizeof(mode_buf)) mode_len = sizeof(mode_buf) - 1;
        memcpy(mode_buf, mode_start, mode_len);
        mode_buf[mode_len] = '\0';
        e->mode = (uint32_t)strtoul(mode_buf, NULL, 8);

        size_t name_len = strlen((const char *)name_start);
        e->name = (char *)malloc(name_len + 1);
        if (e->name == NULL) {
            for (int i = 0; i < cnt; i++) tree_entry_free(&list[i]);
            free(list);
            free(payload);
            avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
            return AVC_ERR_NO_MEMORY;
        }
        memcpy(e->name, name_start, name_len + 1);

        p++;
        if (p + 20 > end) {
            tree_entry_free(e);
            free(payload);
            avc_error_set(error, AVC_ERR_PARSE, "truncated tree entry");
            return AVC_ERR_PARSE;
        }
        memcpy(e->oid, p, 20);
        p += 20;
        cnt++;
    }

    free(payload);
    qsort(list, (size_t)cnt, sizeof(tree_entry), tree_entry_cmp);
    *entries = list;
    *count = cnt;
    return AVC_OK;
}

static int walk_ancestors(avc_odb *odb, const avc_oid start,
                          avc_oid *ancestors, int max_count,
                          avc_error *error) {
    int count = 0;
    avc_oid current;
    memcpy(current, start, 20);

    while (count < max_count) {
        for (int i = 0; i < count; i++) {
            if (memcmp(ancestors[i], current, 20) == 0) {
                return count;
            }
        }
        memcpy(ancestors[count], current, 20);
        count++;

        void *payload = NULL;
        size_t size = 0;
        avc_object_type type;
        avc_status status = avc_odb_read(odb, current, &type, &payload,
                                          &size, error);
        if (status != AVC_OK || type != AVC_OBJECT_COMMIT) {
            free(payload);
            break;
        }

        avc_oid tree_id, parent_ids[16];
        int parent_cnt = 0;
        avc_commit_parse((const unsigned char *)payload, size,
                          &tree_id, parent_ids, &parent_cnt,
                          NULL, 0, NULL, 0, NULL, 0, error);
        free(payload);

        if (parent_cnt == 0) break;
        if (parent_cnt > 1) {
            for (int j = 1; j < parent_cnt; j++) {
                if (count < max_count) {
                    memcpy(ancestors[count], parent_ids[j], 20);
                    count++;
                }
            }
        }
        memcpy(current, parent_ids[0], 20);
    }
    return count;
}

avc_status avc_merge_find_base(avc_odb *odb, const avc_oid commit_a,
                               const avc_oid commit_b, avc_oid base_out,
                               avc_error *error) {
    avc_oid ancestors_a[4096];
    int count_a = walk_ancestors(odb, commit_a, ancestors_a, 4096, error);

    avc_oid ancestors_b[4096];
    int count_b = walk_ancestors(odb, commit_b, ancestors_b, 4096, error);

    for (int i = 0; i < count_a; i++) {
        for (int j = 0; j < count_b; j++) {
            if (memcmp(ancestors_a[i], ancestors_b[j], 20) == 0) {
                memcpy(base_out, ancestors_a[i], 20);
                return AVC_OK;
            }
        }
    }

    avc_error_set(error, AVC_ERR_NOT_FOUND, "no merge base found");
    return AVC_ERR_NOT_FOUND;
}

int avc_merge_is_ancestor(avc_odb *odb, const avc_oid commit,
                          const avc_oid ancestor, avc_error *error) {
    avc_oid current;
    memcpy(current, commit, 20);

    for (int i = 0; i < 4096; i++) {
        if (memcmp(current, ancestor, 20) == 0) return 1;

        void *payload = NULL;
        size_t size = 0;
        avc_object_type type;
        avc_status status = avc_odb_read(odb, current, &type, &payload,
                                          &size, error);
        if (status != AVC_OK || type != AVC_OBJECT_COMMIT) {
            free(payload);
            return 0;
        }

        avc_oid tree_id, parent_ids[16];
        int parent_cnt = 0;
        avc_commit_parse((const unsigned char *)payload, size,
                          &tree_id, parent_ids, &parent_cnt,
                          NULL, 0, NULL, 0, NULL, 0, error);
        free(payload);

        if (parent_cnt == 0) return 0;
        memcpy(current, parent_ids[0], 20);
    }
    return 0;
}

avc_status avc_merge_fast_forward(avc_odb *odb, const avc_oid head_oid,
                                  const avc_oid branch_oid,
                                  avc_oid merged_out, avc_error *error) {
    if (!avc_merge_is_ancestor(odb, branch_oid, head_oid, error)) {
        avc_error_set(error, AVC_ERR_NOT_FOUND,
                      "not a fast-forward merge");
        return AVC_ERR_NOT_FOUND;
    }
    memcpy(merged_out, branch_oid, 20);
    return AVC_OK;
}

avc_status avc_merge_trees(avc_odb *odb,
                           const avc_oid base_tree,
                           const avc_oid a_tree,
                           const avc_oid b_tree,
                           avc_merge_entry **entries, int *entry_count,
                           avc_error *error) {
    tree_entry *base_entries = NULL;
    tree_entry *a_entries = NULL;
    tree_entry *b_entries = NULL;
    int base_count = 0, a_count = 0, b_count = 0;

    avc_status status = parse_tree(odb, base_tree, &base_entries,
                                    &base_count, error);
    if (status != AVC_OK) return status;
    status = parse_tree(odb, a_tree, &a_entries, &a_count, error);
    if (status != AVC_OK) {
        free(base_entries);
        return status;
    }
    status = parse_tree(odb, b_tree, &b_entries, &b_count, error);
    if (status != AVC_OK) {
        free(base_entries);
        free(a_entries);
        return status;
    }

    int max = a_count + b_count + base_count;
    avc_merge_entry *result = (avc_merge_entry *)calloc(
        (size_t)max, sizeof(avc_merge_entry));
    if (result == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
        return AVC_ERR_NO_MEMORY;
    }

    int ri = 0, ai = 0, bi = 0, base_i = 0;

    while (ai < a_count || bi < b_count || base_i < base_count) {
        const char *name_a = ai < a_count ? a_entries[ai].name : NULL;
        const char *name_b = bi < b_count ? b_entries[bi].name : NULL;
        const char *name_base = base_i < base_count ?
                                base_entries[base_i].name : NULL;

        const char *min = NULL;
        if (name_a != NULL && (min == NULL || strcmp(name_a, min) < 0))
            min = name_a;
        if (name_b != NULL && (min == NULL || strcmp(name_b, min) < 0))
            min = name_b;
        if (name_base != NULL && (min == NULL || strcmp(name_base, min) < 0))
            min = name_base;

        int in_a = name_a != NULL && strcmp(name_a, min) == 0;
        int in_b = name_b != NULL && strcmp(name_b, min) == 0;
        int in_base = name_base != NULL && strcmp(name_base, min) == 0;

        avc_merge_entry *re = &result[ri];
        re->path = strdup(min);
        if (re->path == NULL) {
            avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory");
            return AVC_ERR_NO_MEMORY;
        }

        if (in_a) re->mode_a = a_entries[ai].mode;
        if (in_b) re->mode_b = b_entries[bi].mode;

        if (in_a) memcpy(re->oid_a, a_entries[ai].oid, 20);
        if (in_b) memcpy(re->oid_b, b_entries[bi].oid, 20);

        if (!in_base && in_a && !in_b) {
            re->status = AVC_MERGE_ADDED_A;
        } else if (!in_base && !in_a && in_b) {
            re->status = AVC_MERGE_ADDED_B;
        } else if (in_base && !in_a && !in_b) {
            re->status = AVC_MERGE_DELETED_A;
            memcpy(re->oid_a, base_entries[base_i].oid, 20);
            re->mode_a = base_entries[base_i].mode;
            memcpy(re->oid_b, base_entries[base_i].oid, 20);
            re->mode_b = base_entries[base_i].mode;
        } else if (in_base && in_a && !in_b) {
            re->status = AVC_MERGE_DELETED_B;
            memcpy(re->oid_a, base_entries[base_i].oid, 20);
            re->mode_a = base_entries[base_i].mode;
            memcpy(re->oid_b, a_entries[ai].oid, 20);
        } else if (in_base && !in_a && in_b) {
            re->status = AVC_MERGE_DELETED_A;
            memcpy(re->oid_a, b_entries[bi].oid, 20);
            re->mode_a = b_entries[bi].mode;
            memcpy(re->oid_b, base_entries[base_i].oid, 20);
            re->mode_b = base_entries[base_i].mode;
        } else if (in_a && in_b) {
            int same = (memcmp(a_entries[ai].oid, b_entries[bi].oid, 20) == 0
                       && a_entries[ai].mode == b_entries[bi].mode);
            if (same) {
                re->status = AVC_MERGE_SAME;
            } else if (in_base &&
                       memcmp(a_entries[ai].oid, base_entries[base_i].oid,
                              20) == 0 &&
                       a_entries[ai].mode == base_entries[base_i].mode) {
                re->status = AVC_MERGE_MODIFIED_B;
            } else if (in_base &&
                       memcmp(b_entries[bi].oid, base_entries[base_i].oid,
                              20) == 0 &&
                       b_entries[bi].mode == base_entries[base_i].mode) {
                re->status = AVC_MERGE_MODIFIED_A;
            } else {
                re->status = AVC_MERGE_CONFLICT;
            }
        } else if (!in_base && !in_a && in_b) {
            re->status = AVC_MERGE_ADDED_B;
        } else {
            re->status = AVC_MERGE_SAME;
        }

        if (in_a) ai++;
        if (in_b) bi++;
        if (in_base) base_i++;
        ri++;
    }

    free(base_entries);
    free(a_entries);
    free(b_entries);

    *entries = result;
    *entry_count = ri;
    return AVC_OK;
}

void avc_merge_entries_free(avc_merge_entry *entries, int count) {
    if (entries == NULL) return;
    for (int i = 0; i < count; i++) {
        free(entries[i].path);
    }
    free(entries);
}

avc_status avc_merge(avc_odb *odb, const char *metadata_path,
                     const avc_oid head_commit, const avc_oid branch_commit,
                     avc_oid result_commit, int *was_fast_forward,
                     avc_error *error) {
    (void)metadata_path;

    if (avc_merge_is_ancestor(odb, branch_commit, head_commit, error)) {
        memcpy(result_commit, branch_commit, 20);
        if (was_fast_forward) *was_fast_forward = 1;
        return AVC_OK;
    }

    if (was_fast_forward) *was_fast_forward = 0;

    avc_oid base_commit;
    avc_status status = avc_merge_find_base(odb, head_commit, branch_commit,
                                             base_commit, error);
    if (status != AVC_OK) return status;

    void *head_payload = NULL, *branch_payload = NULL, *base_payload = NULL;
    size_t head_size = 0, branch_size = 0, base_size = 0;
    avc_object_type type;

    status = avc_odb_read(odb, head_commit, &type, &head_payload,
                           &head_size, error);
    if (status != AVC_OK) return status;

    status = avc_odb_read(odb, branch_commit, &type, &branch_payload,
                           &branch_size, error);
    if (status != AVC_OK) { free(head_payload); return status; }

    status = avc_odb_read(odb, base_commit, &type, &base_payload,
                           &base_size, error);
    if (status != AVC_OK) { free(head_payload); free(branch_payload); return status; }

    avc_oid head_tree, branch_tree, base_tree;
    avc_commit_parse((const unsigned char *)head_payload, head_size,
                      &head_tree, NULL, NULL, NULL, 0, NULL, 0, NULL, 0,
                      error);
    avc_commit_parse((const unsigned char *)branch_payload, branch_size,
                      &branch_tree, NULL, NULL, NULL, 0, NULL, 0, NULL, 0,
                      error);
    avc_commit_parse((const unsigned char *)base_payload, base_size,
                      &base_tree, NULL, NULL, NULL, 0, NULL, 0, NULL, 0,
                      error);
    free(head_payload);
    free(branch_payload);
    free(base_payload);

    avc_merge_entry *entries = NULL;
    int entry_count = 0;
    status = avc_merge_trees(odb, base_tree, head_tree, branch_tree,
                              &entries, &entry_count, error);
    if (status != AVC_OK) return status;

    int has_conflict = 0;
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].status == AVC_MERGE_CONFLICT) {
            has_conflict = 1;
            break;
        }
    }

    if (has_conflict) {
        avc_merge_entries_free(entries, entry_count);
        avc_error_set(error, AVC_ERR_CONFLICT, "merge conflict");
        return AVC_ERR_CONFLICT;
    }

    avc_merge_entries_free(entries, entry_count);
    avc_error_set(error, AVC_ERR_NOT_FOUND, "merge not fully implemented");
    return AVC_ERR_NOT_FOUND;
}
