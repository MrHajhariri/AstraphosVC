#ifndef AVC_LOG_H
#define AVC_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum avc_log_level {
    AVC_LOG_ERROR = 0,
    AVC_LOG_WARN = 1,
    AVC_LOG_INFO = 2,
    AVC_LOG_DEBUG = 3
} avc_log_level;

void avc_log_set_level(avc_log_level level);
void avc_log_from_environment(void);
void avc_log(avc_log_level level, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
