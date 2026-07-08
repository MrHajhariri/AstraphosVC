#ifndef AVC_FS_H
#define AVC_FS_H

#include <stddef.h>

#include "utils/avc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AVC_REPOSITORY_DIR ".avc"

int avc_fs_is_directory(const char *path);
int avc_fs_is_regular_file(const char *path);
int avc_fs_path_exists(const char *path);
avc_status avc_fs_mkdir_p(const char *path, avc_error *error);
avc_status avc_fs_write_file(const char *path, const void *data, size_t size, avc_error *error);
avc_status avc_fs_read_file(const char *path, char **data, size_t *size, avc_error *error);
char *avc_fs_join(const char *left, const char *right);
char *avc_fs_parent(const char *path);
char *avc_fs_current_directory(avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
