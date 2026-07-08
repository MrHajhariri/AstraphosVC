#include "hashing/avc_hash.h"

#include <string.h>

static uint32_t left_rotate(uint32_t value, unsigned int count) {
    return (value << count) | (value >> (32 - count));
}

void avc_sha1_init(avc_sha1_ctx *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
}

static void sha1_process_block(avc_sha1_ctx *ctx, const unsigned char block[64]) {
    uint32_t W[80];
    uint32_t A, B, C, D, E, T;

    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }

    for (int i = 16; i < 80; i++) {
        W[i] = left_rotate(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (B & C) | ((~B) & D);
            k = 0x5A827999;
        } else if (i < 40) {
            f = B ^ C ^ D;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (B & C) | (B & D) | (C & D);
            k = 0x8F1BBCDC;
        } else {
            f = B ^ C ^ D;
            k = 0xCA62C1D6;
        }

        T = left_rotate(A, 5) + f + E + k + W[i];
        E = D;
        D = C;
        C = left_rotate(B, 30);
        B = A;
        A = T;
    }

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
}

void avc_sha1_update(avc_sha1_ctx *ctx, const void *data, size_t len) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t idx = (size_t)((ctx->count / 8) % 64);

    ctx->count += (uint64_t)len * 8;

    if (idx > 0 && idx + len >= 64) {
        size_t to_copy = 64 - idx;
        memcpy(ctx->buffer + idx, bytes, to_copy);
        sha1_process_block(ctx, ctx->buffer);
        bytes += to_copy;
        len -= to_copy;
        idx = 0;
    }

    while (len >= 64) {
        sha1_process_block(ctx, bytes);
        bytes += 64;
        len -= 64;
    }

    if (len > 0) {
        memcpy(ctx->buffer + idx, bytes, len);
    }
}

void avc_sha1_final(avc_sha1_ctx *ctx, unsigned char digest[20]) {
    size_t idx = (size_t)((ctx->count / 8) % 64);

    ctx->buffer[idx++] = 0x80;

    if (idx > 56) {
        memset(ctx->buffer + idx, 0, 64 - idx);
        sha1_process_block(ctx, ctx->buffer);
        idx = 0;
    }

    memset(ctx->buffer + idx, 0, 56 - idx);

    uint64_t bits = ctx->count;
    ctx->buffer[56] = (unsigned char)(bits >> 56);
    ctx->buffer[57] = (unsigned char)(bits >> 48);
    ctx->buffer[58] = (unsigned char)(bits >> 40);
    ctx->buffer[59] = (unsigned char)(bits >> 32);
    ctx->buffer[60] = (unsigned char)(bits >> 24);
    ctx->buffer[61] = (unsigned char)(bits >> 16);
    ctx->buffer[62] = (unsigned char)(bits >> 8);
    ctx->buffer[63] = (unsigned char)(bits);

    sha1_process_block(ctx, ctx->buffer);

    for (int i = 0; i < 5; i++) {
        digest[i * 4]     = (unsigned char)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)(ctx->state[i]);
    }
}
