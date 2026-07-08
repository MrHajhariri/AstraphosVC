#ifndef AVC_MERGE_H
#define AVC_MERGE_H

#include "objects/avc_oid.h"
#include "objects/avc_object.h"
#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum avc_merge_entry_status {
    AVC_MERGE_SAME,
    AVC_MERGE_MODIFIED_A,
    AVC_MERGE_MODIFIED_B,
    AVC_MERGE_ADDED_A,
    AVC_MERGE_ADDED_B,
    AVC_MERGE_DELETED_A,
    AVC_MERGE_DELETED_B,
    AVC_MERGE_CONFLICT
} avc_merge_entry_status;

typedef struct avc_merge_entry {
    char *path;
    avc_merge_entry_status status;
    avc_oid oid_a;
    avc_oid oid_b;
    uint32_t mode_a;
    uint32_t mode_b;
} avc_merge_entry;

avc_status avc_merge_find_base(avc_odb *odb, const avc_oid commit_a,
                               const avc_oid commit_b, avc_oid base_out,
                               avc_error *error);

int avc_merge_is_ancestor(avc_odb *odb, const avc_oid commit,
                          const avc_oid ancestor, avc_error *error);

avc_status avc_merge_fast_forward(avc_odb *odb, const avc_oid head_oid,
                                  const avc_oid branch_oid,
                                  avc_oid merged_out, avc_error *error);

avc_status avc_merge_trees(avc_odb *odb,
                           const avc_oid base_tree,
                           const avc_oid a_tree,
                           const avc_oid b_tree,
                           avc_merge_entry **entries, int *entry_count,
                           avc_error *error);

void avc_merge_entries_free(avc_merge_entry *entries, int count);

avc_status avc_merge(avc_odb *odb, const char *metadata_path,
                     const avc_oid head_commit, const avc_oid branch_commit,
                     avc_oid result_commit, int *was_fast_forward,
                     avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
