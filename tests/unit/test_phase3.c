#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_index_add_find(void) {
    avc_index idx;
    avc_index_init(&idx);

    avc_error error;
    avc_error_clear(&error);

    avc_oid oid1, oid2;
    memset(oid1, 0x01, 20);
    memset(oid2, 0x02, 20);

    assert(avc_index_add(&idx, "file.txt", AVC_MODE_REGULAR, oid1,
                          &error) == AVC_OK);

    avc_index_entry *e = avc_index_find(&idx, "file.txt");
    assert(e != NULL);
    assert(strcmp(e->path, "file.txt") == 0);
    assert(avc_oid_cmp(e->oid, oid1) == 0);
    assert(e->mode == AVC_MODE_REGULAR);

    e = avc_index_find(&idx, "nonexistent");
    assert(e == NULL);

    assert(avc_index_add(&idx, "file.txt", AVC_MODE_EXECUTABLE, oid2,
                          &error) == AVC_OK);
    e = avc_index_find(&idx, "file.txt");
    assert(e != NULL);
    assert(avc_oid_cmp(e->oid, oid2) == 0);
    assert(e->mode == AVC_MODE_EXECUTABLE);

    assert(avc_index_remove(&idx, "file.txt", &error) == AVC_OK);
    assert(avc_index_find(&idx, "file.txt") == NULL);

    assert(avc_index_remove(&idx, "file.txt", &error) == AVC_ERR_NOT_FOUND);

    avc_index_free(&idx);
}

static void test_index_sorted_insert(void) {
    avc_index idx;
    avc_index_init(&idx);

    avc_error error;
    avc_oid oid;
    memset(oid, 0, 20);

    assert(avc_index_add(&idx, "zeta", AVC_MODE_REGULAR, oid, &error) == AVC_OK);
    assert(avc_index_add(&idx, "alpha", AVC_MODE_REGULAR, oid, &error) == AVC_OK);
    assert(avc_index_add(&idx, "beta", AVC_MODE_REGULAR, oid, &error) == AVC_OK);

    assert(idx.count == 3);
    assert(strcmp(idx.entries[0].path, "alpha") == 0);
    assert(strcmp(idx.entries[1].path, "beta") == 0);
    assert(strcmp(idx.entries[2].path, "zeta") == 0);

    avc_index_free(&idx);
}

static void test_index_write_load(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    char *index_path = avc_fs_join(root, "test_index");
    assert(index_path != NULL);

    avc_oid oid1, oid2, oid3;
    memset(oid1, 0xaa, 20);
    memset(oid2, 0xbb, 20);
    memset(oid3, 0xcc, 20);

    avc_index write_idx;
    avc_index_init(&write_idx);
    assert(avc_index_add(&write_idx, "file_a.txt", AVC_MODE_REGULAR,
                          oid1, &error) == AVC_OK);
    assert(avc_index_add(&write_idx, "file_b.txt", AVC_MODE_EXECUTABLE,
                          oid2, &error) == AVC_OK);
    assert(avc_index_add(&write_idx, "sub/file_c.txt", AVC_MODE_REGULAR,
                          oid3, &error) == AVC_OK);

    assert(avc_index_write(&write_idx, index_path, &error) == AVC_OK);
    avc_index_free(&write_idx);

    avc_index read_idx;
    avc_index_init(&read_idx);
    assert(avc_index_load(&read_idx, index_path, &error) == AVC_OK);

    assert(read_idx.count == 3);

    avc_index_entry *e;

    e = avc_index_find(&read_idx, "file_a.txt");
    assert(e != NULL);
    assert(avc_oid_cmp(e->oid, oid1) == 0);
    assert(e->mode == AVC_MODE_REGULAR);

    e = avc_index_find(&read_idx, "file_b.txt");
    assert(e != NULL);
    assert(avc_oid_cmp(e->oid, oid2) == 0);
    assert(e->mode == AVC_MODE_EXECUTABLE);

    e = avc_index_find(&read_idx, "sub/file_c.txt");
    assert(e != NULL);
    assert(avc_oid_cmp(e->oid, oid3) == 0);

    avc_index_free(&read_idx);
    free(index_path);
}

static void test_index_load_nonexistent(void) {
    avc_error error;
    avc_error_clear(&error);

    avc_index idx;
    avc_index_init(&idx);
    assert(avc_index_load(&idx, "/tmp/nonexistent_index_file_xyz",
                          &error) == AVC_OK);
    assert(idx.count == 0);

    avc_index_free(&idx);
}

static void test_fs_stat(void) {
    avc_file_stat st;
    assert(avc_fs_stat("/", &st) == 0);
    assert(st.dev != 0 || st.ino != 0);

    assert(avc_fs_stat("/nonexistent_path_xyz_123", &st) != 0);
}

int main(void) {
    test_index_add_find();
    test_index_sorted_insert();

    char template[] = "/tmp/astraphosvc-test3-XXXXXX";
    char *root = mkdtemp(template);
    assert(root != NULL);

    test_index_write_load(root);
    test_index_load_nonexistent();
    test_fs_stat();

    puts("phase3 tests passed");
    return 0;
}
