#ifndef AVP_PC_TITLE_NATIVE_H
#define AVP_PC_TITLE_NATIVE_H
/* R82: first-boot retail title video through the already present Jaguar
 * reference machine. The executable does not contain cartridge content.
 * If AVP_BUILD_JAG_REFERENCE=OFF, these calls safely fall back to the
 * independently reconstructed PC title renderer. */
#include "avp_rom.h"
#include "jaguar/framebuffer.h"
#include <stdint.h>

typedef struct AvpPcTitleNative AvpPcTitleNative;
AvpPcTitleNative *avp_pc_title_native_create(const AvpRomImage *rom);
void avp_pc_title_native_destroy(AvpPcTitleNative *title);
void avp_pc_title_native_advance_boot(AvpPcTitleNative *title);
int avp_pc_title_native_rewind(AvpPcTitleNative *title);
/* Returns one if the original ROM-executed scanout was copied. Returning
 * zero leaves the caller's existing PC title composition untouched. */
int avp_pc_title_native_present(AvpPcTitleNative *title,
    AvpJaguarFramebuffer *fb,unsigned title_tick);
/* Cleanly drop the ROM/CPU renderer when the player skips ahead. */
void avp_pc_title_native_stop(AvpPcTitleNative *title);
/* No-ROM layout regression seam: source stride is in pixels, and the Jaguar
 * visible 320x240 is stored at framebuffer row 14 for the PC video scaler. */
int avp_pc_title_native_copy_rgb(const uint32_t *source,unsigned width,
    unsigned height,unsigned stride,AvpJaguarFramebuffer *dest);
#endif
