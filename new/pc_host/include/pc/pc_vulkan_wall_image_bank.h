#ifndef AVP_PC_VULKAN_WALL_IMAGE_BANK_H
#define AVP_PC_VULKAN_WALL_IMAGE_BANK_H
/* RE47 R339: transactional per-slot opaque wall image ownership.
 * SDK independent. Texture bytes are never copied here. An acquire callback
 * may allocate and synchronously upload a real Vulkan image (R325). Only a
 * fully successful set becomes visible to the caller. No Software renderer
 * dependencies or mutable global state. */
#include <stddef.h>
#include <stdint.h>
#include "pc/pc_vulkan_wall_source_upload.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef int (*AvpVkWallImageAcquireFn)(void *user,uint32_t slot,
    const uint32_t *words,uint32_t width,uint32_t height,
    uint32_t encoding,size_t bytes,void **resource);
typedef void (*AvpVkWallImageReleaseFn)(void *user,uint32_t slot,
    void *resource);
typedef struct AvpVkWallImageEntry {
    void *resource; /* owned backend object; never a staging pointer */
    uint32_t width,height,encoding;
} AvpVkWallImageEntry;
typedef struct AvpVkWallImageBank {
    AvpVkWallImageEntry slots[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t order[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t count;
    uint64_t uploaded_bytes;
} AvpVkWallImageBank;

/* Must be called only after all GPU references to these resources finish.
 * Destroy in reverse creation order, clearing all ownership information. */
void avp_vk_wall_image_bank_dispose(AvpVkWallImageBank *bank,
    AvpVkWallImageReleaseFn release,void *user);

/* `out` MUST be zero-initialized, and is never overwritten when live.
 * All used slots are preflighted via R338 BEFORE the first callback.
 * Acquire returns a unique non-NULL resource on success, one which owns
 * its upload data independently. On failure it may return a non-NULL
 * partial resource, which is rolled back by `release` as well.
 * A bad/duplicate resource is rejected, with no double-destruction.
 * The candidate bank is fully destroyed on ANY failure. Out remains empty.
 * An empty plan produces an empty bank without invoking either callback.
 * Caller must not publish GPU descriptors before this returns success. */
int avp_vk_wall_image_bank_build(const AvpVkWallSlotPlan *plan,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    size_t max_texture_bytes,uint64_t max_total_bytes,
    AvpWorldGpuStaging *scratch,
    AvpVkWallImageAcquireFn acquire,AvpVkWallImageReleaseFn release,
    void *user,AvpVkWallImageBank *out);
#ifdef __cplusplus
}
#endif
#endif
