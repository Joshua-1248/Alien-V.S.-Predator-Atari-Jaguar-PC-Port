#ifndef AVP_PC_VULKAN_WSI_H
#define AVP_PC_VULKAN_WSI_H
/* R322: standalone Vulkan diagnostic; NOT the game's hardware renderer.
 * Uses Vulkan-Headers installed by the builder, not a private ABI shim.
 * This function never receives game state or copyrighted ROM assets. */
#include <stdint.h>
#include "pc_scanlines.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpVkWsiSmoke {
    int instance_created;
    int surface_created;
    int device_created;
    int swapchain_created;
    int command_submitted;
    int presented;
    uint32_t frames_submitted, frames_presented, swapchain_recreations;
    uint32_t width, height;
    uint32_t textures_uploaded, texture_bytes_uploaded;
    uint32_t geometry_bytes_uploaded; /* R332 synthetic GPU STORAGE_BUFFER test */
    char device_name[256];
    char detail[256];
} AvpVkWsiSmoke;

/* On Linux/X11 a valid Display* and mapped Window are required.
 * Creates GPU-owned swapchain images and clears ONE image with GPU commands.
 * Destroys all Vulkan objects before returning. No CPU framebuffer upload.
 * Returns 1 only when vkQueuePresentKHR accepts the frame; this does NOT
 * certify visual parity, game integration or high frame rates.
 * A null/empty key selects the highest-ranked eligible non-CPU device. */
int avp_vk_wsi_smoke(void *x11_display, uintptr_t x11_window,
                     uint32_t requested_width, uint32_t requested_height,
                     const char *gpu_key, AvpVkWsiSmoke *out);
/* An opt-in bounded multi-frame diagnostic. This owns Vulkan resources but
 * NEVER submits game geometry. frame_limit must be 1..4096. */
int avp_vk_wsi_loop(void *x11_display, uintptr_t x11_window,
                    uint32_t requested_width, uint32_t requested_height,
                    const char *gpu_key, uint32_t frame_limit, AvpVkWsiSmoke *out);
/* R327: OPTIONAL test-only shader sampling and presentation. It requires
 * compiled SPIR-V, real Vulkan SDK and graphics/present capable GPU.
 * This is a synthetic fullscreen triangle, NOT game wall rendering. */
int avp_vk_wsi_shader_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out);
/* R335: opt-in full-width synthetic geometry->GPU shader->readback gate.
 * Requires official Vulkan SDK and compatible GPU. NOT gameplay drawing. */
/* R336 original-projection *solid wall* shader diagnostic, not gameplay. */
int avp_vk_wsi_projected_wall_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out);
/* RE47 R337 optional two texture / two descriptor / two draw diagnostic. */
int avp_vk_wsi_two_slot_wall_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out);
/* R348: mixed original CRY + 720-line replacement, v3 exact rows. */
int avp_vk_wsi_mixed_cry_rgb_wall_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out);
/* R350: opt-in *source-backed* live opaque-wall diagnostic. A caller may
 * supply the game's completed ray packet and current original/hi-res texture
 * state; no synthetic scene, CPU-composed frame or ROM redistribution.
 * Requires an X11 Vulkan device and SPIR-V; fails closed on any unsupported
 * door, portal, sprite, transparent source or partial geometry. Never
 * enables the game's Hardware option and does not implement other passes.
 * Caller must retain the scene and textures through the synchronous call. */
struct AvpWorldScenePacket;
struct AvpAvpGpuState;
int avp_vk_wsi_live_wall_frame(void *x11_display, uintptr_t x11_window,
    const struct AvpWorldScenePacket *scene,
    const struct AvpAvpGpuState *state,const char *gpu_key,
    const char *vertex_spv,const char *fragment_spv,AvpVkWsiSmoke *out);

/* R353 optional game-owned, one-frame live-wall probe. Transfers ownership
 * ONLY of a private X11 diagnostic Display and Window. The game's own Xlib
 * Display and Window must NEVER be passed here. The caller's scene/state
 * remain borrowed for this synchronous operation. XCloseDisplay precedes
 * vkDestroyInstance so Vulkan's registered Xlib hooks remain valid. */
/* R356: private-window one-frame original floor/ceiling + wall GPU pass.
 * Requires both SPIR-V fragments. Fails closed on unsupported features.
 * Normal Software gameplay and Hardware menu remain unchanged. */
/* R358: optional original door/window/portal visibility + floor/ceiling
 * source-texel GPU diagnostic (one frame, Software remains authoritative). */
int avp_vk_wsi_run_owned_xlib_live_layered(void *x11_display,
    uintptr_t x11_window,const struct AvpWorldScenePacket *scene,
    const struct AvpAvpGpuState *state,const char *gpu_key,
    const char *vertex_spv,const char *layered_spv,AvpVkWsiSmoke *out);

