#define _POSIX_C_SOURCE 200809L

#include "api/astraphosvc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void hex_digest(const unsigned char digest[20], char out[41]) {
    for (int i = 0; i < 20; i++) {
        sprintf(out + i * 2, "%02x", digest[i]);
    }
    out[40] = '\0';
}

static void test_sha1_empty(void) {
    avc_sha1_ctx ctx;
    unsigned char digest[20];
    avc_sha1_init(&ctx);
    avc_sha1_final(&ctx, digest);

    char hex[41];
    hex_digest(digest, hex);
    assert(strcmp(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);
}

static void test_sha1_abc(void) {
    avc_sha1_ctx ctx;
    unsigned char digest[20];
    avc_sha1_init(&ctx);
    avc_sha1_update(&ctx, "abc", 3);
    avc_sha1_final(&ctx, digest);

    char hex[41];
    hex_digest(digest, hex);
    assert(strcmp(hex, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
}

static void test_sha1_two_updates(void) {
    avc_sha1_ctx ctx;
    unsigned char digest[20];
    avc_sha1_init(&ctx);
    avc_sha1_update(&ctx, "The quick brown fox ", 20);
    avc_sha1_update(&ctx, "jumps over the lazy dog", 23);
    avc_sha1_final(&ctx, digest);

    char hex[41];
    hex_digest(digest, hex);
    assert(strcmp(hex, "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12") == 0);
}

static void test_sha1_blob_header(void) {
    const char *content = "hello\n";
    size_t content_len = 6;
    char header[64];
    int header_len = snprintf(header, sizeof(header), "blob %zu",
                              content_len);
    assert(header_len > 0);
    header[header_len] = '\0';

    avc_sha1_ctx ctx;
    avc_sha1_init(&ctx);
    avc_sha1_update(&ctx, header, (size_t)header_len + 1);
    avc_sha1_update(&ctx, content, content_len);

    unsigned char digest[20];
    avc_sha1_final(&ctx, digest);

    avc_oid oid;
    memcpy(oid, digest, 20);
    char hex[41];
    avc_oid_hex(oid, hex);

    assert(strlen(hex) == 40);
    assert(avc_oid_parse(hex, oid) == 0);
    assert(avc_oid_is_zero(oid) == 0);
}

static void test_oid_round_trip(void) {
    avc_oid oid = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                   0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                   0x01, 0x23, 0x45, 0x67};
    char hex[41];
    avc_oid_hex(oid, hex);
    assert(strcmp(hex, "0123456789abcdef0123456789abcdef01234567") == 0);

    avc_oid parsed;
    assert(avc_oid_parse(hex, parsed) == 0);
    assert(avc_oid_cmp(oid, parsed) == 0);
}

static void test_object_write_read_blob(const char *root) {
    avc_repository repo;
    avc_error error;
    avc_error_clear(&error);

    avc_repository_init(root, "main", &repo, &error);
    assert(error.code == AVC_OK);
    assert(repo.objects_path != NULL);

    avc_odb odb;
    avc_odb_init(&odb);
    avc_odb_open(&odb, repo.objects_path, &error);
    assert(error.code == AVC_OK);

    const char *data = "hello world\n";
    size_t data_len = 12;

    avc_oid oid1, oid2;
    avc_error_clear(&error);
    avc_status status = avc_odb_write_blob(&odb, data, data_len, oid1,
                                            &error);
    assert(status == AVC_OK);

    assert(avc_odb_exists(&odb, oid1));

    status = avc_odb_write_blob(&odb, data, data_len, oid2, &error);
    assert(status == AVC_OK);
    assert(avc_oid_cmp(oid1, oid2) == 0);

    avc_object_type read_type;
    void *payload = NULL;
    size_t payload_size = 0;
    avc_error_clear(&error);
    status = avc_odb_read(&odb, oid1, &read_type, &payload, &payload_size,
                          &error);
    assert(status == AVC_OK);
    assert(read_type == AVC_OBJECT_BLOB);
    assert(payload_size == data_len);
    assert(memcmp(payload, data, data_len) == 0);

    free(payload);

    avc_oid nonexistent;
    memset(nonexistent, 0xff, 20);
    status = avc_odb_read(&odb, nonexistent, &read_type, &payload,
                          &payload_size, &error);
    assert(status == AVC_ERR_NOT_FOUND);

    avc_odb_close(&odb);
    avc_repository_free(&repo);
}

static void test_object_types(const char *root) {
    avc_repository repo;
    avc_error error;
    avc_error_clear(&error);

    avc_repository_init(root, "main", &repo, &error);
    assert(error.code == AVC_OK);

    avc_odb odb;
    avc_odb_init(&odb);
    avc_odb_open(&odb, repo.objects_path, &error);
    assert(error.code == AVC_OK);

    const char *payloads[] = {"blob content", "tree content",
                              "commit content", "tag content"};
    avc_object_type types[] = {AVC_OBJECT_BLOB, AVC_OBJECT_TREE,
                               AVC_OBJECT_COMMIT, AVC_OBJECT_TAG};

    for (int i = 0; i < 4; i++) {
        avc_oid oid;
        avc_error_clear(&error);
        avc_status status = avc_odb_write(&odb, types[i], payloads[i],
                                           strlen(payloads[i]), oid, &error);
        assert(status == AVC_OK);
        assert(avc_odb_exists(&odb, oid));

        avc_object_type read_type;
        void *payload = NULL;
        size_t payload_size = 0;
        avc_error_clear(&error);
        status = avc_odb_read(&odb, oid, &read_type, &payload,
                              &payload_size, &error);
        assert(status == AVC_OK);
        assert(read_type == types[i]);
        assert(payload_size == strlen(payloads[i]));
        assert(memcmp(payload, payloads[i], payload_size) == 0);
        free(payload);
    }

    assert(strcmp(avc_object_type_name(AVC_OBJECT_BLOB), "blob") == 0);
    assert(strcmp(avc_object_type_name(AVC_OBJECT_TREE), "tree") == 0);
    assert(strcmp(avc_object_type_name(AVC_OBJECT_COMMIT), "commit") == 0);
    assert(strcmp(avc_object_type_name(AVC_OBJECT_TAG), "tag") == 0);

    avc_object_type t;
    assert(avc_object_type_from_name("blob", &t) == 0 && t == AVC_OBJECT_BLOB);
    assert(avc_object_type_from_name("commit", &t) == 0 && t == AVC_OBJECT_COMMIT);
    assert(avc_object_type_from_name("unknown", &t) != 0);

    avc_odb_close(&odb);
    avc_repository_free(&repo);
}

static void test_compress_round_trip(void) {
    avc_error error;
    avc_error_clear(&error);

    const char *data = "The quick brown fox jumps over the lazy dog";
    size_t data_len = strlen(data);

    void *compressed = NULL;
    size_t compressed_size = 0;
    avc_status status = avc_compress(data, data_len, &compressed,
                                      &compressed_size, &error);
    assert(status == AVC_OK);
    assert(compressed != NULL);
    assert(compressed_size > 0);

    void *decompressed = NULL;
    size_t decompressed_size = 0;
    avc_error_clear(&error);
    status = avc_decompress(compressed, compressed_size, &decompressed,
                            &decompressed_size, &error);
    assert(status == AVC_OK);
    assert(decompressed_size == data_len);
    assert(memcmp(decompressed, data, data_len) == 0);

    free(compressed);
    free(decompressed);
}

int main(void) {
    test_sha1_empty();
    test_sha1_abc();
    test_sha1_two_updates();
    test_sha1_blob_header();
    test_oid_round_trip();

    char template[] = "/tmp/astraphosvc-test2-XXXXXX";
    char *root = mkdtemp(template);
    assert(root != NULL);

    test_compress_round_trip();
    test_object_write_read_blob(root);
    test_object_types(root);

    puts("phase2 tests passed");
    return 0;
}
