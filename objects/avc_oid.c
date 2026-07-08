#include "objects/avc_oid.h"

#include <stdio.h>

void avc_oid_hex(const avc_oid oid, char hex[AVC_OID_HEX_SIZE]) {
    for (int i = 0; i < AVC_OID_BYTES; i++) {
        sprintf(hex + i * 2, "%02x", oid[i]);
    }
    hex[AVC_OID_HEX_SIZE - 1] = '\0';
}

int avc_oid_parse(const char hex[AVC_OID_HEX_SIZE], avc_oid oid) {
    for (int i = 0; i < AVC_OID_BYTES; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) {
            return -1;
        }
        oid[i] = (unsigned char)byte;
    }
    return 0;
}
