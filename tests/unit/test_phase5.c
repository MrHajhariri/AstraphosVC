#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void test_current_branch_detached(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_oid oid;
    memset(oid, 0xbb, 20);
    avc_refs_write_head_oid(root, oid, &error);

    char *branch = NULL;
    avc_error_clear(&error);
    avc_status status = avc_refs_current_branch(root, &branch, &error);
    assert(status == AVC_OK);
    assert(branch == NULL);
}

static void test_current_branch_symbolic(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_status status = avc_refs_write_head_ref(root, "refs/heads/feature",
                                                &error);
    assert(status == AVC_OK);

    char *branch = NULL;
    avc_error_clear(&error);
    status = avc_refs_current_branch(root, &branch, &error);
    assert(status == AVC_OK);
    assert(branch != NULL);
    assert(strcmp(branch, "feature") == 0);
    free(branch);
}

static void test_list_branches(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    char **branches = NULL;
    int count = 0;
    avc_status status = avc_refs_list_branches(root, &branches, &count,
                                               &error);
    assert(status == AVC_OK);
    assert(count == 0);
    free(branches);

    avc_oid oid;
    memset(oid, 0x11, 20);
    avc_refs_write_ref(root, "refs/heads/main", oid, &error);
    avc_refs_write_ref(root, "refs/heads/dev", oid, &error);
    avc_refs_write_ref(root, "refs/heads/feature-x", oid, &error);

    avc_error_clear(&error);
    status = avc_refs_list_branches(root, &branches, &count, &error);
    assert(status == AVC_OK);
    assert(count == 3);

    int found_main = 0, found_dev = 0, found_feature = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(branches[i], "main") == 0) found_main = 1;
        if (strcmp(branches[i], "dev") == 0) found_dev = 1;
        if (strcmp(branches[i], "feature-x") == 0) found_feature = 1;
        free(branches[i]);
    }
    free(branches);
    assert(found_main);
    assert(found_dev);
    assert(found_feature);
}

static void test_delete_ref(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_oid oid;
    memset(oid, 0x22, 20);
    avc_refs_write_ref(root, "refs/heads/to-delete", oid, &error);

    avc_oid read_oid;
    avc_error_clear(&error);
    avc_status status = avc_refs_read_ref(root, "refs/heads/to-delete",
                                          read_oid, &error);
    assert(status == AVC_OK);

    avc_error_clear(&error);
    status = avc_refs_delete_ref(root, "refs/heads/to-delete", &error);
    assert(status == AVC_OK);

    avc_error_clear(&error);
    status = avc_refs_read_ref(root, "refs/heads/to-delete",
                               read_oid, &error);
    assert(status == AVC_ERR_NOT_FOUND);
}

static void test_branch_workflow(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo;
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    avc_odb odb;
    avc_odb_init(&odb);
    assert(avc_odb_open(&odb, repo.objects_path, &error) == AVC_OK);

    avc_oid blob_oid;
    assert(avc_odb_write_blob(&odb, "data", 4, blob_oid, &error) == AVC_OK);

    avc_index index;
    avc_index_init(&index);
    assert(avc_index_add(&index, "f.txt", AVC_MODE_REGULAR,
                          blob_oid, &error) == AVC_OK);

    avc_oid tree_oid;
    assert(avc_commit_build_tree(&odb, &index, tree_oid, &error) == AVC_OK);

    avc_signature sig;
    snprintf(sig.name, sizeof(sig.name), "T");
    snprintf(sig.email, sizeof(sig.email), "t@t");
    sig.timestamp = 1700000000;
    sig.tz_offset = 0;

    avc_oid commit_oid;
    assert(avc_commit_create(&odb, tree_oid, NULL, 0, &sig, &sig,
                              "initial", commit_oid, &error) == AVC_OK);
    assert(avc_refs_write_ref(repo.metadata_path, "refs/heads/main",
                               commit_oid, &error) == AVC_OK);
    assert(avc_refs_write_head_ref(repo.metadata_path, "refs/heads/main",
                                    &error) == AVC_OK);

    char *branch = NULL;
    avc_refs_current_branch(repo.metadata_path, &branch, &error);
    assert(branch != NULL);
    assert(strcmp(branch, "main") == 0);
    free(branch);

    assert(avc_refs_write_ref(repo.metadata_path, "refs/heads/dev", commit_oid,
                               &error) == AVC_OK);

    char **branches = NULL;
    int count = 0;
    avc_refs_list_branches(repo.metadata_path, &branches, &count, &error);
    assert(count == 2);
    for (int i = 0; i < count; i++) free(branches[i]);
    free(branches);

    avc_refs_write_head_ref(repo.metadata_path, "refs/heads/dev", &error);
    avc_refs_current_branch(repo.metadata_path, &branch, &error);
    assert(strcmp(branch, "dev") == 0);
    free(branch);

    avc_oid resolved;
    avc_refs_resolve_head(repo.metadata_path, resolved, &error);
    assert(avc_oid_cmp(resolved, commit_oid) == 0);

    avc_index_free(&index);
    avc_odb_close(&odb);
    avc_repository_free(&repo);
}

int main(void) {
    char t1[] = "/tmp/astraphosvc-test5a-XXXXXX";
    assert(mkdtemp(t1) != NULL);
    test_current_branch_detached(t1);

    char t2[] = "/tmp/astraphosvc-test5b-XXXXXX";
    assert(mkdtemp(t2) != NULL);
    test_current_branch_symbolic(t2);

    char t3[] = "/tmp/astraphosvc-test5c-XXXXXX";
    assert(mkdtemp(t3) != NULL);
    test_list_branches(t3);

    char t4[] = "/tmp/astraphosvc-test5d-XXXXXX";
    assert(mkdtemp(t4) != NULL);
    test_delete_ref(t4);

    char t5[] = "/tmp/astraphosvc-test5e-XXXXXX";
    assert(mkdtemp(t5) != NULL);
    test_branch_workflow(t5);

    puts("phase5 tests passed");
    return 0;
}
