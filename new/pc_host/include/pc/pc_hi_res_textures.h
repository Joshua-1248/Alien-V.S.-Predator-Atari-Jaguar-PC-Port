#ifndef AVP_PC_HI_RES_TEXTURES_H
#define AVP_PC_HI_RES_TEXTURES_H

#include "jaguar/avp_gpu/bridge.h"
#include <stddef.h>
#include <stdint.h>

/* External, PC-only presentation art. Index == original ROM FILES panel index.
 * No original ROM texture or game logic is changed; missing slots fall back
 * individually to the original (or enabled prototype-restoration) panel. */
#define AVP_PC_HI_RES_SLOTS 74u
typedef struct AvpPcHiResTextures {
    AvpGpuRgbImage image[AVP_PC_HI_RES_SLOTS];
    /* Display-only composed signs, original retail source indices 16/56..73. */
    AvpGpuRgbImage notice[18];
    unsigned notices_composed;
    unsigned applied;
} AvpPcHiResTextures;

/* Shared strict 8-bit RGB/RGBA PNG decoder for external sprite packs.
 * Caller owns rgb/alpha; no CRY image generated unless requested separately. */
int avp_pc_hi_res_read_png(const char *path,AvpGpuRgbImage *out);
void avp_pc_hi_res_textures_destroy(AvpPcHiResTextures *pack);
/* Rebuild only signs affected by supplied PNG components. The retail
 * component dimensions determine the exact Jaguar layout and offsets. */
unsigned avp_pc_hi_res_textures_build_notices(AvpPcHiResTextures *pack,
    const AvpGpuRgbImage original[AVP_PC_HI_RES_SLOTS],
    const uint8_t *qconv,size_t qconv_size);
/* Returns accepted panels. Search is strictly executable-relative:
 * /path/to/avp_pc -> /path/to/hi_res_textures/NN.png.
 * A invalid/missing texture never alters any original texture. */
unsigned avp_pc_hi_res_textures_load(AvpPcHiResTextures *pack,
    const char *executable_path,const uint8_t *qconv,size_t qconv_size);

#endif
