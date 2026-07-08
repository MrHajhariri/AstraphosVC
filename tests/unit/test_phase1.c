#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_temp_template(path) _mktemp(path)
#else
#include <unistd.h>
#define mkdir_temp_template(path) mkdtemp(path)
#endif

static char *join3(const char *a, const char *b, const char *c) {
    char *ab = avc_fs_join(a, b);
    assert(ab != NULL);
    char *abc = avc_fs_join(ab, c);
    free(ab);
    assert(abc != NULL);
    return abc;
}

static void test_config_round_trip(const char *root) {
    avc_error error;
    avc_error_clear(&error);
    avc_config config;
    avc_config_init(&config);
    assert(avc_config_set(&config, "core", "repositoryformatversion", "1", &error) == AVC_OK);
    assert(avc_config_set(&config, "user", "name", "Astraphos Tester", &error) == AVC_OK);

    char *path = avc_fs_join(root, "roundtrip.config");
    assert(path != NULL);
    assert(avc_config_write(&config, path, &error) == AVC_OK);
    avc_config_free(&config);

    avc_config loaded;
    avc_config_init(&loaded);
    assert(avc_config_load(&loaded, path, &error) == AVC_OK);
    assert(strcmp(avc_config_get(&loaded, "core", "repositoryformatversion"), "1") == 0);
    assert(strcmp(avc_config_get(&loaded, "user", "name"), "Astraphos Tester") == 0);
    avc_config_free(&loaded);
    free(path);
}

static void test_repository_init_and_discover(const char *root) {
    avc_error error;
    avc_error_clear(&error);
    avc_repository repo = {0};
    assert(avc_repository_init(root, "main", &repo, &error) == AVC_OK);

    char *head = avc_fs_join(repo.metadata_path, "HEAD");
    char *config = avc_fs_join(repo.metadata_path, "config");
    char *tmp = avc_fs_join(repo.metadata_path, "objects/tmp");
    char *heads = avc_fs_join(repo.metadata_path, "refs/heads");
    assert(head != NULL && config != NULL && tmp != NULL && heads != NULL);
    assert(avc_fs_is_regular_file(head));
    assert(avc_fs_is_regular_file(config));
    assert(avc_fs_is_directory(tmp));
    assert(avc_fs_is_directory(heads));

    char *src = join3(root, "src", "nested");
    assert(avc_fs_mkdir_p(src, &error) == AVC_OK);
    avc_repository discovered = {0};
    assert(avc_repository_discover(src, &discovered, &error) == AVC_OK);
    assert(strcmp(discovered.metadata_path, repo.metadata_path) == 0);

    avc_repository_free(&discovered);
    avc_repository_free(&repo);
    free(head);
    free(config);
    free(tmp);
    free(heads);
    free(src);
}

int main(void) {
    char template_path[] = "/tmp/astraphosvc-test-XXXXXX";
    char *root = mkdir_temp_template(template_path);
    assert(root != NULL);

    test_config_round_trip(root);
    test_repository_init_and_discover(root);

    puts("tests passed");
    return 0;
}
