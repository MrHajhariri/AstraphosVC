#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_pack_write_read(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo;
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    avc_odb odb;
    avc_odb_init(&odb);
    assert(avc_odb_open(&odb, repo.objects_path, &error) == AVC_OK);

    avc_signature sig;
    snprintf(sig.name, sizeof(sig.name), "T");
    snprintf(sig.email, sizeof(sig.email), "t@t");
    sig.timestamp = 1700000000;
    sig.tz_offset = 0;

    avc_oid blob_oid, tree_oid, commit_oid;
    assert(avc_odb_write_blob(&odb, "hello world", 11, blob_oid,
                               &error) == AVC_OK);

    avc_index index;
    avc_index_init(&index);
    assert(avc_index_add(&index, "f.txt", AVC_MODE_REGULAR,
                          blob_oid, &error) == AVC_OK);
    assert(avc_commit_build_tree(&odb, &index, tree_oid, &error) == AVC_OK);
    avc_index_free(&index);

    assert(avc_commit_create(&odb, tree_oid, NULL, 0,
                              &sig, &sig, "c1", commit_oid,
                              &error) == AVC_OK);

    avc_oid oids[3];
    memcpy(oids[0], blob_oid, 20);
    memcpy(oids[1], tree_oid, 20);
    memcpy(oids[2], commit_oid, 20);

    char *pack_path = avc_fs_join(root, "test.pack");
    char *idx_path = avc_fs_join(root, "test.idx");
    assert(pack_path != NULL);
    assert(idx_path != NULL);

    avc_error_clear(&error);
    assert(avc_pack_write(&odb, (const avc_oid *)oids, 3, pack_path, idx_path,
                           &error) == AVC_OK);

    avc_odb odb2;
    avc_odb_init(&odb2);
    assert(avc_odb_open(&odb2, repo.objects_path, &error) == AVC_OK);

    avc_error_clear(&error);
    assert(avc_pack_read(&odb2, pack_path, idx_path, &error) == AVC_OK);

    assert(avc_odb_exists(&odb2, blob_oid));
    assert(avc_odb_exists(&odb2, tree_oid));
    assert(avc_odb_exists(&odb2, commit_oid));

    free(pack_path);
    free(idx_path);
    avc_odb_close(&odb);
    avc_odb_close(&odb2);
    avc_repository_free(&repo);
}

int main(void) {
    char t1[] = "/tmp/astraphosvc-test9a-XXXXXX";
    assert(mkdtemp(t1) != NULL);
    test_pack_write_read(t1);

    puts("phase9 tests passed");
    return 0;
}
