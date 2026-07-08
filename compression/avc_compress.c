#include "compression/avc_compress.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

avc_status avc_compress(const void *input, size_t input_size,
                        void **output, size_t *output_size,
                        avc_error *error) {
    if (input == NULL || output == NULL || output_size == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "compress received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    uLong bound = compressBound((uLong)input_size);
    size_t alloc = bound + 4;
    unsigned char *buf = (unsigned char *)malloc(alloc);
    if (buf == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory for compression");
        return AVC_ERR_NO_MEMORY;
    }

    buf[0] = (unsigned char)(input_size >> 24);
    buf[1] = (unsigned char)(input_size >> 16);
    buf[2] = (unsigned char)(input_size >> 8);
    buf[3] = (unsigned char)(input_size);

    uLongf dest_len = (uLongf)bound;
    int ret = compress2(buf + 4, &dest_len, (const unsigned char *)input,
                        (uLong)input_size, Z_BEST_COMPRESSION);
    if (ret != Z_OK) {
        free(buf);
        avc_error_set(error, AVC_ERR_IO, "compression failed");
        return AVC_ERR_IO;
    }

    *output = buf;
    *output_size = dest_len + 4;
    return AVC_OK;
}

avc_status avc_decompress(const void *input, size_t input_size,
                          void **output, size_t *output_size,
                          avc_error *error) {
    if (input == NULL || output == NULL || output_size == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "decompress received null");
        return AVC_ERR_INVALID_ARGUMENT;
    }
    if (input_size < 4) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT,
                      "compressed data too small");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    const unsigned char *bytes = (const unsigned char *)input;
    size_t uncompressed_size = ((size_t)bytes[0] << 24) |
                               ((size_t)bytes[1] << 16) |
                               ((size_t)bytes[2] << 8) |
                               ((size_t)bytes[3]);

    unsigned char *buf = (unsigned char *)malloc(uncompressed_size + 1);
    if (buf == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY,
                      "out of memory for decompression");
        return AVC_ERR_NO_MEMORY;
    }

    uLongf dest_len = (uLongf)uncompressed_size;
    int ret = uncompress(buf, &dest_len, bytes + 4,
                         (uLong)(input_size - 4));
    if (ret != Z_OK) {
        free(buf);
        avc_error_set(error, AVC_ERR_IO, "decompression failed");
        return AVC_ERR_IO;
    }

    buf[uncompressed_size] = '\0';
    *output = buf;
    *output_size = uncompressed_size;
    return AVC_OK;
}
