#ifndef AVP_PC_VULKAN_FRAME_POLICY_H
#define AVP_PC_VULKAN_FRAME_POLICY_H
/* R322: API-independent WSI result classification and safety policy.
 * Numerical values are from the public Vulkan 1.0/KHR_swapchain registry;
 * this header does not pretend to replace Khronos Vulkan-Headers. */
#include <stdint.h>
typedef enum AvpWsiResultAction {
    AVP_WSI_RESULT_FATAL = -1,
    AVP_WSI_RESULT_OK = 0,
    AVP_WSI_RESULT_SUBOPTIMAL = 1,
    AVP_WSI_RESULT_OUT_OF_DATE = 2
} AvpWsiResultAction;
AvpWsiResultAction avp_wsi_result_action(int32_t result);
/* R370: zero-size X11 configure events mean a temporarily unavailable
 * surface, not a destroyed Vulkan device. Defer swapchain rebuild until
 * both dimensions become nonzero (possibly the original dimensions). */
int avp_wsi_surface_paused(uint32_t width,uint32_t height);
int avp_wsi_retry_allowed(uint32_t consecutive_rebuilds);
int avp_wsi_window_extent_changed(uint32_t width, uint32_t height,
                                  uint32_t actual_width, uint32_t actual_height);
/* R369: a WSI OUT_OF_DATE/SUBOPTIMAL result requires recreation even when
 * X11 reports exactly the same size (surface format/mode can change). */
int avp_wsi_hybrid_rebuild_needed(uint32_t old_width,uint32_t old_height,
    uint32_t new_width,uint32_t new_height,int pending);
#endif
