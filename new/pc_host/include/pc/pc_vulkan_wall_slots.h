#ifndef AVP_PC_VULKAN_WALL_SLOTS_H
#define AVP_PC_VULKAN_WALL_SLOTS_H
/* RE47 R337. SDK-independent per-source-slot pass planning for R336's
 * projected opaque-wall stream. Texture memory is NOT copied or uploaded.
 * Each active slot can later be bound for a separate GPU draw; all draws
 * use the same original source-derived wall projection buffer. */
#include <stddef.h>
#include <stdint.h>
#include "pc/pc_vulkan_wall_projection.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpVkWallSlot {
    uint32_t used, columns, width, height, encoding;
} AvpVkWallSlot;
typedef struct AvpVkWallSlotPlan {
    uint32_t used_count, active_columns;
    uint32_t order[AVP_WORLD_SCENE_IMAGE_SLOTS]; /* ascending slot ID */
    AvpVkWallSlot slots[AVP_WORLD_SCENE_IMAGE_SLOTS];
} AvpVkWallSlotPlan;
/* Fail closed if the projected stream is incomplete/corrupt or if columns
 * disagree on image dimensions/encoding for any shared image slot. Never
 * retain stale slot counts on failure. Caller uploads game source pixels
 * with avp_world_scene_pack_image only after this plan succeeds. */
int avp_vk_wall_slots_collect(const uint8_t *wire,size_t bytes,
                              AvpVkWallSlotPlan *plan);
#ifdef __cplusplus
}
#endif
#endif
