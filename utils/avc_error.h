#ifndef AVC_ERROR_H
#define AVC_ERROR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum avc_status {
    AVC_OK = 0,
    AVC_ERR_INVALID_ARGUMENT,
    AVC_ERR_NOT_FOUND,
    AVC_ERR_ALREADY_EXISTS,
    AVC_ERR_IO,
    AVC_ERR_PARSE,
    AVC_ERR_NO_MEMORY,
    AVC_ERR_UNSUPPORTED,
    AVC_ERR_CONFLICT
} avc_status;

typedef struct avc_error {
    avc_status code;
    char message[256];
} avc_error;

void avc_error_clear(avc_error *error);
void avc_error_set(avc_error *error, avc_status code, const char *message);
void avc_error_setf(avc_error *error, avc_status code, const char *format, ...);
const char *avc_status_name(avc_status status);

#ifdef __cplusplus
}
#endif

#endif
