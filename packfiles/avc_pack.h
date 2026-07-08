#ifndef AVC_PACK_H
#define AVC_PACK_H

#include "objects/avc_oid.h"
#include "objects/avc_object.h"
#include "utils/avc_error.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AVC_PACK_SIGNATURE "AVCPACK"
#define AVC_PACK_VERSION 1

avc_status avc_pack_write(avc_odb *odb,
                          const avc_oid *oids, int oid_count,
                          const char *pack_path, const char *idx_path,
                          avc_error *error);

avc_status avc_pack_read(avc_odb *odb,
                         const char *pack_path, const char *idx_path,
                         avc_error *error);

#ifdef __cplusplus
}
#endif

#endif
