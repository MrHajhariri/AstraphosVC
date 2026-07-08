#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_diff_blobs_basic(void) {
    const unsigned char *a = (const unsigned char *)"hello\nworld\n";
    const unsigned char *b = (const unsigned char *)"hello\nuniverse\n";

    char *diff = avc_diff_blobs(a, 12, b, 15, "a.txt", "b.txt");
    assert(diff != NULL);

    assert(strstr(diff, "--- a.txt") != NULL);
    assert(strstr(diff, "+++ b.txt") != NULL);
    assert(strstr(diff, "-world") != NULL);
    assert(strstr(diff, "+universe") != NULL);
    free(diff);
}

static void test_diff_blobs_identical(void) {
    const unsigned char *a = (const unsigned char *)"same\ncontent\n";
    const unsigned char *b = (const unsigned char *)"same\ncontent\n";

    char *diff = avc_diff_blobs(a, 13, b, 13, "a", "b");
    assert(diff != NULL);
    assert(strstr(diff, "--- a") != NULL);
    assert(strstr(diff, "+++ b") != NULL);
    free(diff);
}

static void test_diff_blobs_added(void) {
    const unsigned char *a = (const unsigned char *)"";
    const unsigned char *b = (const unsigned char *)"new\nfile\n";

    char *diff = avc_diff_blobs(a, 0, b, 8, "a", "b");
    assert(diff != NULL);
    assert(strstr(diff, "+new") != NULL);
    assert(strstr(diff, "+file") != NULL);
    free(diff);
}

static void test_diff_blobs_deleted(void) {
    const unsigned char *a = (const unsigned char *)"old\ncontent\n";
    const unsigned char *b = (const unsigned char *)"";

    char *diff = avc_diff_blobs(a, 12, b, 0, "a", "b");
    assert(diff != NULL);
    assert(strstr(diff, "-old") != NULL);
    assert(strstr(diff, "-content") != NULL);
    free(diff);
}

static void test_diff_trees_add_delete(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo;
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    avc_odb odb;
    avc_odb_init(&odb);
    assert(avc_odb_open(&odb, repo.objects_path, &error) == AVC_OK);

    avc_oid blob;
    assert(avc_odb_write_blob(&odb, "data", 4, blob, &error) == AVC_OK);

    avc_index idx;
    avc_index_init(&idx);
    assert(avc_index_add(&idx, "f1.txt", AVC_MODE_REGULAR,
                          blob, &error) == AVC_OK);
    assert(avc_index_add(&idx, "f2.txt", AVC_MODE_REGULAR,
                          blob, &error) == AVC_OK);

    avc_oid tree_a;
    assert(avc_commit_build_tree(&odb, &idx, tree_a, &error) == AVC_OK);

    avc_index idx2;
    avc_index_init(&idx2);
    assert(avc_index_add(&idx2, "f1.txt", AVC_MODE_REGULAR,
                          blob, &error) == AVC_OK);

    avc_oid tree_b;
    assert(avc_commit_build_tree(&odb, &idx2, tree_b, &error) == AVC_OK);

    avc_diff_file *files = NULL;
    int count = 0;
    assert(avc_diff_trees(&odb, tree_a, tree_b,
                           &files, &count, &error) == AVC_OK);
    assert(count == 2);

    int found_f1 = 0, found_f2 = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(files[i].path, "f1.txt") == 0) {
            assert(files[i].status == AVC_DIFF_UNCHANGED);
            found_f1 = 1;
        }
        if (strcmp(files[i].path, "f2.txt") == 0) {
            assert(files[i].status == AVC_DIFF_DELETED);
            found_f2 = 1;
        }
    }
    assert(found_f1);
    assert(found_f2);

    avc_diff_files_free(files, count);
    avc_index_free(&idx);
    avc_index_free(&idx2);
    avc_odb_close(&odb);
    avc_repository_free(&repo);
}

int main(void) {
    test_diff_blobs_basic();
    test_diff_blobs_identical();
    test_diff_blobs_added();
    test_diff_blobs_deleted();

    char t1[] = "/tmp/astraphosvc-test7a-XXXXXX";
    assert(mkdtemp(t1) != NULL);
    test_diff_trees_add_delete(t1);

    puts("phase7 tests passed");
    return 0;
}
