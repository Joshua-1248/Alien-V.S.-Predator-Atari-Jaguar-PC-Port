#ifndef AVP_ROM_H
#define AVP_ROM_H

#include "avp_types.h"
#include <stddef.h>

/* Host-independent view of the user-supplied retail cartridge image.
 * Platform code owns how the bytes are loaded; AVP/Jaguar code may inspect
 * this verified in-memory image without depending on X11/Linux/filesystem APIs. */
typedef struct AvpRomImage {
    u8 *data;
    size_t size;
    int exact_retail_sha256;
    char sha256_hex[65];
} AvpRomImage;

#endif
