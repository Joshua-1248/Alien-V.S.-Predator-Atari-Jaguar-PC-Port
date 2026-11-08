#ifndef AVP_JAGUAR_AVP_GPU_TRACKER_BRUSH_H
#define AVP_JAGUAR_AVP_GPU_TRACKER_BRUSH_H

#include "avp_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MAZE4.GAS: eight 128x60 monochrome allscope frames, 16 bytes per row.
 * The frame base advances by RING_B_WIDTH * RING_HEIGHT (16*60 bytes).
 * The Jaguar Blitter expands each set source bit to the RING_COLS color. */
int avp_gpu_tracker_ring_bit(const u8 *brush,size_t brush_size,
                             unsigned frame,unsigned x,unsigned y);

/* MAZE4.GAS::dot_loop selects one of eight BLIPS columns from the OR of
 * the absolute signed camera-space sprite x/z words in the GPU GMP block. */
unsigned avp_gpu_tracker_blip_index(s16 view_x,s16 view_z);

#ifdef __cplusplus
}
#endif
#endif
