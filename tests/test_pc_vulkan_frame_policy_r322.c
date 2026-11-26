/* R322 WSI result policy regression; no SDK, GPU, or ROM required. */
#include "pc/pc_vulkan_frame_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    assert(avp_wsi_result_action(0) == AVP_WSI_RESULT_OK);
    assert(avp_wsi_result_action(1000001003) == AVP_WSI_RESULT_SUBOPTIMAL);
    assert(avp_wsi_result_action(-1000001004) == AVP_WSI_RESULT_OUT_OF_DATE);
    assert(avp_wsi_result_action(-4) == AVP_WSI_RESULT_FATAL); /* device lost */
    assert(avp_wsi_result_action(-1000000000) == AVP_WSI_RESULT_FATAL);
    /* R370: X11 minimize is a pause, never a permanent Vulkan failure.
     * Even if the restored size matches the old one, pending forces rebuild. */
    assert(!avp_wsi_surface_paused(640u,480u));
    assert(avp_wsi_surface_paused(0u,480u));
    assert(avp_wsi_surface_paused(640u,0u));
    assert(avp_wsi_surface_paused(0u,0u));
    assert(!avp_wsi_hybrid_rebuild_needed(640u,480u,0u,0u,1));
    assert(avp_wsi_hybrid_rebuild_needed(640u,480u,640u,480u,1));
    assert(avp_wsi_hybrid_rebuild_needed(640u,480u,1920u,1080u,1));
    /* R369: same-size OUT_OF_DATE is NOT a no-op, and recoverable statuses
     * must be distinguished from a lost Vulkan device. */
    assert(!avp_wsi_hybrid_rebuild_needed(640u,480u,640u,480u,0));
    assert(avp_wsi_hybrid_rebuild_needed(640u,480u,640u,480u,1));
    assert(avp_wsi_hybrid_rebuild_needed(640u,480u,800u,480u,0));
    assert(avp_wsi_hybrid_rebuild_needed(640u,480u,640u,600u,0));
    assert(!avp_wsi_hybrid_rebuild_needed(640u,480u,0u,480u,1));
    assert(!avp_wsi_hybrid_rebuild_needed(640u,480u,640u,0u,1));
    for (uint32_t i = 0; i < 100000u; i++) {
        assert(avp_wsi_retry_allowed(i) == (i <= 8u));
        assert(!avp_wsi_surface_paused(i+1u,100u));
        assert(!avp_wsi_window_extent_changed(i+1u, 100u, i+1u, 100u));
        assert(avp_wsi_window_extent_changed(i+1u, 100u, i+2u, 100u));
        assert(avp_wsi_window_extent_changed(i+1u, 100u, i+1u, 101u));
        assert(!avp_wsi_window_extent_changed(i+1u, 100u, 0u, 100u));
        assert(!avp_wsi_window_extent_changed(i+1u, 100u, i+1u, 0u));
        if(i < 10000u){
            assert(!avp_wsi_hybrid_rebuild_needed(i+1u,100u,i+1u,100u,0));
            assert(avp_wsi_hybrid_rebuild_needed(i+1u,100u,i+1u,100u,1));
        }
        if (i > 0u) assert(avp_wsi_result_action((int32_t)i) == AVP_WSI_RESULT_FATAL);
    }
    puts("R322 Vulkan frame policy: 100000 deterministic size/retry/status cases PASS");
    return 0;
}
