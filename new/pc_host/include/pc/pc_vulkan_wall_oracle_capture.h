#ifndef AVP_PC_VULKAN_WALL_ORACLE_CAPTURE_H
#define AVP_PC_VULKAN_WALL_ORACLE_CAPTURE_H
/* RE47 R347: isolated source-backed CPU snapshot for verifying ACTUAL Vulkan
 * readback. Texture words and geometry are retained independently of the
 * upload scratch or the live game; never captures a Software framebuffer.
 * No Vulkan headers and no changes to active Software gameplay. */
#include <stddef.h>
#include <stdint.h>
#include "pc/pc_vulkan_wall_pixel_oracle.h"
#include "pc/pc_vulkan_wall_slots.h"
#include "jaguar/avp_gpu/world_texture_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AvpVkWallOracleCapture {
    uint8_t *wire;
    size_t wire_bytes;
    uint32_t *owned_words[AVP_WORLD_SCENE_IMAGE_SLOTS];
    AvpVkWallPixelSource sources[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t *owned_cry_lut;
    uint32_t texture_count;
    uint64_t texture_bytes;
} AvpVkWallOracleCapture;
void avp_vk_wall_oracle_capture_dispose(AvpVkWallOracleCapture *capture);
/* Transactional: 'out' must be empty (zeroed), is unchanged on any failure.
 * Snapshot packs the SAME source words uploaded by R338/R339; its lifetime
 * is independent of upload callbacks, scratch buffers and scene geometry. */
int avp_vk_wall_oracle_capture_build(const uint8_t *wire,size_t wire_bytes,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    const uint32_t *cry_lut,size_t max_texture_bytes,uint64_t max_total_bytes,
    AvpVkWallOracleCapture *out);
/* Fail closed if frame is absent or mismatched; mismatch count is UINT32_MAX
 * if source validation fails. The caller must wait on the GPU fence before
 * calling with mapped HOST_COHERENT readback bytes. */
int avp_vk_wall_oracle_capture_compare(const AvpVkWallOracleCapture *capture,
    uint32_t swapchain_format,const uint8_t *gpu_pixels,size_t gpu_bytes,
    uint32_t *mismatches);
#ifdef __cplusplus
}
#endif
#endif
