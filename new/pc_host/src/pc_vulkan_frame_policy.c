/* R322: portable Vulkan WSI status policy; no Vulkan implementation imported. */
#include "pc/pc_vulkan_frame_policy.h"
AvpWsiResultAction avp_wsi_result_action(int32_t result)
{
    if (result == 0) return AVP_WSI_RESULT_OK;             /* VK_SUCCESS */
    if (result == 1000001003) return AVP_WSI_RESULT_SUBOPTIMAL; /* VK_SUBOPTIMAL_KHR */
    if (result == -1000001004) return AVP_WSI_RESULT_OUT_OF_DATE; /* VK_ERROR_OUT_OF_DATE_KHR */
    return AVP_WSI_RESULT_FATAL; /* VK_ERROR_DEVICE_LOST etc. must not be retried blindly */
}
int avp_wsi_surface_paused(uint32_t width,uint32_t height)
{
    return width==0u || height==0u;
}
int avp_wsi_hybrid_rebuild_needed(uint32_t old_width,uint32_t old_height,
    uint32_t new_width,uint32_t new_height,int pending)
{
    if(avp_wsi_surface_paused(new_width,new_height))return 0; /* wait for restore */
    return !!pending || old_width!=new_width || old_height!=new_height;
}
int avp_wsi_retry_allowed(uint32_t consecutive_rebuilds)
{
    return consecutive_rebuilds <= 8u;
}
int avp_wsi_window_extent_changed(uint32_t width, uint32_t height,
                                  uint32_t actual_width, uint32_t actual_height)
{
    return actual_width != 0u && actual_height != 0u &&
           (width != actual_width || height != actual_height);
}
