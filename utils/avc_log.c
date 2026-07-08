#include "utils/avc_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static avc_log_level g_log_level = AVC_LOG_WARN;

void avc_log_set_level(avc_log_level level) {
    g_log_level = level;
}

void avc_log_from_environment(void) {
    const char *value = getenv("ASTRAPHOSVC_LOG");
    if (value == NULL) {
        return;
    }
    if (strcmp(value, "error") == 0) {
        g_log_level = AVC_LOG_ERROR;
    } else if (strcmp(value, "warn") == 0) {
        g_log_level = AVC_LOG_WARN;
    } else if (strcmp(value, "info") == 0) {
        g_log_level = AVC_LOG_INFO;
    } else if (strcmp(value, "debug") == 0) {
        g_log_level = AVC_LOG_DEBUG;
    }
}

void avc_log(avc_log_level level, const char *format, ...) {
    if (level > g_log_level || format == NULL) {
        return;
    }

    const char *name = "error";
    if (level == AVC_LOG_WARN) {
        name = "warn";
    } else if (level == AVC_LOG_INFO) {
        name = "info";
    } else if (level == AVC_LOG_DEBUG) {
        name = "debug";
    }

    fprintf(stderr, "astraphosvc[%s]: ", name);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}
