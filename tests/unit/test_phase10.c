#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "api/astraphosvc.h"

static int failures = 0;

#define TEST(name) do { printf("  %s ... ", name); fflush(stdout); } while (0)
#define PASS() puts("PASS")
#define FAIL(msg) do { \
    puts("FAIL"); \
    fprintf(stderr, "  FAIL at %s:%d: %s\n", __FILE__, __LINE__, msg); \
    failures++; \
} while (0)
#define ASSERT(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while (0)

static void remove_dir(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    (void)system(cmd);
}

static int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f == NULL) return -1;
    if (content != NULL) fputs(content, f);
    fclose(f);
    return 0;
}

static int mkdir_p(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    return system(cmd);
}

static avc_status write_loose_object(const char *objects_path,
                                     const char *type_name,
                                     const void *payload, size_t payload_size,
                                     avc_oid out, avc_error *error) {
    int header_len = snprintf(NULL, 0, "%s %zu", type_name, payload_size);
    if (header_len < 0) return AVC_ERR_IO;

    size_t canonical_size = (size_t)header_len + 1 + payload_size;
    unsigned char *canonical = (unsigned char *)malloc(canonical_size);
    if (canonical == NULL) return AVC_ERR_NO_MEMORY;

    snprintf((char *)canonical, canonical_size, "%s %zu", type_name, payload_size);
    canonical[(size_t)header_len] = '\0';
    memcpy(canonical + (size_t)header_len + 1, payload, payload_size);

    avc_sha1_ctx hash_ctx;
    avc_sha1_init(&hash_ctx);
    avc_sha1_update(&hash_ctx, canonical, canonical_size);
    avc_sha1_final(&hash_ctx, out);

    void *compressed = NULL;
    size_t compressed_size = 0;
    avc_status status = avc_compress(canonical, canonical_size,
                                      &compressed, &compressed_size, error);
    free(canonical);
    if (status != AVC_OK) return status;

    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(out, hex);
    char dir[3] = {hex[0], hex[1], '\0'};
    char file[39];
    memcpy(file, hex + 2, 38);
    file[38] = '\0';

    char obj_dir[512];
    snprintf(obj_dir, sizeof(obj_dir), "%s/%s", objects_path, dir);
    mkdir_p(obj_dir);

    char obj_path[512];
    snprintf(obj_path, sizeof(obj_path), "%s/%s/%s", objects_path, dir, file);

    status = avc_fs_write_file(obj_path, compressed, compressed_size, error);
    free(compressed);
    return status;
}

static void test_git_repo_detect(void) {
    TEST("git_repo_detect");
    remove_dir("/tmp/test_phase10_detect");

    ASSERT(!avc_git_is_git_repo("/tmp/test_phase10_detect"),
           "should not detect non-existent as git repo");

    ASSERT(mkdir("/tmp/test_phase10_detect", 0755) == 0, "mkdir");
    ASSERT(!avc_git_is_git_repo("/tmp/test_phase10_detect"),
           "should not detect empty dir as git repo");

    write_file("/tmp/test_phase10_detect/HEAD", "ref: refs/heads/main\n");
    ASSERT(avc_git_is_git_repo("/tmp/test_phase10_detect"),
           "should detect dir with HEAD as git repo");

    remove_dir("/tmp/test_phase10_detect");
    PASS();
}

static void test_git_repo_open(void) {
    TEST("git_repo_open");
    remove_dir("/tmp/test_phase10_open");
    mkdir("/tmp/test_phase10_open", 0755);

    avc_git_repo repo;
    avc_git_repo_init(&repo);
    avc_error err;
    avc_error_clear(&err);

    ASSERT(avc_git_repo_open("/tmp/test_phase10_open", &repo, &err) != AVC_OK,
           "should fail without HEAD");

    write_file("/tmp/test_phase10_open/HEAD", "ref: refs/heads/main\n");
    mkdir_p("/tmp/test_phase10_open/objects/pack");
    mkdir_p("/tmp/test_phase10_open/refs/heads");

    ASSERT(avc_git_repo_open("/tmp/test_phase10_open", &repo, &err) == AVC_OK,
           "should succeed with HEAD");

    ASSERT(repo.gitdir_path != NULL, "gitdir_path not null");
    ASSERT(repo.objects_path != NULL, "objects_path not null");
    ASSERT(repo.refs_path != NULL, "refs_path not null");
    ASSERT(repo.pack_path != NULL, "pack_path not null");
    ASSERT(strstr(repo.objects_path, "objects") != NULL, "objects path contains objects");
    ASSERT(strstr(repo.pack_path, "pack") != NULL, "pack path contains pack");

    avc_git_repo_free(&repo);
    remove_dir("/tmp/test_phase10_open");
    PASS();
}

