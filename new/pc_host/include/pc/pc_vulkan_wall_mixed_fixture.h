#ifndef AVP_PC_VULKAN_WALL_MIXED_FIXTURE_H
#define AVP_PC_VULKAN_WALL_MIXED_FIXTURE_H
/* R348: independent diagnostic scene, not the game or its assets.
 * Exercises original Jaguar CRY, RGB hi-res, Jaguar distance lighting,
 * and exact CPU high-resolution row mapping through one GPU draw batch. */
#include "pc/pc_vulkan_wall_projection.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_VK_MIXED_CRY_SLOT 5u
#define AVP_VK_MIXED_RGB_SLOT 6u
#define AVP_VK_MIXED_CRY_HEIGHT 128u
#define AVP_VK_MIXED_RGB_HEIGHT 720u
#define AVP_VK_MIXED_TEX_WIDTH 16u
typedef struct AvpVkWallMixedFixture {
    AvpVkWallProjectionStream geometry;
    uint8_t *rgb;
    uint16_t *cry;
    uint32_t *cry_lut;
    AvpGpuRgbImage owned[2];
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t mapped_columns, mapped_rows;
} AvpVkWallMixedFixture;
/* Transactional: on failure leaves out empty. 640x480 intentionally
 * produces v3 exact row maps; no mock screenshot/pixels are uploaded. */
int avp_vk_wall_mixed_fixture_build(uint32_t width,uint32_t height,
                                   AvpVkWallMixedFixture *out);
void avp_vk_wall_mixed_fixture_dispose(AvpVkWallMixedFixture *fixture);
#ifdef __cplusplus
}
#endif
#endif
