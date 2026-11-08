#ifndef AVP_JAGUAR_FRAMEBUFFER_H
#define AVP_JAGUAR_FRAMEBUFFER_H

#include "avp_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Transitional software presentation surface for the faithful Jaguar path.
 * It deliberately lives outside X11.  The current storage is XRGB8888 so A5
 * remains behavior-neutral; a later fidelity pass can make CRY16 authoritative
 * without changing the host window/presentation API again. */
typedef struct AvpJaguarFramebuffer {
    unsigned width;
    unsigned height;
    u32 *pixels;
} AvpJaguarFramebuffer;

int avp_jaguar_framebuffer_init(AvpJaguarFramebuffer *fb,unsigned width,unsigned height);
void avp_jaguar_framebuffer_destroy(AvpJaguarFramebuffer *fb);
void avp_jaguar_framebuffer_clear(AvpJaguarFramebuffer *fb,u32 color);
size_t avp_jaguar_framebuffer_count(const AvpJaguarFramebuffer *fb);

#ifdef __cplusplus
}
#endif
#endif