static void test_git_read_object_loose(void) {
    TEST("git_read_object_loose");
    remove_dir("/tmp/test_phase10_obj");
    mkdir("/tmp/test_phase10_obj", 0755);

    write_file("/tmp/test_phase10_obj/HEAD", "ref: refs/heads/main\n");
    mkdir_p("/tmp/test_phase10_obj/objects");
    mkdir_p("/tmp/test_phase10_obj/refs/heads");

    avc_error err;
    avc_error_clear(&err);

    avc_oid blob_oid;
    ASSERT(write_loose_object("/tmp/test_phase10_obj/objects", "blob",
                               "hello\n", 6, blob_oid, &err) == AVC_OK,
           "write test blob");

    avc_git_repo repo;
    avc_git_repo_init(&repo);
    ASSERT(avc_git_repo_open("/tmp/test_phase10_obj", &repo, &err) == AVC_OK,
           "open git repo");

    avc_object_type type;
    void *payload = NULL;
    size_t payload_size = 0;
    ASSERT(avc_git_read_object(&repo, blob_oid, &type, &payload,
                                &payload_size, &err) == AVC_OK,
           "read blob object");
    ASSERT(type == AVC_OBJECT_BLOB, "type is blob");
    ASSERT(payload_size == 6, "payload size is 6");
    ASSERT(memcmp(payload, "hello\n", 6) == 0, "payload matches");
    free(payload);

    avc_oid nonexistent;
    memset(nonexistent, 0xaa, 20);
    ASSERT(avc_git_read_object(&repo, nonexistent, &type, &payload,
                                &payload_size, &err) != AVC_OK,
           "read nonexistent object fails");

    avc_git_repo_free(&repo);
    remove_dir("/tmp/test_phase10_obj");
    PASS();
}

static void test_git_read_ref(void) {
    TEST("git_read_ref");
    remove_dir("/tmp/test_phase10_ref");
    mkdir("/tmp/test_phase10_ref", 0755);

    write_file("/tmp/test_phase10_ref/HEAD", "ref: refs/heads/main\n");
    mkdir_p("/tmp/test_phase10_ref/refs/heads");
    mkdir_p("/tmp/test_phase10_ref/objects");

    avc_oid expected;
    memset(expected, 0, 20);
    expected[0] = 0xab;
    expected[19] = 0xcd;

    char hex[AVC_OID_HEX_SIZE];
    avc_oid_hex(expected, hex);
    write_file("/tmp/test_phase10_ref/refs/heads/main", hex);

    avc_git_repo repo;
    avc_git_repo_init(&repo);
    avc_error err;
    avc_error_clear(&err);
    ASSERT(avc_git_repo_open("/tmp/test_phase10_ref", &repo, &err) == AVC_OK,
           "open repo");

    avc_oid actual;
    ASSERT(avc_git_read_ref(&repo, "refs/heads/main", actual, &err) == AVC_OK,
           "read refs/heads/main");
    ASSERT(memcmp(actual, expected, 20) == 0, "ref value matches");

    ASSERT(avc_git_resolve_ref(&repo, "HEAD", actual, &err) == AVC_OK,
           "resolve HEAD");
    ASSERT(memcmp(actual, expected, 20) == 0, "HEAD resolves to branch tip");

    ASSERT(avc_git_read_ref(&repo, "refs/heads/nonexistent", actual, &err) != AVC_OK,
           "read nonexistent ref fails");

    avc_git_repo_free(&repo);
    remove_dir("/tmp/test_phase10_ref");
    PASS();
}

static void test_git_resolve_head_detached(void) {
    TEST("git_resolve_head_detached");
    remove_dir("/tmp/test_phase10_detached");
    mkdir("/tmp/test_phase10_detached", 0755);

    char hex[AVC_OID_HEX_SIZE];
    memset(hex, 0, sizeof(hex));
    avc_oid expected;
    memset(expected, 0, 20);
    expected[5] = 0x42;
    avc_oid_hex(expected, hex);

    char head_buf[64];
    snprintf(head_buf, sizeof(head_buf), "%s\n", hex);
    write_file("/tmp/test_phase10_detached/HEAD", head_buf);
    mkdir_p("/tmp/test_phase10_detached/objects");
    mkdir_p("/tmp/test_phase10_detached/refs/heads");

    avc_git_repo repo;
    avc_git_repo_init(&repo);
    avc_error err;
    avc_error_clear(&err);
    ASSERT(avc_git_repo_open("/tmp/test_phase10_detached", &repo, &err) == AVC_OK,
           "open repo");

    avc_oid actual;
    ASSERT(avc_git_resolve_ref(&repo, "HEAD", actual, &err) == AVC_OK,
           "resolve detached HEAD");
    ASSERT(memcmp(actual, expected, 20) == 0, "detached HEAD oid matches");

    avc_git_repo_free(&repo);
    remove_dir("/tmp/test_phase10_detached");
    PASS();
}

