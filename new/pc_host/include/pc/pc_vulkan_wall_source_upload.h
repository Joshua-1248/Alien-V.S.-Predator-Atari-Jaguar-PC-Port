#ifndef AVP_PC_VULKAN_WALL_SOURCE_UPLOAD_H
#define AVP_PC_VULKAN_WALL_SOURCE_UPLOAD_H
/* RE47 R338: bridge actual source images to R337's validated wall slots.
 * No Vulkan dependency, ROM content, or GPU framebuffer pixels are stored.
 * A caller-supplied synchronous upload callback may forward packed R32_UINT
 * words to the existing Vulkan image uploader. The callback MUST copy/upload
 * before returning, and MUST NOT retain `words` after it returns.
 *
 * This contract prepares only opaque source textures for the conservative
 * projected-solid-wall path. Alpha holes/translucency, portals, floors,
 * ceilings, sprites and the gameplay renderer are still unsupported.
 */
#include <stddef.h>
#include <stdint.h>
#include "pc/pc_vulkan_wall_slots.h"
#include "jaguar/avp_gpu/world_texture_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef int (*AvpVkWallTextureUploadFn)(void *user, uint32_t source_slot,
    const uint32_t *words, uint32_t width, uint32_t height,
    uint32_t encoding, size_t bytes);

typedef struct AvpVkWallUploadStats {
    uint32_t slots_uploaded;
    uint64_t source_bytes_uploaded;
} AvpVkWallUploadStats;

/* Full preflight of every referenced source BEFORE calling `upload` once.
 * The source pointer array must have AVP_WORLD_SCENE_IMAGE_SLOTS entries.
 * Only slots referenced by `plan` are examined or uploaded. Reuses a single
 * caller-owned staging allocation, never a 92-texture CPU atlas. A failed
 * call zeros stats and invalidates staging even after partial upload; the
 * caller MUST destroy/unpublish any temporary GPU resources it created on
 * callback failure. It must commit GPU resources only after return==1.
 * `max_texture_bytes` and `max_total_bytes` are nonzero hard upper limits.
 * A valid empty plan succeeds without upload calls or allocations.
 */
int avp_vk_wall_source_upload(const AvpVkWallSlotPlan *plan,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    size_t max_texture_bytes, uint64_t max_total_bytes,
    AvpWorldGpuStaging *scratch, AvpVkWallTextureUploadFn upload,
    void *user, AvpVkWallUploadStats *stats);
#ifdef __cplusplus
}
#endif
#endif
