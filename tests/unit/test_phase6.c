#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static avc_odb odb;
static avc_signature sig;

static void setup_odb(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo;
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    avc_odb_init(&odb);
    assert(avc_odb_open(&odb, repo.objects_path, &error) == AVC_OK);

    snprintf(sig.name, sizeof(sig.name), "Tester");
    snprintf(sig.email, sizeof(sig.email), "t@t");
    sig.timestamp = 1700000000;
    sig.tz_offset = 0;

    avc_repository_free(&repo);
}

static void teardown_odb(void) {
    avc_odb_close(&odb);
}

static void make_commit_tree(avc_oid tree_oid,
                             const char *name, const char *content) {
    avc_error error;
    avc_error_clear(&error);
    avc_index index;
    avc_index_init(&index);

    avc_oid blob_oid;
    assert(avc_odb_write_blob(&odb, content, strlen(content),
                               blob_oid, &error) == AVC_OK);
    assert(avc_index_add(&index, name, AVC_MODE_REGULAR,
                          blob_oid, &error) == AVC_OK);
    assert(avc_commit_build_tree(&odb, &index, tree_oid, &error) == AVC_OK);
    avc_index_free(&index);
}

static void make_commit(avc_oid out, const avc_oid tree_oid,
                        const avc_oid *parent, int parent_count) {
    avc_error error;
    avc_error_clear(&error);
    assert(avc_commit_create(&odb, tree_oid,
                              (const avc_oid *)parent, parent_count,
                              &sig, &sig, "test", out, &error)
           == AVC_OK);
}

static void test_find_base_same(const char *root) {
    (void)root;
    avc_error error;
    avc_error_clear(&error);

    avc_oid tree, c1, base;
    make_commit_tree(tree, "f.txt", "base");
    make_commit(c1, tree, NULL, 0);

    assert(avc_merge_find_base(&odb, c1, c1, base, &error) == AVC_OK);
    assert(avc_oid_cmp(base, c1) == 0);
}

static void test_find_base_linear(const char *root) {
    (void)root;
    avc_error error;
    avc_error_clear(&error);

    avc_oid tree, tree2, c1, c2, c3, base;
    make_commit_tree(tree, "f.txt", "base");
    make_commit(c1, tree, NULL, 0);

    make_commit_tree(tree2, "f.txt", "change");
    make_commit(c2, tree2, (const avc_oid *)&c1, 1);

    make_commit(c3, tree2, (const avc_oid *)&c2, 1);

    assert(avc_merge_find_base(&odb, c3, c2, base, &error) == AVC_OK);
    assert(avc_oid_cmp(base, c2) == 0);
}

static void test_is_ancestor(const char *root) {
    (void)root;
    avc_error error;
    avc_error_clear(&error);

    avc_oid tree, c1, c2, c3;
    make_commit_tree(tree, "f.txt", "base");
    make_commit(c1, tree, NULL, 0);
    make_commit(c2, tree, (const avc_oid *)&c1, 1);
    make_commit(c3, tree, (const avc_oid *)&c2, 1);

    assert(avc_merge_is_ancestor(&odb, c3, c1, &error));
    assert(avc_merge_is_ancestor(&odb, c3, c2, &error));
    assert(avc_merge_is_ancestor(&odb, c3, c3, &error));
    assert(!avc_merge_is_ancestor(&odb, c1, c2, &error));
    assert(!avc_merge_is_ancestor(&odb, c1, c3, &error));
}

static void test_fast_forward(const char *root) {
    (void)root;
    avc_error error;
    avc_error_clear(&error);

    avc_oid tree, c1, c2, merged;
    make_commit_tree(tree, "f.txt", "base");
    make_commit(c1, tree, NULL, 0);
    make_commit(c2, tree, (const avc_oid *)&c1, 1);

    avc_error_clear(&error);
    avc_status status = avc_merge_fast_forward(&odb, c1, c2, merged, &error);
    assert(status == AVC_OK);
    assert(avc_oid_cmp(merged, c2) == 0);

    avc_error_clear(&error);
    status = avc_merge_fast_forward(&odb, c2, c1, merged, &error);
    assert(status != AVC_OK);
}

