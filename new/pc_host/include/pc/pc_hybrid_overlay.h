#ifndef AVP_PC_HYBRID_OVERLAY_H
#define AVP_PC_HYBRID_OVERLAY_H
/* RE47/R360 — binary-opacity CPU foreground over a genuinely GPU-rendered
 * source world.  This is a transition contract, not a Software framebuffer
 * presentation path. The original Software raster can record its wall/flat
 * image once BEFORE sprites, weapons and HUD; the diff against the completed
 * image then describes only pixels which would need a CPU overlay.
 *
 * An XRGB final pixel equal to the world pixel needs no overlay. A different
 * final pixel is uploaded with alpha 255. This is correct even when the
 * original CPU used partial alpha blending: the already-blended final pixel
 * is opaque at that location. Large global post-processing effects cause the
 * overlay to fail the coverage gate rather than claiming useful GPU drawing.
 * No Vulkan types or APIs are used in this portable module. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_HYBRID_OVERLAY_MAX_PIXELS (4096u*4096u)
typedef struct AvpHybridOverlay {
    uint32_t *world;      /* exact Software world, BEFORE actors and weapons */
    uint32_t *foreground; /* 0xAARRGGBB: 00 = GPU world, FF = Software overlay */
    uint32_t width,height;
    uint32_t changed, transparent;
    uint32_t min_x,min_y,max_x,max_y; /* exclusive bbox, empty when changed=0 */
    int valid;
} AvpHybridOverlay;
/* Transactional resize: failure leaves the previous buffers valid but resets
 * the eligibility bit so a stale overlay can NEVER be rendered. */
int avp_hybrid_overlay_ensure(AvpHybridOverlay *frame,uint32_t w,uint32_t h);
void avp_hybrid_overlay_invalidate(AvpHybridOverlay *frame);
void avp_hybrid_overlay_destroy(AvpHybridOverlay *frame);
/* Caller MUST have filled `frame->world` at the original world/actor boundary.
 * Final receives the same logical dimensions, stride in pixels. The cap is
 * specified as an integer percentage 0..100 (not floating point), inclusive.
 * A rejected frame has valid=0; Software presentation must be used instead. */
int avp_hybrid_overlay_finish(AvpHybridOverlay *frame,
    const uint32_t *final,uint32_t stride,uint32_t cap_percent);
/* Reference-only test: compose exactly the captured CPU world with its opaque
 * foreground. The world is replaced by GPU sampling in the actual hybrid path.
 * Explicitly fails if the frame is invalid or output capacity is insufficient. */
int avp_hybrid_overlay_reconstruct(const AvpHybridOverlay *frame,
    uint32_t *dst,size_t dst_pixels);
#ifdef __cplusplus
}
#endif
#endif
