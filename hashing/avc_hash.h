#ifndef AVC_HASH_H
#define AVC_HASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AVC_SHA1_BLOCK_SIZE 64
#define AVC_SHA1_DIGEST_SIZE 20

typedef struct avc_sha1_ctx {
    uint32_t state[5];
    uint64_t count;
    unsigned char buffer[AVC_SHA1_BLOCK_SIZE];
} avc_sha1_ctx;

void avc_sha1_init(avc_sha1_ctx *ctx);
void avc_sha1_update(avc_sha1_ctx *ctx, const void *data, size_t len);
void avc_sha1_final(avc_sha1_ctx *ctx, unsigned char digest[AVC_SHA1_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif
