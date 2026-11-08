/* RE25-S45: original retail $F34E-$F3D8 panel overlay BLITTER tail.
 * Entry is AFTER $BD44 has loaded the overlay into $1FA000.
 * No FILES/JP80 decoding, GPU scheduling, or live-game integration here. */
#ifndef AVP_JAG_PANEL_OVERLAY_H
#define AVP_JAG_PANEL_OVERLAY_H
#include "jaguar/avp_gpu/view3d.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JAG_PANEL_OVERLAY_SCRATCH 0x1FA000u
#define AVP_JAG_PANEL_OVERLAY_WIDTH 0x2EF5Eu
#define AVP_JAG_PANEL_OVERLAY_HEIGHT 0x2EF60u
#define AVP_JAG_PANEL_OVERLAY_A2_FLAGS 0x2EF62u
/* Caller must have already loaded the authentic overlay resource to scratch.
 * Coordinates are the two descriptor bytes (signed 8-bit) in the retail table.
 * This only schedules the original Blitter command; it never decompresses.
 * Returns zero without issuing a Blit for unsupported/incomplete state. */
int avp_jagpanel_overlay_postload(const AvpView3DBus *bus,
                                  uint8_t x, uint8_t y,
                                  uint32_t destination,
                                  int resource_ready, int blitter_ready);
#ifdef __cplusplus
}
#endif
#endif
