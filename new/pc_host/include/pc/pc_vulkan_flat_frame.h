#ifndef AVP_PC_VULKAN_FLAT_FRAME_H
#define AVP_PC_VULKAN_FLAT_FRAME_H
/* RE47/R356: SDK-neutral original flat source capture and composition oracle.
 * No composed Software framebuffer enters the Vulkan upload path. */
#include "pc/pc_vulkan_flat_projection.h"
#include "pc/pc_vulkan_wall_slots.h"
#include "pc/pc_vulkan_wall_pixel_oracle.h"
#include "pc/pc_vulkan_wall_oracle_capture.h"
#include "jaguar/avp_gpu/world_texture_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpVkFlatFrame {
    AvpVkFlatProjection geometry;
    AvpVkWallSlotPlan slots; /* Reuse validated per-image staging contract. */
    const AvpGpuRgbImage *images[AVP_WORLD_SCENE_IMAGE_SLOTS]; /* borrowed */
    AvpVkWallPixelSource pixels[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t *owned_words[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t *cry_lut; /* 65536 native Jaguar CRY-to-RGB words */
} AvpVkFlatFrame;

/* Fail closed on non-opaque images, invalid coordinates, unsupported
 * filtering, or incomplete source metadata. The caller owns scene/state. */
int avp_vk_flat_frame_build(const AvpWorldScenePacket *scene,
    const struct AvpAvpGpuState *state,AvpVkFlatFrame *out);
void avp_vk_flat_frame_dispose(AvpVkFlatFrame *frame);
/* Make a complete black-background CPU oracle from the same SOURCE TEXELS
 * that the Vulkan fragment shader reads. No Software pixels are copied. */
int avp_vk_flat_frame_oracle_make(const AvpVkFlatFrame *frame,
    uint32_t format,uint8_t *dst,size_t bytes);
/* Draw order: original floors and ceilings, then opaque wall columns.
 * Requires a separate wall source-backed capture and byte-exact image. */
int avp_vk_flat_frame_compare_composite(const AvpVkFlatFrame *frame,
    const AvpVkWallOracleCapture *walls,uint32_t format,
    const uint8_t *gpu,size_t gpu_bytes,uint32_t *mismatches);
#ifdef __cplusplus
}
#endif
#endif
