#include "utils/avc_fs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define getcwd _getcwd
#define mkdir_one(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define mkdir_one(path) mkdir((path), 0777)
#endif

static char *avc_strdup(const char *value) {
    if (value == NULL) {
        return NULL;
    }
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);
    if (copy != NULL) {
        memcpy(copy, value, length);
    }
    return copy;
}

int avc_fs_path_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0;
}

int avc_fs_is_directory(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int avc_fs_is_regular_file(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

avc_status avc_fs_mkdir_p(const char *path, avc_error *error) {
    if (path == NULL || path[0] == '\0') {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "directory path is empty");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    char *copy = avc_strdup(path);
    if (copy == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory creating directory path");
        return AVC_ERR_NO_MEMORY;
    }

    for (char *p = copy + 1; *p != '\0'; ++p) {
        if (*p != '/' && *p != '\\') {
            continue;
        }
        char saved = *p;
        *p = '\0';
        if (copy[0] != '\0' && !avc_fs_is_directory(copy) && mkdir_one(copy) != 0 && errno != EEXIST) {
            avc_error_setf(error, AVC_ERR_IO, "failed to create directory '%s': %s", copy, strerror(errno));
            free(copy);
            return AVC_ERR_IO;
        }
        *p = saved;
    }

    if (!avc_fs_is_directory(copy) && mkdir_one(copy) != 0 && errno != EEXIST) {
        avc_error_setf(error, AVC_ERR_IO, "failed to create directory '%s': %s", copy, strerror(errno));
        free(copy);
        return AVC_ERR_IO;
    }

    free(copy);
    return AVC_OK;
}

avc_status avc_fs_write_file(const char *path, const void *data, size_t size, avc_error *error) {
    if (path == NULL || data == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "write_file received null argument");
        return AVC_ERR_INVALID_ARGUMENT;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        avc_error_setf(error, AVC_ERR_IO, "failed to open '%s' for writing: %s", path, strerror(errno));
        return AVC_ERR_IO;
    }
    if (size > 0 && fwrite(data, 1, size, file) != size) {
        avc_error_setf(error, AVC_ERR_IO, "failed to write '%s': %s", path, strerror(errno));
        fclose(file);
        return AVC_ERR_IO;
    }
    if (fclose(file) != 0) {
        avc_error_setf(error, AVC_ERR_IO, "failed to close '%s': %s", path, strerror(errno));
        return AVC_ERR_IO;
    }
    return AVC_OK;
}

avc_status avc_fs_read_file(const char *path, char **data, size_t *size, avc_error *error) {
    if (path == NULL || data == NULL || size == NULL) {
        avc_error_set(error, AVC_ERR_INVALID_ARGUMENT, "read_file received null argument");
        return AVC_ERR_INVALID_ARGUMENT;
    }
    *data = NULL;
    *size = 0;

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        if (errno == ENOENT || errno == ENOTDIR) {
            avc_error_setf(error, AVC_ERR_NOT_FOUND, "file not found: %s", path);
            return AVC_ERR_NOT_FOUND;
        }
        avc_error_setf(error, AVC_ERR_IO, "failed to open '%s' for reading: %s", path, strerror(errno));
        return AVC_ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        avc_error_setf(error, AVC_ERR_IO, "failed to seek '%s'", path);
        fclose(file);
        return AVC_ERR_IO;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        avc_error_setf(error, AVC_ERR_IO, "failed to size '%s'", path);
        fclose(file);
        return AVC_ERR_IO;
    }

    char *buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory reading file");
        fclose(file);
        return AVC_ERR_NO_MEMORY;
    }
    if (length > 0 && fread(buffer, 1, (size_t)length, file) != (size_t)length) {
        avc_error_setf(error, AVC_ERR_IO, "failed to read '%s'", path);
        free(buffer);
        fclose(file);
        return AVC_ERR_IO;
    }
    buffer[length] = '\0';
    fclose(file);
    *data = buffer;
    *size = (size_t)length;
    return AVC_OK;
}

char *avc_fs_join(const char *left, const char *right) {
    if (left == NULL || left[0] == '\0') {
        return avc_strdup(right);
    }
    if (right == NULL || right[0] == '\0') {
        return avc_strdup(left);
    }
    size_t left_len = strlen(left);
    size_t right_len = strlen(right);
    int needs_sep = left[left_len - 1] != '/' && left[left_len - 1] != '\\';
    char *result = malloc(left_len + (needs_sep ? 1U : 0U) + right_len + 1U);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, left, left_len);
    size_t offset = left_len;
    if (needs_sep) {
        result[offset++] = '/';
    }
    memcpy(result + offset, right, right_len + 1U);
    return result;
}

char *avc_fs_parent(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    char *copy = avc_strdup(path);
    if (copy == NULL) {
        return NULL;
    }
    size_t length = strlen(copy);
    while (length > 1 && (copy[length - 1] == '/' || copy[length - 1] == '\\')) {
        copy[--length] = '\0';
    }
    char *slash = strrchr(copy, '/');
#ifdef _WIN32
    char *backslash = strrchr(copy, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash)) {
        slash = backslash;
    }
#endif
    if (slash == NULL) {
        free(copy);
        return avc_strdup(".");
    }
    if (slash == copy) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
    return copy;
}

int avc_fs_stat(const char *path, avc_file_stat *statbuf) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    if (statbuf != NULL) {
        statbuf->dev = (uint32_t)st.st_dev;
        statbuf->ino = (uint32_t)st.st_ino;
        statbuf->mode = (uint32_t)st.st_mode;
        statbuf->uid = (uint32_t)st.st_uid;
        statbuf->gid = (uint32_t)st.st_gid;
        statbuf->size = (uint32_t)st.st_size;
        statbuf->ctime_sec = (uint32_t)st.st_ctime;
        statbuf->mtime_sec = (uint32_t)st.st_mtime;
    }
    return 0;
}

char *avc_fs_current_directory(avc_error *error) {
    size_t size = 256;
    for (;;) {
        char *buffer = malloc(size);
        if (buffer == NULL) {
            avc_error_set(error, AVC_ERR_NO_MEMORY, "out of memory obtaining current directory");
            return NULL;
        }
        if (getcwd(buffer, size) != NULL) {
            return buffer;
        }
        free(buffer);
        if (errno != ERANGE) {
            avc_error_setf(error, AVC_ERR_IO, "failed to obtain current directory: %s", strerror(errno));
            return NULL;
        }
        size *= 2;
    }
}
