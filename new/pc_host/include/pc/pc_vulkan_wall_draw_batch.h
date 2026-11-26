#ifndef AVP_PC_VULKAN_WALL_DRAW_BATCH_H
#define AVP_PC_VULKAN_WALL_DRAW_BATCH_H
/* RE47 R340. Frame-validated source-image -> GPU wall draw mapping.
 * Platform-neutral: normal Software game builds never submit this data.
 * A draw contains one source slot bound to an immutable descriptor set.
 * The GPU shader discards columns belonging to other source slots. */
#include <stddef.h>
#include <stdint.h>
#include "pc/pc_vulkan_wall_image_bank.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpVkWallDraw {
    uint32_t slot,columns,width,height,encoding;
} AvpVkWallDraw;
typedef struct AvpVkWallDrawBatch {
    AvpVkWallDraw draws[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t count,active_columns,width,height;
} AvpVkWallDrawBatch;
/* Recompute the slot plan from this validated frame, then prove every
 * referenced texture is present and dimension/encoding-compatible with the
 * committed bank. Reject incomplete, stale or aliased banks. Do NOT draw
 * partially populated frames. Failure clears the output batch. */
int avp_vk_wall_draw_batch_build(const uint8_t *wire,size_t bytes,
    const AvpVkWallImageBank *bank,AvpVkWallDrawBatch *out);
#ifdef __cplusplus
}
#endif
#endif
