#ifndef AVC_OID_H
#define AVC_OID_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AVC_OID_BYTES 20
#define AVC_OID_HEX_SIZE 41

typedef unsigned char avc_oid[AVC_OID_BYTES];

void avc_oid_hex(const avc_oid oid, char hex[AVC_OID_HEX_SIZE]);
int avc_oid_parse(const char hex[AVC_OID_HEX_SIZE], avc_oid oid);

static inline int avc_oid_cmp(const avc_oid a, const avc_oid b) {
    return memcmp(a, b, AVC_OID_BYTES);
}

static inline int avc_oid_is_zero(const avc_oid oid) {
    for (int i = 0; i < AVC_OID_BYTES; i++) {
        if (oid[i] != 0) return 0;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif
