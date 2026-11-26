#ifndef AVP_PC_VIGNETTE_H
#define AVP_PC_VIGNETTE_H

#include <stddef.h>
#include <stdint.h>

/* Host-only post-process. Never modifies Jaguar framebuffers, game simulation,
 * or the F12 menu. One reusable radial mask per local-player viewport. */
typedef struct AvpPcVignetteMask {
    uint8_t *weights;
    unsigned width, height;
    /* R390: cached outer-edge spans for the current strength. Rows have a
     * central zero-opacity interval that must not be touched each frame. */
    unsigned *left_count,*right_count;
    unsigned span_strength,span_valid;
} AvpPcVignetteMask;

int avp_pc_vignette_prepare(AvpPcVignetteMask *cache,unsigned width,unsigned height);
void avp_pc_vignette_destroy(AvpPcVignetteMask *cache);
void avp_pc_vignette_apply(AvpPcVignetteMask *cache,
    uint32_t *pixels,unsigned pitch,unsigned frame_width,unsigned frame_height,
    unsigned x,unsigned y,unsigned width,unsigned height,unsigned strength);

#endif
