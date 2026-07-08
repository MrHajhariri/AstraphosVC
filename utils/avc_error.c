#include "utils/avc_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void avc_error_clear(avc_error *error) {
    if (error == NULL) {
        return;
    }
    error->code = AVC_OK;
    error->message[0] = '\0';
}

void avc_error_set(avc_error *error, avc_status code, const char *message) {
    if (error == NULL) {
        return;
    }
    error->code = code;
    if (message == NULL) {
        error->message[0] = '\0';
        return;
    }
    snprintf(error->message, sizeof(error->message), "%s", message);
}

void avc_error_setf(avc_error *error, avc_status code, const char *format, ...) {
    if (error == NULL) {
        return;
    }
    error->code = code;
    if (format == NULL) {
        error->message[0] = '\0';
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

const char *avc_status_name(avc_status status) {
    switch (status) {
    case AVC_OK:
        return "ok";
    case AVC_ERR_INVALID_ARGUMENT:
        return "invalid-argument";
    case AVC_ERR_NOT_FOUND:
        return "not-found";
    case AVC_ERR_ALREADY_EXISTS:
        return "already-exists";
    case AVC_ERR_IO:
        return "io";
    case AVC_ERR_PARSE:
        return "parse";
    case AVC_ERR_NO_MEMORY:
        return "no-memory";
    case AVC_ERR_UNSUPPORTED:
        return "unsupported";
    case AVC_ERR_CONFLICT:
        return "conflict";
    default:
        return "unknown";
    }
}