int avp_vk_wsi_run_owned_xlib_live_wall_flat(void *x11_display,
    uintptr_t x11_window,const struct AvpWorldScenePacket *scene,
    const struct AvpAvpGpuState *state,const char *gpu_key,
    const char *vertex_spv,const char *wall_spv,const char *flat_spv,
    AvpVkWsiSmoke *out);

int avp_vk_wsi_run_owned_xlib_live_wall(void *x11_display, uintptr_t x11_window,
    const struct AvpWorldScenePacket *scene,
    const struct AvpAvpGpuState *state, const char *gpu_key,
    const char *vertex_spv, const char *fragment_spv, AvpVkWsiSmoke *out);

/* R359: same-window, persistent Vulkan gameplay WSI foundation. Retains
 * Vulkan instance/device/graphics swapchain across game frames and resizes.
 * Borrows the existing game's X11 Display and Window: NEVER closes them.
 * Does NOT draw or present a completed Software framebuffer, and does not
 * claim active Hardware rendering until GPU world+overlay composition exists.
 * IMPORTANT: call suspend BEFORE XCloseDisplay, then destroy AFTER it,
 * to avoid NVIDIA/Xlib close-hook use-after-unload (R352). */
typedef struct AvpVkWsiGameplayContext AvpVkWsiGameplayContext;
AvpVkWsiGameplayContext *avp_vk_wsi_gameplay_create(
    void *x11_display, uintptr_t x11_window, uint32_t width, uint32_t height,
    const char *gpu_key,int vsync_enabled, AvpVkWsiSmoke *out);
int avp_vk_wsi_gameplay_reconfigure(AvpVkWsiGameplayContext *ctx,
    void *x11_display,uintptr_t x11_window,uint32_t width,uint32_t height,
    const char *gpu_key,int vsync_enabled,AvpVkWsiSmoke *out);
/* Nonblocking check BEFORE preparing 32+ MiB of source geometry. A busy GPU
 * frame must not force the host game thread to wait for its completion. */
int avp_vk_wsi_gameplay_frame_busy(const AvpVkWsiGameplayContext *ctx);
int avp_vk_wsi_gameplay_ready(const AvpVkWsiGameplayContext *ctx);
/* R369: recoverable swapchain result. Caller shows Software for the skipped
 * frame and preserves Vulkan so resize() can rebuild on the next frame. */
int avp_vk_wsi_gameplay_rebuild_pending(const AvpVkWsiGameplayContext *ctx);
/* R370: a zero-sized X11 viewport pauses Hybrid presentation without
 * destroying the persistent device; restoring either the old or a new size
 * rebuilds before attempting another GPU frame. */
int avp_vk_wsi_gameplay_resize(AvpVkWsiGameplayContext *ctx,
    uint32_t width,uint32_t height,AvpVkWsiSmoke *out);
void avp_vk_wsi_gameplay_suspend(AvpVkWsiGameplayContext *ctx);
void avp_vk_wsi_gameplay_destroy(AvpVkWsiGameplayContext *ctx);
/* R361 experimental real game-window WORLD(source textures) + foreground
 * (only actors/weapons/HUD changed since the world pass) GPU submission.
 * Does not upload the completed Software world. Must only run after a strict
 * independent source-world/Software world parity gate; unsupported frames
 * stay fully Software. Failure invalidates the Vulkan context until reset. */
struct AvpVkLayeredFrame;
struct AvpHybridOverlay;
int avp_vk_wsi_gameplay_present_hybrid(AvpVkWsiGameplayContext *ctx,
    const struct AvpVkLayeredFrame *layered,
    const struct AvpHybridOverlay *overlay,
    uint32_t image_width,uint32_t image_height,
    const char *vertex_spv,const char *world_spv,const char *foreground_spv,
    const AvpPcScanlines *scanlines,AvpVkWsiSmoke *out);


/* Standalone executable only. Takes ownership of the Xlib Display/Window,
 * including on failure, and closes them AFTER the surface but BEFORE the
 * VkInstance. Never pass a display borrowed from the running game. */
typedef enum AvpVkWsiDiagnosticMode {
    AVP_VK_WSI_DIAG_CLEAR = 0,
    AVP_VK_WSI_DIAG_SHADER = 1,
    AVP_VK_WSI_DIAG_RAY_COLUMNS = 2,
    AVP_VK_WSI_DIAG_PROJECTED_WALL = 3,
    AVP_VK_WSI_DIAG_TWO_SLOT = 4,
    AVP_VK_WSI_DIAG_MIXED_CRY_RGB = 5,
    AVP_VK_WSI_DIAG_LOOP = 6
} AvpVkWsiDiagnosticMode;
int avp_vk_wsi_run_owned_xlib_diagnostic(void *x11_display, uintptr_t x11_window,
    uint32_t requested_width, uint32_t requested_height, const char *gpu_key,
    AvpVkWsiDiagnosticMode mode, uint32_t frame_limit,
    const char *vertex_spv, const char *fragment_spv, AvpVkWsiSmoke *out);

int avp_vk_wsi_ray_columns_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out);
#ifdef __cplusplus
}
#endif
#endif
