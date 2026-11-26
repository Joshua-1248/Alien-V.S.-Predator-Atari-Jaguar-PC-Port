#ifndef AVP_PC_VULKAN_FLAT_PROJECTION_H
#define AVP_PC_VULKAN_FLAT_PROJECTION_H
/* RE47/R355: floor/ceiling source-texel projection for a future Vulkan pass.
 * This is NOT a composed Software framebuffer. Every 16-byte record names
 * an original/hi-res source image, exact nearest texel and Jaguar CRY shade.
 * No Vulkan dependency; no gameplay renderer enabled by this module. */
#include <stddef.h>
#include <stdint.h>
#include "jaguar/avp_gpu/world_scene_packet.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_VK_FLAT_MAGIC 0x4c465641u /* 'AVFL', little-endian */
#define AVP_VK_FLAT_VERSION 1u
#define AVP_VK_FLAT_HEADER_BYTES 32u
#define AVP_VK_FLAT_PIXEL_BYTES 16u
#define AVP_VK_FLAT_NO_IMAGE UINT32_MAX
#define AVP_VK_FLAT_MAX_BYTES (64u*1024u*1024u)
typedef struct AvpVkFlatProjection {
    uint8_t *data;
    size_t bytes;
    uint32_t pixels,source_pixels,source_slots;
} AvpVkFlatProjection;
typedef struct AvpVkFlatTexel {
    uint32_t slot,source_x,source_y;
    int32_t cry_shade;
} AvpVkFlatTexel;
/* Only nearest point sampling is certified. Invalid map, unsupported
 * geometry filtering, malformed source bounds or overlarge output fails
 * closed and leaves the output empty. The input packet remains borrowed. */
int avp_vk_flat_projection_build(const AvpWorldScenePacket *scene,
                                  AvpVkFlatProjection *out);
int avp_vk_flat_projection_validate(const uint8_t *wire,size_t bytes,
                                     const AvpWorldScenePacket *scene);
int avp_vk_flat_projection_get(const uint8_t *wire,size_t bytes,
                                uint32_t x,uint32_t y,AvpVkFlatTexel *out);
void avp_vk_flat_projection_dispose(AvpVkFlatProjection *flat);
#ifdef __cplusplus
}
#endif
#endif
