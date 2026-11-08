#ifndef AVP_JAGUAR_AVP_GPU_FRONTEND_FADE_H
#define AVP_JAGUAR_AVP_GPU_FRONTEND_FADE_H

#include "avp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exact packed-RGB16 arithmetic used by the retail MJP GPU cross/fade jobs.
 * Jaguar RGB16 is R5:B5:G6: bits 15..11 red, 10..6 blue, 5..0 green.
 *
 * The GPU handlers do not multiply by k/16.  They sum independently truncated
 * binary fractions (1/2, 1/4, 1/8, 1/16) under channel-safe masks.  These
 * helpers deliberately preserve that bitwise behavior.
 */
u16 avp_gpu_rgb16_pack(u8 r,u8 g,u8 b);
void avp_gpu_rgb16_unpack(u16 pixel,u8 *r,u8 *g,u8 *b);
u16 avp_gpu_rgb16_fraction(u16 pixel,unsigned phase);
u16 avp_gpu_rgb16_cross(u16 from,u16 to,unsigned phase);
u16 avp_gpu_rgb16_scene_cross(u16 old_px,u16 new_px,unsigned stage);
/* RE24-S10: retail title->menu dim.  Executed on the jagref reference
 * machine, the retail program dims the retained $125000 title bitmap IN PLACE
 * with three successive GPU fade passes, 15/16 then 13/16 then 11/16, each
 * applied to the previous pass's result (all 76800 pixels verified).
 * `passes` = number of completed passes (0..3). */
u16 avp_gpu_rgb16_title_menu_dim(u16 pixel,unsigned passes);
unsigned avp_gpu_title_menu_dim_passes(unsigned elapsed_vbls);

/* Frozen 80-line CROSSX seed table recovered from the exact retail GPU payload.
 * CROSSX advances the signed line accumulator once per GPU pass.  The phase
 * mapper below is the exact dispatch transform performed before the 17-way
 * blend table is indexed. */
int avp_gpu_crossx_seed(unsigned line);
unsigned avp_gpu_crossx_dispatch_phase(int accumulator);

#ifdef __cplusplus
}
#endif

#endif
