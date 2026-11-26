#ifndef AVP_PC_VULKAN_WALL_PIXEL_ORACLE_H
#define AVP_PC_VULKAN_WALL_PIXEL_ORACLE_H
/* R346: CPU pixel oracle for future real Vulkan source-image readback.
 * R32_UINT texture words are source texels, not a Software framebuffer.
 * Supports validated opaque plain walls only; no portals, sprites or HUD.
 * Does NOT enable a Vulkan gameplay renderer or certify GPU execution. */
#include <stddef.h>
#include <stdint.h>
#include "pc/pc_vulkan_wall_projection.h"
#ifdef __cplusplus
extern "C" {
#endif
/* VK_FORMAT_R8G8B8A8_UNORM = 37, B8G8R8A8_UNORM = 44. */
#define AVP_VK_WALL_PIXEL_RGBA8 37u
#define AVP_VK_WALL_PIXEL_BGRA8 44u
#define AVP_VK_WALL_PIXEL_MAX_FRAME_BYTES (64u*1024u*1024u)
typedef struct AvpVkWallPixelSource {
    const uint32_t *words;  /* row-major source R32_UINT texels */
    uint32_t width,height,encoding;
} AvpVkWallPixelSource;
/* Transactional: failure leaves 'dst' untouched. Output is black/opaque
 * outside walls, mirroring the diagnostic render-pass clear. Encodings:
 * 1=original CRY16+alpha8, 2=RGB8+alpha8; both must be fully opaque. */
int avp_vk_wall_pixel_oracle_make(const uint8_t *wire,size_t wire_bytes,
    const AvpVkWallPixelSource sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    const uint32_t *cry_lut,uint32_t format,uint8_t *dst,size_t dst_bytes);
/* Compare an actual Vulkan readback with source-derived expected pixels.
 * Fails closed on invalid inputs; mismatch count is UINT32_MAX on failure. */
int avp_vk_wall_pixel_oracle_compare(const uint8_t *wire,size_t wire_bytes,
    const AvpVkWallPixelSource sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    const uint32_t *cry_lut,uint32_t format,
    const uint8_t *gpu_pixels,size_t gpu_bytes,uint32_t *mismatches);
#ifdef __cplusplus
}
#endif
#endif
