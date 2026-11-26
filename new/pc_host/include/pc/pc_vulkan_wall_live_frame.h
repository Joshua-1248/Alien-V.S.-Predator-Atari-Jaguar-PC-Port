#ifndef AVP_PC_VULKAN_WALL_LIVE_FRAME_H
#define AVP_PC_VULKAN_WALL_LIVE_FRAME_H
/* RE47 R340: SOURCE-FAITHFUL wall scene bridge. Caller obtains scene by
 * avp_gpu_trace_world_rays(), not by rendering the Software framebuffer.
 * The source textures are selected with the very same state-dependent
 * original/hi-res rules as the Software renderer. This module is fully
 * GPU-SDK-independent; the acquire callback owns actual GPU uploads. */
#include "pc/pc_vulkan_wall_draw_batch.h"
#include "jaguar/avp_gpu/state.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpVkWallLiveFrame {
    AvpVkWallProjectionStream geometry;
    AvpVkWallImageBank images;
    AvpVkWallDrawBatch draws;
} AvpVkWallLiveFrame;

/* R350: share one validated, non-uploading scene preparation route between
 * the SDK-independent live-frame bridge and the optional Vulkan WSI draw.
 * Uses the original ray packet and the same normal/hi-res image selection as
 * the Software game. Outputs are committed only when all checks pass; callers
 * must retain state/source pixels until synchronous staging/upload completes.
 * Any incomplete/unsupported wall frame fails closed, never draws a partial
 * replacement frame. Empty, zero-initialized outputs are required. */
int avp_vk_wall_live_prepare(const AvpWorldScenePacket *scene,
    const AvpAvpGpuState *state,AvpVkWallProjectionStream *geometry,
    AvpVkWallSlotPlan *slots,
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS]);

/* Opt-in only: fully validates the source-derived wall packet and sources,
 * uploads each visible image through a synchronous caller-owned backend,
 * and records original Jaguar CRY16 wall shade (v2 projection),
 * then commits a matching per-slot draw schedule. Any failed step destroys
 * provisional image objects and leaves a zero-initialized output untouched.
 * Never accepts an existing live output, even if it contains zero draws.
 * Does not capture or upload the game's completed pixel framebuffer. */
int avp_vk_wall_live_frame_build(const AvpWorldScenePacket *scene,
    const AvpAvpGpuState *state,size_t max_texture_bytes,
    uint64_t max_total_bytes,AvpWorldGpuStaging *scratch,
    AvpVkWallImageAcquireFn acquire,AvpVkWallImageReleaseFn release,
    void *user,AvpVkWallLiveFrame *out);
/* Call only after GPU reads finish. A nonempty image bank requires release.
 * Returns 0 if callback is missing, leaving live ownership intact. */
int avp_vk_wall_live_frame_dispose(AvpVkWallLiveFrame *frame,
    AvpVkWallImageReleaseFn release,void *user);
#ifdef __cplusplus
}
#endif
#endif
