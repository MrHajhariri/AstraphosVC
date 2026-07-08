#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void test_refs_write_read(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_oid test_oid;
    memset(test_oid, 0xaa, 20);

    avc_status status = avc_refs_write_ref(root, "refs/heads/test",
                                            test_oid, &error);
    assert(status == AVC_OK);

    avc_oid read_oid;
    avc_error_clear(&error);
    status = avc_refs_read_ref(root, "refs/heads/test", read_oid, &error);
    assert(status == AVC_OK);
    assert(avc_oid_cmp(test_oid, read_oid) == 0);

    avc_error_clear(&error);
    status = avc_refs_read_ref(root, "refs/heads/nonexistent",
                               read_oid, &error);
    assert(status == AVC_ERR_NOT_FOUND);
}

static void test_refs_head_symbolic(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_status status = avc_refs_write_head_ref(root, "refs/heads/main",
                                                 &error);
    assert(status == AVC_OK);

    char *ref_or_oid = NULL;
    int is_symbolic = 0;
    avc_error_clear(&error);
    status = avc_refs_read_head(root, &ref_or_oid, &is_symbolic, &error);
    assert(status == AVC_OK);
    assert(is_symbolic);
    assert(strcmp(ref_or_oid, "refs/heads/main") == 0);
    free(ref_or_oid);
}

static void test_parse_and_create_commit(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo;
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    avc_odb odb;
    avc_odb_init(&odb);
    assert(avc_odb_open(&odb, repo.objects_path, &error) == AVC_OK);

    avc_oid blob_oid;
    assert(avc_odb_write_blob(&odb, "file content", 12, blob_oid,
                               &error) == AVC_OK);

    avc_index index;
    avc_index_init(&index);
    assert(avc_index_add(&index, "file.txt", AVC_MODE_REGULAR,
                          blob_oid, &error) == AVC_OK);

    avc_oid tree_oid;
    assert(avc_commit_build_tree(&odb, &index, tree_oid, &error) == AVC_OK);

    assert(avc_odb_exists(&odb, tree_oid));

    avc_signature sig;
    snprintf(sig.name, sizeof(sig.name), "Test User");
    snprintf(sig.email, sizeof(sig.email), "test@test");
    sig.timestamp = 1700000000;
    sig.tz_offset = 0;

    avc_oid commit_oid;
    assert(avc_commit_create(&odb, tree_oid, NULL, 0, &sig, &sig,
                              "initial commit", commit_oid, &error) == AVC_OK);
    assert(avc_odb_exists(&odb, commit_oid));

    void *payload = NULL;
    size_t payload_size = 0;
    avc_object_type type;
    assert(avc_odb_read(&odb, commit_oid, &type, &payload,
                         &payload_size, &error) == AVC_OK);
    assert(type == AVC_OBJECT_COMMIT);

    avc_oid parsed_tree;
    avc_oid parsed_parents[16];
    int parent_count = 0;
    char author[256] = "", msg[4096] = "";

    assert(avc_commit_parse((const unsigned char *)payload, payload_size,
                             &parsed_tree, parsed_parents, &parent_count,
                             author, sizeof(author),
                             NULL, 0, msg, sizeof(msg), &error) == AVC_OK);
    assert(avc_oid_cmp(parsed_tree, tree_oid) == 0);
    assert(parent_count == 0);
    assert(strstr(author, "Test User") != NULL);
    assert(strstr(msg, "initial commit") != NULL);
    free(payload);

    avc_oid child_oid;
    assert(avc_commit_create(&odb, tree_oid, (const avc_oid *)&commit_oid, 1, &sig, &sig,
                              "second commit", child_oid, &error) == AVC_OK);

    assert(avc_odb_read(&odb, child_oid, &type, &payload,
                         &payload_size, &error) == AVC_OK);
    assert(avc_commit_parse((const unsigned char *)payload, payload_size,
                             &parsed_tree, parsed_parents, &parent_count,
                             author, sizeof(author),
                             NULL, 0, msg, sizeof(msg), &error) == AVC_OK);
    assert(parent_count == 1);
    assert(avc_oid_cmp(parsed_parents[0], commit_oid) == 0);
    free(payload);

    avc_index_free(&index);
    avc_odb_close(&odb);
    avc_repository_free(&repo);
}

static void test_full_commit_workflow(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo;
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    avc_odb odb;
    avc_odb_init(&odb);
    assert(avc_odb_open(&odb, repo.objects_path, &error) == AVC_OK);

    avc_oid blob_oid;
    assert(avc_odb_write_blob(&odb, "hello", 5, blob_oid, &error) == AVC_OK);

    avc_index index;
    avc_index_init(&index);
    assert(avc_index_add(&index, "hello.txt", AVC_MODE_REGULAR,
                          blob_oid, &error) == AVC_OK);

    avc_oid tree_oid;
    assert(avc_commit_build_tree(&odb, &index, tree_oid, &error) == AVC_OK);

    avc_signature sig;
    snprintf(sig.name, sizeof(sig.name), "Test");
    snprintf(sig.email, sizeof(sig.email), "test@test");
    sig.timestamp = 1700000000;
    sig.tz_offset = 0;

    avc_oid commit_oid;
    assert(avc_commit_create(&odb, tree_oid, NULL, 0, &sig, &sig,
                              "first", commit_oid, &error) == AVC_OK);

    assert(avc_refs_write_ref(root, "refs/heads/main", commit_oid,
                               &error) == AVC_OK);
    assert(avc_refs_write_head_ref(root, "refs/heads/main", &error) == AVC_OK);

    avc_oid resolved;
    assert(avc_refs_resolve_head(root, resolved, &error) == AVC_OK);
    assert(avc_oid_cmp(resolved, commit_oid) == 0);

    avc_index_free(&index);
    avc_odb_close(&odb);
    avc_repository_free(&repo);
}

int main(void) {
    char t1[] = "/tmp/astraphosvc-test4a-XXXXXX";
    char *r1 = mkdtemp(t1);
    assert(r1 != NULL);
    test_refs_write_read(r1);
    test_refs_head_symbolic(r1);

    char t2[] = "/tmp/astraphosvc-test4b-XXXXXX";
    char *r2 = mkdtemp(t2);
    assert(r2 != NULL);
    test_parse_and_create_commit(r2);

    char t3[] = "/tmp/astraphosvc-test4c-XXXXXX";
    char *r3 = mkdtemp(t3);
    assert(r3 != NULL);
    test_full_commit_workflow(r3);

    puts("phase4 tests passed");
    return 0;
}
