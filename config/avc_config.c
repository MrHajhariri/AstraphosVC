#include "config/avc_config.h"

#include "utils/avc_fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *avc_strdup_local(const char *value) {
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);
    if (copy != NULL) {
        memcpy(copy, value, length);
    }
    return copy;
}

static char *trim(char *value) {
    while (isspace((unsigned char)*value)) {
        ++value;
    }
    char *end = value + strlen(value);
    while (end > value && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return value;
}

void avc_config_init(avc_config *config) {
    if (config == NULL) {
        return;
    }
    config->entries = NULL;
    config->count = 0;
    config->capacity = 0;
}

void avc_config_free(avc_config *config) {
    if (config == NULL) {
        return;
    }
    for (size_t i = 0; i < config->count; ++i) {
        free(config->entries[i].section);
        free(config->entries[i].key);
        free(config->entries[i].value);
    }
    free(config->entries);
    avc_config_init(config);
}

static size_t find_entry_index(const avc_config *config, const char *section, const char *key) {
    for (size_t i = 0; i < config->count; ++i) {
        if (strcmp(config->entries[i].section, section) == 0 && strcmp(config->entries[i].key, key) == 0) {
            return i;
        }
    }
    return config->count;
}

avc_status avc_config_set(avc_config *config, const char *section, const char *key, const char *value, avc_error *error) {
    if (config == NULL || section == NULL || key == NULL || value == NULL || section[0] == '\0' || key[0] == '\0') {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "invalid config key");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    size_t index = find_entry_index(config, section, key);
    if (index < config->count) {
        char *replacement = avc_strdup_local(value);
        if (replacement == NULL) {
            avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory replacing config value");
            return AVC_ERR_NO_MEMORY;
        }
        free(config->entries[index].value);
        config->entries[index].value = replacement;
        return AVC_OK;
    }

    if (config->count == config->capacity) {
        size_t next_capacity = config->capacity == 0 ? 8 : config->capacity * 2;
        avc_config_entry *entries = realloc(config->entries, next_capacity * sizeof(*entries));
        if (entries == NULL) {
            avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory growing config");
            return AVC_ERR_NO_MEMORY;
        }
        config->entries = entries;
        config->capacity = next_capacity;
    }

    avc_config_entry *entry = &config->entries[config->count];
    entry->section = avc_strdup_local(section);
    entry->key = avc_strdup_local(key);
    entry->value = avc_strdup_local(value);
    if (entry->section == NULL || entry->key == NULL || entry->value == NULL) {
        free(entry->section);
        free(entry->key);
        free(entry->value);
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory adding config entry");
        return AVC_ERR_NO_MEMORY;
    }
    config->count++;
    return AVC_OK;
}

const char *avc_config_get(const avc_config *config, const char *section, const char *key) {
    if (config == NULL || section == NULL || key == NULL) {
        return NULL;
    }
    size_t index = find_entry_index(config, section, key);
    return index < config->count ? config->entries[index].value : NULL;
}

avc_status avc_config_load(avc_config *config, const char *path, avc_error *error) {
    if (config == NULL || path == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "invalid config load arguments");
        return AVC_ERR_INVALID_ARGUMENT;
    }
    char *data = NULL;
    size_t size = 0;
    avc_status status = avc_fs_read_file(path, &data, &size, error);
    if (status != AVC_OK) {
        return status;
    }

    char section[128] = "core";
    char *cursor = data;
    while (cursor < data + size) {
        char *line = cursor;
        char *newline = strchr(cursor, '\n');
        if (newline == NULL) {
            cursor = data + size;
        } else {
            *newline = '\0';
            cursor = newline + 1;
        }
        char *text = trim(line);
        if (text[0] == '\0' || text[0] == '#' || text[0] == ';') {
            continue;
        }
        if (text[0] == '[') {
            char *end = strchr(text, ']');
            if (end == NULL) {
                free(data);
                avc_error_set(error, AVC_ERR_PARSE, "unterminated config section");
                return AVC_ERR_PARSE;
            }
            *end = '\0';
            snprintf(section, sizeof(section), "%s", trim(text + 1));
            continue;
        }
        char *equals = strchr(text, '=');
        if (equals == NULL) {
            free(data);
            avc_error_set(error, AVC_ERR_PARSE, "config entry missing '='");
            return AVC_ERR_PARSE;
        }
        *equals = '\0';
        status = avc_config_set(config, section, trim(text), trim(equals + 1), error);
        if (status != AVC_OK) {
            free(data);
            return status;
        }
    }

    free(data);
    return AVC_OK;
}

avc_status avc_config_write(const avc_config *config, const char *path, avc_error *error) {
    if (config == NULL || path == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "invalid config write arguments");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    size_t capacity = 256;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory writing config");
        return AVC_ERR_NO_MEMORY;
    }
    size_t length = 0;
    const char *current_section = NULL;
    for (size_t i = 0; i < config->count; ++i) {
        const avc_config_entry *entry = &config->entries[i];
        int need_section = current_section == NULL || strcmp(current_section, entry->section) != 0;
        size_t needed = strlen(entry->section) + strlen(entry->key) + strlen(entry->value) + 16;
        if (length + needed >= capacity) {
            while (length + needed >= capacity) {
                capacity *= 2;
            }
            char *grown = realloc(buffer, capacity);
            if (grown == NULL) {
                free(buffer);
                avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory growing config text");
                return AVC_ERR_NO_MEMORY;
            }
            buffer = grown;
        }
        if (need_section) {
            length += (size_t)snprintf(buffer + length, capacity - length, "%s[%s]\n", length == 0 ? "" : "\n", entry->section);
            current_section = entry->section;
        }
        length += (size_t)snprintf(buffer + length, capacity - length, "%s = %s\n", entry->key, entry->value);
    }

    avc_status status = avc_fs_write_file(path, buffer, length, error);
    free(buffer);
    return status;
}
