#ifndef AVP_JAGUAR_AVP_GPU_THERMAL_VISION_H
#define AVP_JAGUAR_AVP_GPU_THERMAL_VISION_H
#include "avp_types.h"
#include "amp.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* RE39 R162: PC presentation only; retail multispectrum modes and simulation
 * unchanged. On-screen screenshot and Jaguar X_FADE pixel-word comparison:
 * blue Thermal uses {VMODE=06c7, extra=26f8}, pink/yellow Electromagnetic
 * uses {VMODE=06c7, extra=9558}. The latter was previously (incorrectly)
 * identified as Thermal; the old Electromagnetic selector hit Night Vision.
 * Returned style: 0=original, 1=Enhanced Thermal, 2=Enhanced Electromagnetic,
 * 3=Enhanced Infrared (new R164 extended selector only).
 */
enum { AVP_VISION_ORIGINAL=0, AVP_VISION_ENHANCED_THERMAL=1,
       AVP_VISION_ENHANCED_ELECTROMAGNETIC=2,
       AVP_VISION_ENHANCED_INFRARED=3 };
/* R166: independent scenery-only false-color pass. The style selector always
 * recognizes the three ORIGINAL register pairs, regardless of actor toggles.
 * The source mask is 0 for environment and nonzero for any foreground pixel. */
int avp_predator_background_style(int enabled,int species,unsigned cloak_state,
                                   u16 vmode,u16 xfade);
u32 avp_predator_background_color(unsigned style,u32 original,
                                   unsigned native_x,unsigned native_y);
void avp_predator_background_composite_rgb16(u32 *scene,const u32 *mask,
                                   unsigned width,unsigned height,unsigned style);
/* R175: Infrared background only: real renderer depth, not texture brightness,
 * determines cool (far) versus red/pink (near). Wall distances are the active
 * nearest-visible-geometry depth map; FLT_MAX means use projected flat depth.
 * The old color-only API is retained for ABI and existing unrelated modes. */
u32 avp_predator_infrared_background_depth_color(u32 original,double distance);
void avp_predator_infrared_background_composite_depth_rgb16(
    u32 *scene,const u32 *mask,const float *wall_depth,
    unsigned width,unsigned height,int draw_distance_mode);
void avp_predator_background_mark_scaled_overlay(u32 *mask,unsigned stride,
                                   unsigned width,unsigned height,
                                   const u32 *before,const u32 *after);
int avp_predator_vision_style(int thermal_enabled,int electromagnetic_enabled,
                              int species,unsigned cloak_state,u16 vmode,u16 xfade);
/* R164: optional third enhancement on the ORIGINAL RGB16 Infrared pair.
 * The existing two-toggle style function stays compatible with R162 tests. */
int avp_infrared_is_mode(int enabled,int species,unsigned cloak_state,
                         u16 vmode,u16 xfade);
int avp_predator_vision_style_ex(int thermal_enabled,int electromagnetic_enabled,
                                int infrared_enabled,int species,unsigned cloak_state,
                                u16 vmode,u16 xfade);
u32 avp_infrared_palette(unsigned intensity);
int avp_electromagnetic_is_mode(int enabled,int species,unsigned cloak_state,
                          u16 vmode,u16 xfade);
unsigned avp_thermal_heat_intensity(const AvpAmp *a,double distance,u32 source_rgb);
u32 avp_thermal_heat_palette(unsigned intensity);
void avp_thermal_composite_vmode(const u32 *records,u32 *scene,
                                  unsigned width,unsigned height,
                                  unsigned mode,unsigned style);
int avp_thermal_is_mode(int enabled,int species,unsigned cloak_state,
                        u16 vmode,u16 xfade);
int avp_thermal_is_living(const AvpAmp *a);
unsigned avp_thermal_intensity(const AvpAmp *a,double distance,u32 source_rgb);
u32 avp_thermal_palette(unsigned intensity);
/* Each nonzero record packs (thermal intensity + 1) into the high byte and
 * the original 24-bit sprite color below it. A later opaque scenery/nonliving
 * sprite sets its record to zero. No full-screen tint is ever used. */
u32 avp_thermal_record(unsigned intensity,u32 original_color);
void avp_thermal_remove_covered(u32 *records,const u32 *scene,size_t count);
void avp_thermal_composite(const u32 *records,u32 *scene,size_t count);
/* Preserve the enhanced false-color palette after native Predator RGB16
 * line-buffer resampling, while retaining the original 320-word mapping. */
void avp_thermal_composite_rgb16(const u32 *records,u32 *scene,
                                 unsigned width,unsigned height);
#ifdef __cplusplus
}
#endif
#endif
