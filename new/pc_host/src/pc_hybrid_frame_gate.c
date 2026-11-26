#include "pc/pc_hybrid_frame_gate.h"
AvpHybridFrameBlock avp_hybrid_frame_gate(const AvpHybridFrameConditions *c)
{
    if(!c)return AVP_HYBRID_FRAME_VIEWPORT;
    if(!c->requested)return AVP_HYBRID_FRAME_NOT_REQUESTED;
    if(!c->wsi_ready)return AVP_HYBRID_FRAME_WSI_UNAVAILABLE;
    if(c->menu_open)return AVP_HYBRID_FRAME_MENU_OPEN;
    if(c->frontend)return AVP_HYBRID_FRAME_FRONTEND;
    if(c->computer)return AVP_HYBRID_FRAME_COMPUTER;
    if(c->debug_overlay)return AVP_HYBRID_FRAME_DEBUG_OVERLAY;
    if(c->vr_preview)return AVP_HYBRID_FRAME_VR_PREVIEW;
    /* Host crosshair is composited above Vulkan via a shaped X11 child. */
    if(c->vignette)return AVP_HYBRID_FRAME_VIGNETTE;
    if(!c->viewport_width||!c->viewport_height||!c->window_width||
       !c->window_height||c->viewport_width>c->window_width||
       c->viewport_height>c->window_height)
        return AVP_HYBRID_FRAME_VIEWPORT;
    return AVP_HYBRID_FRAME_READY;
}
const char *avp_hybrid_frame_block_detail(AvpHybridFrameBlock reason)
{
    switch(reason){
    case AVP_HYBRID_FRAME_READY:return "eligible";
    case AVP_HYBRID_FRAME_NOT_REQUESTED:return "Software renderer selected";
    case AVP_HYBRID_FRAME_WSI_UNAVAILABLE:return "Vulkan game-window WSI unavailable";
    case AVP_HYBRID_FRAME_MENU_OPEN:return "F12 menu open";
    case AVP_HYBRID_FRAME_FRONTEND:return "frontend screen";
    case AVP_HYBRID_FRAME_COMPUTER:return "computer/terminal active";
    case AVP_HYBRID_FRAME_DEBUG_OVERLAY:return "debug/status overlay enabled";
    case AVP_HYBRID_FRAME_VR_PREVIEW:return "VR preview enabled";
    case AVP_HYBRID_FRAME_CROSSHAIR:return "PC crosshair overlay enabled";
    case AVP_HYBRID_FRAME_VIGNETTE:return "vignette postprocessing enabled";
    case AVP_HYBRID_FRAME_VIEWPORT:return "viewport/window dimensions unsupported";
    case AVP_HYBRID_FRAME_SHADERS_MISSING:return "missing/incomplete Hybrid SPIR-V shaders/ folder beside Vulkan executable";
    case AVP_HYBRID_FRAME_SOURCE_UNAVAILABLE:return "source-world capture unavailable";
    case AVP_HYBRID_FRAME_FOREGROUND_UNAVAILABLE:return "foreground extraction unavailable";
    case AVP_HYBRID_FRAME_WORLD_MISMATCH:return "source-world pixel oracle rejected frame";
    case AVP_HYBRID_FRAME_EMPTY_SOURCE:return "no supported source texture slots";
    default:return "unknown Hybrid restriction";
    }
}
