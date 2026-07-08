#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_remote_add_list_remove(const char *root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository repo;
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    avc_remote_add(repo.metadata_path, "origin", "/tmp/other", &error);
    avc_remote_add(repo.metadata_path, "upstream", "/tmp/up", &error);

    avc_remote *remotes = NULL;
    int count = 0;
    assert(avc_remote_list(repo.metadata_path, &remotes, &count,
                            &error) == AVC_OK);
    assert(count == 2);

    int found_origin = 0, found_upstream = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(remotes[i].name, "origin") == 0) {
            assert(strcmp(remotes[i].url, "/tmp/other") == 0);
            found_origin = 1;
        }
        if (strcmp(remotes[i].name, "upstream") == 0) {
            assert(strcmp(remotes[i].url, "/tmp/up") == 0);
            found_upstream = 1;
        }
    }
    assert(found_origin);
    assert(found_upstream);
    avc_remote_list_free(remotes, count);

    avc_remote_remove(repo.metadata_path, "origin", &error);
    assert(avc_remote_list(repo.metadata_path, &remotes, &count,
                            &error) == AVC_OK);
    assert(count == 1);
    assert(strcmp(remotes[0].name, "upstream") == 0);
    avc_remote_list_free(remotes, count);

    avc_repository_free(&repo);
}

static void test_fetch_push_local(const char *src_root, const char *dst_root) {
    avc_error error;
    avc_error_clear(&error);

    avc_repository src_repo;
    assert(avc_repository_init(src_root, "main", &src_repo,
                                &error) == AVC_OK);

    avc_odb src_odb;
    avc_odb_init(&src_odb);
    assert(avc_odb_open(&src_odb, src_repo.objects_path, &error) == AVC_OK);

    avc_signature sig;
    snprintf(sig.name, sizeof(sig.name), "T");
    snprintf(sig.email, sizeof(sig.email), "t@t");
    sig.timestamp = 1700000000;
    sig.tz_offset = 0;

    avc_oid tree, commit;
    {
        avc_index idx;
        avc_index_init(&idx);
        avc_oid blob;
        assert(avc_odb_write_blob(&src_odb, "data", 4, blob,
                                   &error) == AVC_OK);
        assert(avc_index_add(&idx, "f.txt", AVC_MODE_REGULAR,
                              blob, &error) == AVC_OK);
        assert(avc_commit_build_tree(&src_odb, &idx, tree, &error) == AVC_OK);
        avc_index_free(&idx);
    }
    assert(avc_commit_create(&src_odb, tree, NULL, 0,
                              &sig, &sig, "c1", commit, &error) == AVC_OK);
    assert(avc_refs_write_ref(src_repo.metadata_path, "refs/heads/main",
                               commit, &error) == AVC_OK);

    avc_repository dst_repo;
    assert(avc_repository_init(dst_root, "main", &dst_repo,
                                &error) == AVC_OK);

    avc_odb dst_odb;
    avc_odb_init(&dst_odb);
    assert(avc_odb_open(&dst_odb, dst_repo.objects_path, &error) == AVC_OK);

    avc_error_clear(&error);
    avc_status status = avc_remote_push(&src_odb, src_repo.metadata_path,
                                         dst_root, "origin", "main", &error);
    assert(status == AVC_OK);

    assert(avc_odb_exists(&dst_odb, commit));

    avc_odb_close(&src_odb);
    avc_odb_close(&dst_odb);
    avc_repository_free(&src_repo);
    avc_repository_free(&dst_repo);
}

int main(void) {
    char t1[] = "/tmp/astraphosvc-test8a-XXXXXX";
    assert(mkdtemp(t1) != NULL);
    test_remote_add_list_remove(t1);

    char t2[] = "/tmp/astraphosvc-test8b-XXXXXX";
    char t3[] = "/tmp/astraphosvc-test8c-XXXXXX";
    assert(mkdtemp(t2) != NULL);
    assert(mkdtemp(t3) != NULL);
    test_fetch_push_local(t2, t3);

    puts("phase8 tests passed");
    return 0;
}
