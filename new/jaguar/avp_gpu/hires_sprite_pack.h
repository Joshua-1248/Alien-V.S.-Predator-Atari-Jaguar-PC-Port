#ifndef AVP_JAGUAR_GPU_HIRES_SPRITE_PACK_H
#define AVP_JAGUAR_GPU_HIRES_SPRITE_PACK_H
#include "jaguar/avp_gpu/bridge.h"
/* Presentation-only. No ROM records, simulation animation or palette tables
 * are rewritten. Pointers are owned by the PC frontend and shared by views. */
#define AVP_HIRES_AAF_SHEETS 63u
#define AVP_HIRES_AAF_FRAMES 256u
#define AVP_HIRES_FILES_FIRST 133u
#define AVP_HIRES_FILES_COUNT 52u
#define AVP_HIRES_HUD_FIRST 185u
#define AVP_HIRES_HUD_COUNT 3u
typedef struct AvpGpuExternalSprites {
    AvpGpuRgbImage *props[AVP_HIRES_AAF_SHEETS][AVP_HIRES_AAF_FRAMES];
    AvpGpuRgbImage *enemies[AVP_HIRES_AAF_SHEETS][AVP_HIRES_AAF_FRAMES];
    AvpGpuRgbImage *weapons[AVP_HIRES_FILES_COUNT];
    AvpGpuRgbImage *hud[AVP_HIRES_HUD_COUNT];
    unsigned prop_count,enemy_count,weapon_count,hud_count;
} AvpGpuExternalSprites;

/* R329: First-person PNGs are sampled into the original FILES.DAT display
 * rectangle, which already fixes weapon sway/position and target footprint.
 * The authored PNG canvas may have arbitrary dimensions. Requiring an exact
 * integer multiple incorrectly discarded valid 1247x1261/1254x1254 art.
 * Keep AAF props/enemies on their independent strict rectangle policy. */
static inline int avp_gpu_hires_fp_image_usable(const AvpGpuRgbImage *im,
                                               int native_width,int native_height)
{
    return im && im->rgb && im->alpha &&
           native_width>0 && native_height>0 &&
           im->width>0 && im->height>0;
}
#endif
