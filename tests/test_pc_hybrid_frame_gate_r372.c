#include "pc/pc_hybrid_frame_gate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    AvpHybridFrameConditions c={0};
    assert(avp_hybrid_frame_gate(NULL)==AVP_HYBRID_FRAME_VIEWPORT);
    c.requested=1;c.wsi_ready=1;
    c.viewport_width=1280;c.viewport_height=720;
    c.window_width=1920;c.window_height=1080;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_READY);
    c.show_fps=1;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_READY); /* host FPS is not a GPU blocker */
    c.show_fps=0;c.menu_open=1;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_MENU_OPEN);
    c.menu_open=0;c.crosshair_overlay=1;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_READY);
    c.crosshair_overlay=0;c.vignette=1;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_VIGNETTE);
    c.vignette=0;c.debug_overlay=1;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_DEBUG_OVERLAY);
    c.debug_overlay=0;c.frontend=1;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_FRONTEND);
    c.frontend=0;c.computer=1;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_COMPUTER);
    c.computer=0;c.vr_preview=1;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_VR_PREVIEW);
    c.vr_preview=0;c.window_width=1279;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_VIEWPORT);
    c.window_width=1920;c.wsi_ready=0;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_WSI_UNAVAILABLE);
    /* R380: Video > Renderer = Hardware suffices; no environment opt-in. */
    c.wsi_ready=1;c.requested=0;
    assert(avp_hybrid_frame_gate(&c)==AVP_HYBRID_FRAME_NOT_REQUESTED);
    assert(strstr(avp_hybrid_frame_block_detail(AVP_HYBRID_FRAME_SHADERS_MISSING),"shaders/")!=NULL);
    assert(avp_hybrid_frame_block_detail(AVP_HYBRID_FRAME_WORLD_MISMATCH)!=NULL);
    puts("R380 Hybrid gate: Hardware auto-eligible, Show FPS On permitted PASS");
    return 0;
}