static void test_merge_trees_conflict(const char *root) {
    (void)root;
    avc_error error;
    avc_error_clear(&error);

    avc_oid base_tree, a_tree, b_tree;
    make_commit_tree(base_tree, "f.txt", "base");
    make_commit_tree(a_tree, "f.txt", "modified-a");
    make_commit_tree(b_tree, "f.txt", "modified-b");

    avc_merge_entry *entries = NULL;
    int count = 0;
    assert(avc_merge_trees(&odb, base_tree, a_tree, b_tree,
                            &entries, &count, &error) == AVC_OK);
    assert(count == 1);
    assert(strcmp(entries[0].path, "f.txt") == 0);
    assert(entries[0].status == AVC_MERGE_CONFLICT);
    avc_merge_entries_free(entries, count);
}

static void test_merge_trees_modify_one(const char *root) {
    (void)root;
    avc_error error;
    avc_error_clear(&error);

    avc_oid base_tree, a_tree;
    make_commit_tree(base_tree, "f.txt", "base");
    make_commit_tree(a_tree, "f.txt", "modified-a");

    avc_merge_entry *entries = NULL;
    int count = 0;
    assert(avc_merge_trees(&odb, base_tree, a_tree, base_tree,
                            &entries, &count, &error) == AVC_OK);
    assert(count == 1);
    assert(entries[0].status == AVC_MERGE_MODIFIED_A);
    avc_merge_entries_free(entries, count);
}

static void test_merge_workflow_fast_forward(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo;
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    avc_odb local_odb;
    avc_odb_init(&local_odb);
    assert(avc_odb_open(&local_odb, repo.objects_path, &error) == AVC_OK);

    avc_signature local_sig;
    snprintf(local_sig.name, sizeof(local_sig.name), "T");
    snprintf(local_sig.email, sizeof(local_sig.email), "t@t");
    local_sig.timestamp = 1700000000;
    local_sig.tz_offset = 0;

    avc_oid tree, c1, c2, result;
    {
        avc_index index;
        avc_index_init(&index);
        avc_oid blob_oid;
        assert(avc_odb_write_blob(&local_odb, "base", 4,
                                   blob_oid, &error) == AVC_OK);
        assert(avc_index_add(&index, "f.txt", AVC_MODE_REGULAR,
                              blob_oid, &error) == AVC_OK);
        assert(avc_commit_build_tree(&local_odb, &index, tree,
                                      &error) == AVC_OK);
        avc_index_free(&index);
    }
    assert(avc_commit_create(&local_odb, tree, NULL, 0,
                              &local_sig, &local_sig, "c1", c1,
                              &error) == AVC_OK);
    assert(avc_refs_write_ref(repo.metadata_path, "refs/heads/main",
                               c1, &error) == AVC_OK);
    assert(avc_refs_write_head_ref(repo.metadata_path, "refs/heads/main",
                                    &error) == AVC_OK);

    assert(avc_commit_create(&local_odb, tree,
                              (const avc_oid *)&c1, 1,
                              &local_sig, &local_sig, "c2", c2,
                              &error) == AVC_OK);
    assert(avc_refs_write_ref(repo.metadata_path, "refs/heads/feature",
                               c2, &error) == AVC_OK);

    int was_ff = 0;
    avc_merge(&local_odb, repo.metadata_path, c1, c2,
               result, &was_ff, &error);
    assert(was_ff);
    assert(avc_oid_cmp(result, c2) == 0);

    avc_odb_close(&local_odb);
    avc_repository_free(&repo);
}

int main(void) {
    char t1[] = "/tmp/astraphosvc-test6a-XXXXXX";
    assert(mkdtemp(t1) != NULL);
    setup_odb(t1);
    test_find_base_same(t1);
    test_find_base_linear(t1);
    test_is_ancestor(t1);
    test_fast_forward(t1);
    test_merge_trees_conflict(t1);
    test_merge_trees_modify_one(t1);

    char t2[] = "/tmp/astraphosvc-test6b-XXXXXX";
    assert(mkdtemp(t2) != NULL);
    test_merge_workflow_fast_forward(t2);

    teardown_odb();
    puts("phase6 tests passed");
    return 0;
}
