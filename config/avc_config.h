#ifndef AVC_CONFIG_H
#define AVC_CONFIG_H

#include <stddef.h>

#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avc_config_entry {
    char *section;
    char *key;
    char *value;
} avc_config_entry;

typedef struct avc_config {
    avc_config_entry *entries;
    size_t count;
    size_t capacity;
} avc_config;

void avc_config_init(avc_config *config);
void avc_config_free(avc_config *config);
avc_status avc_config_set(avc_config *config, const char *section, const char *key, const char *value, avc_error *error);
const char *avc_config_get(const avc_config *config, const char *section, const char *key);
avc_status avc_config_load(avc_config *config, const char *path, avc_error *error);
avc_status avc_config_write(const avc_config *config, const char *path, avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
