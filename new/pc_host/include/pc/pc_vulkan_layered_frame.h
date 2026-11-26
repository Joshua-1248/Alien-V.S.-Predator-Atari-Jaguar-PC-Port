#ifndef AVP_PC_VULKAN_LAYERED_FRAME_H
#define AVP_PC_VULKAN_LAYERED_FRAME_H
/* RE47/R358: source-texel Vulkan preparation for original layered walls,
 * doors, windows and floor/ceiling. GPU receives source coordinates, NOT a
 * Software RGB frame. Binary source alpha is resolved in original layer order;
 * partial alpha and unsupported filtering FAIL CLOSED. Hardware still OFF. */
#include "pc/pc_vulkan_wall_layers.h"
#include "pc/pc_vulkan_flat_projection.h"
#include "pc/pc_vulkan_wall_slots.h"
#include "pc/pc_vulkan_wall_pixel_oracle.h"
#include "jaguar/avp_gpu/world_texture_transfer.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Inclusive minimum, exclusive maximum in native source-pixel coordinates. */
typedef struct AvpVkSourceBounds {
    uint32_t x0,y0,x1,y1;
} AvpVkSourceBounds;

typedef struct AvpVkLayeredFrame {
    AvpVkFlatProjection geometry; /* final AVFL v1 16-byte/pixel SOURCE refs */
    AvpVkFlatProjection flats; /* original source-only flat refs for oracle */
    AvpVkWallLayers layers; /* original back-to-front wall/portal spans */
    AvpVkWallSlotPlan slots;
    const AvpGpuRgbImage *images[AVP_WORLD_SCENE_IMAGE_SLOTS];
    AvpGpuRgbImage upload_images[AVP_WORLD_SCENE_IMAGE_SLOTS];
    AvpVkWallPixelSource pixels[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t *owned_words[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t *cry_lut;
    uint32_t masked_pixels,wall_coverage_pixels;
    /* R375: set only after the original independent layer validator succeeds.
     * The game treats a validated frame as immutable until its GPU submit. */
    unsigned validated_build;
    AvpVkSourceBounds visible_bounds[AVP_WORLD_SCENE_IMAGE_SLOTS];

} AvpVkLayeredFrame;

/* Empty initial output required. Only original source texture bytes and exact
 * geometry/row metadata are retained. Caller still owns scene and state.
 * upload_images have source RGB/CRY data but alpha NULL: this is safe ONLY
 * for this pre-resolved coverage tape, never for ordinary geometry draws. */
int avp_vk_layered_frame_build(const AvpWorldScenePacket *scene,
    const struct AvpAvpGpuState *state,AvpVkLayeredFrame *out);
void avp_vk_layered_frame_dispose(AvpVkLayeredFrame *frame);
/* Verify the final tape against original layer spans and source alpha. */
int avp_vk_layered_frame_validate(const AvpVkLayeredFrame *frame);
/* Fast structural check for a builder-validated, immutable same-frame packet;
 * non-built packets must still pass the complete independent validator. */
int avp_vk_layered_frame_ready_for_present(const AvpVkLayeredFrame *frame);
/* Conservative per-slot VkScissor analogue in swapchain pixel coordinates.
 * Must cover every pixel that would pass the original per-slot shader test.
 * For uncertified packets, return the entire region to remain fail-closed. */
int avp_vk_layered_frame_slot_scissor(const AvpVkLayeredFrame *frame,
    uint32_t slot,uint32_t region_x,uint32_t region_y,
    uint32_t region_width,uint32_t region_height,
    uint32_t *x,uint32_t *y,uint32_t *width,uint32_t *height);
/* Exactly compare source colors from the previously layer-validated resolved
 * GPU tape against Software. No additional wall-depth replay is necessary:
 * the builder has already independently checked every resolved record. */
int avp_vk_layered_frame_validated_tape_matches_world(const AvpVkLayeredFrame *frame,
    const uint32_t *software_world,uint32_t pitch,uint32_t width,uint32_t height,
    uint32_t *mismatches);
/* Independent expected RGBA/BGRA computed directly from the original flat
 * tape plus back-to-front layers, NOT from the final GPU tape. */
int avp_vk_layered_frame_oracle_make(const AvpVkLayeredFrame *frame,
    uint32_t format,uint8_t *dst,size_t bytes);
/* Full independent source oracle vs original Software world, fused into
 * one traversal. Does NOT compare against the final GPU geometry tape;
 * reads the original layer spans, flat refs and packed source texels. */
int avp_vk_layered_frame_oracle_matches_world(const AvpVkLayeredFrame *frame,
    const uint32_t *software_world,uint32_t pitch,uint32_t width,uint32_t height,
    uint32_t *mismatches);
int avp_vk_layered_frame_compare(const AvpVkLayeredFrame *frame,
    uint32_t format,const uint8_t *gpu,size_t bytes,uint32_t *mismatches);
#ifdef __cplusplus
}
#endif
#endif
