#ifndef AVC_COMPRESS_H
#define AVC_COMPRESS_H

#include <stddef.h>

#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

avc_status avc_compress(const void *input, size_t input_size,
                        void **output, size_t *output_size,
                        avc_error *error);

avc_status avc_decompress(const void *input, size_t input_size,
                          void **output, size_t *output_size,
                          avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
