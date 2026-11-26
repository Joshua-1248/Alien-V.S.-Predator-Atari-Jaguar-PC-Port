#ifndef AVP_JAGUAR_CRY_H
#define AVP_JAGUAR_CRY_H

#include "avp_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Jaguar TOM CRY16 color-space primitives.  These belong to the Jaguar
 * compatibility boundary rather than to any host windowing backend. */
void avp_jaguar_cry16_to_rgb(u16 cry,u8 *r,u8 *g,u8 *b);
u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *qconv,size_t qconv_size);
u16 avp_jaguar_srcshade_cry(u16 src,int intensity_delta);

/* Jaguar RMW semantics used by AVP's HUD path.  The first operation is the
 * hardware-space CRY blend.  The XRGB8888 helper is only the compatibility
 * adapter for the current software framebuffer and can disappear once the
 * authoritative framebuffer is kept in CRY form. */
u16 avp_jaguar_rmw_blend_cry(u16 dst,u16 src);
void avp_jaguar_build_hud_rmw_palette(const u16 source_palette[32],int brightness,u16 out[32]);
u32 avp_jaguar_rmw_xrgb8888(u32 dst,u16 src,const u8 *qconv,size_t qconv_size);
/* PC-only visual alternative. Lock the destination CRY hue to neutral gray
 * before the original RMW ink modifier; preserve the actual backdrop luma.
 * Never use for Jaguar object-list emulation or with the option disabled. */
u32 avp_jaguar_rmw_fixed_hue_xrgb8888(u32 dst,u16 src,
                                       const u8 *qconv,size_t qconv_size);

/* MAZESCRN.S::MazeList X_FADE: reproduce the CRY modifier written to the
 * hardware CLUT from the existing player damage/vision and fade state. */
u16 avp_jaguar_game_clut(u16 fade_color,u16 extra_color,s16 fade_level);

/* TOM VMODE: RGB16 consumes each 16-bit Jaguar line-buffer word as 5:6:5;
 * RGB24 consumes two consecutive words as G,R,unused,B.  The original
 * Predator multispectrum table selects both VMODE and the CRY modifier. */
u32 avp_jaguar_vmode_rgb16(u16 word);
u32 avp_jaguar_vmode_rgb24(u16 first,u16 second);


#ifdef __cplusplus
}
#endif
#endif
