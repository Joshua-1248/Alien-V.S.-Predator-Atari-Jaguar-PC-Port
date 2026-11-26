#ifndef AVP_PC_HYBRID_FRAME_GATE_H
#define AVP_PC_HYBRID_FRAME_GATE_H
/* R372: inexpensive, SDK-independent eligibility policy. Avoid building a
 * costly Hybrid source/oracle frame when presentation is known to be blocked.
 * The original Software renderer is always the fallback. */
#ifdef __cplusplus
extern "C" {
#endif
typedef enum AvpHybridFrameBlock {
    AVP_HYBRID_FRAME_READY=0,
    AVP_HYBRID_FRAME_NOT_REQUESTED,
    AVP_HYBRID_FRAME_WSI_UNAVAILABLE,
    AVP_HYBRID_FRAME_MENU_OPEN,
    AVP_HYBRID_FRAME_FRONTEND,
    AVP_HYBRID_FRAME_COMPUTER,
    AVP_HYBRID_FRAME_DEBUG_OVERLAY,
    AVP_HYBRID_FRAME_VR_PREVIEW,
    AVP_HYBRID_FRAME_CROSSHAIR,
    AVP_HYBRID_FRAME_VIGNETTE,
    AVP_HYBRID_FRAME_VIEWPORT,
    AVP_HYBRID_FRAME_SHADERS_MISSING,
    AVP_HYBRID_FRAME_SOURCE_UNAVAILABLE,
    AVP_HYBRID_FRAME_FOREGROUND_UNAVAILABLE,
    AVP_HYBRID_FRAME_WORLD_MISMATCH,
    AVP_HYBRID_FRAME_EMPTY_SOURCE
} AvpHybridFrameBlock;
typedef struct AvpHybridFrameConditions {
    int requested, wsi_ready;
    int menu_open, frontend, computer, show_fps, debug_overlay;
    int vr_preview, crosshair_overlay, vignette;
    unsigned viewport_width,viewport_height,window_width,window_height;
} AvpHybridFrameConditions;
AvpHybridFrameBlock avp_hybrid_frame_gate(const AvpHybridFrameConditions *conditions);
const char *avp_hybrid_frame_block_detail(AvpHybridFrameBlock reason);
#ifdef __cplusplus
}
#endif
#endif