static void test_git_pack_open_fail(void) {
    TEST("git_pack_open_fail");
    avc_git_pack pack;
    avc_error err;
    avc_error_clear(&err);

    memset(&pack, 0, sizeof(pack));
    ASSERT(avc_git_pack_open(&pack, "/nonexistent/pack.pack", &err) != AVC_OK,
           "open nonexistent pack fails");
    avc_git_pack_close(&pack);

    remove_dir("/tmp/test_phase10_pack_fail");
    mkdir("/tmp/test_phase10_pack_fail", 0755);
    write_file("/tmp/test_phase10_pack_fail/bad.pack", "notapack");

    memset(&pack, 0, sizeof(pack));
    ASSERT(avc_git_pack_open(&pack, "/tmp/test_phase10_pack_fail/bad.pack",
                              &err) != AVC_OK,
           "open invalid pack fails");
    avc_git_pack_close(&pack);

    remove_dir("/tmp/test_phase10_pack_fail");
    PASS();
}

static void test_config_subsection(void) {
    TEST("config subsection parsing");
    remove_dir("/tmp/test_phase10_config");
    mkdir("/tmp/test_phase10_config", 0755);

    const char *config_text =
        "[core]\n"
        "\trepositoryformatversion = 0\n"
        "[remote \"origin\"]\n"
        "\turl = https://github.com/test/repo.git\n"
        "\tfetch = +refs/heads/*:refs/remotes/origin/*\n"
        "[branch \"main\"]\n"
        "\tremote = origin\n"
        "\tmerge = refs/heads/main\n";

    char *cfg_path = "/tmp/test_phase10_config/gitconfig";
    write_file(cfg_path, config_text);

    avc_config config;
    avc_config_init(&config);
    avc_error err;
    avc_error_clear(&err);
    ASSERT(avc_config_load(&config, cfg_path, &err) == AVC_OK,
           "load git config");

    const char *val = avc_config_get(&config, "core", "repositoryformatversion");
    ASSERT(val != NULL && strcmp(val, "0") == 0,
           "core.repositoryformatversion = 0");

    val = avc_config_get(&config, "remote.origin", "url");
    ASSERT(val != NULL && strcmp(val, "https://github.com/test/repo.git") == 0,
           "remote.origin.url matches");

    val = avc_config_get(&config, "remote.origin", "fetch");
    ASSERT(val != NULL, "remote.origin.fetch not null");

    val = avc_config_get(&config, "branch.main", "remote");
    ASSERT(val != NULL && strcmp(val, "origin") == 0,
           "branch.main.remote = origin");

    val = avc_config_get(&config, "branch.main", "merge");
    ASSERT(val != NULL && strcmp(val, "refs/heads/main") == 0,
           "branch.main.merge = refs/heads/main");

    avc_config_free(&config);
    remove_dir("/tmp/test_phase10_config");
    PASS();
}

static void test_config_reparse(void) {
    TEST("config subsection write + re-read");
    remove_dir("/tmp/test_phase10_reparse");
    mkdir("/tmp/test_phase10_reparse", 0755);

    avc_config config;
    avc_config_init(&config);
    avc_error err;
    avc_error_clear(&err);

    ASSERT(avc_config_set(&config, "core", "repositoryformatversion", "0", &err) == AVC_OK,
           "set core.repoformatversion");
    ASSERT(avc_config_set(&config, "remote.origin", "url",
                           "https://example.com/repo.git", &err) == AVC_OK,
           "set remote.origin.url");
    ASSERT(avc_config_set(&config, "remote.origin", "fetch",
                           "+refs/heads/*:refs/remotes/origin/*", &err) == AVC_OK,
           "set remote.origin.fetch");

    char *cfg_path = "/tmp/test_phase10_reparse/config";
    ASSERT(avc_config_write(&config, cfg_path, &err) == AVC_OK,
           "write config with subsection");

    avc_config loaded;
    avc_config_init(&loaded);
    ASSERT(avc_config_load(&loaded, cfg_path, &err) == AVC_OK,
           "re-read config");

    const char *val = avc_config_get(&loaded, "core", "repositoryformatversion");
    ASSERT(val != NULL && strcmp(val, "0") == 0,
           "core.repoformatversion after re-read");

    val = avc_config_get(&loaded, "remote.origin", "url");
    ASSERT(val != NULL && strcmp(val, "https://example.com/repo.git") == 0,
           "remote.origin.url after re-read");

    val = avc_config_get(&loaded, "remote.origin", "fetch");
    ASSERT(val != NULL, "remote.origin.fetch after re-read");

    avc_config_free(&loaded);
    avc_config_free(&config);
    remove_dir("/tmp/test_phase10_reparse");
    PASS();
}

int main(void) {
    puts("Phase 10: Git Compatibility Layer");

    test_git_repo_detect();
    test_git_repo_open();
    test_git_read_object_loose();
    test_git_read_ref();
    test_git_resolve_head_detached();
    test_git_pack_open_fail();
    test_config_subsection();
    test_config_reparse();

    if (failures > 0) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    puts("\nAll tests passed.");
    return 0;
}
