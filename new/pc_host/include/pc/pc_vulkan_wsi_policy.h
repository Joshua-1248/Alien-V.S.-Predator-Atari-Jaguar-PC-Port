#ifndef AVP_PC_VULKAN_WSI_POLICY_H
#define AVP_PC_VULKAN_WSI_POLICY_H
/* R321: Vulkan WSI capability selection that can be tested without a GPU,
 * driver, ROM, X11 window or Vulkan SDK. Values use public Vulkan constants.
 * This code does not access native driver objects. */
#include <stdint.h>
typedef struct AvpWsiExtent { uint32_t width, height; } AvpWsiExtent;
/* R349: minimum actual fragment-pipeline limits, checked BEFORE selecting a
 * GPU. All fields are SDK-independent booleans or public unsigned limits.
 * R32_UINT must be usable as a sampled optimal-tiled image. The shader binds
 * two integer image samplers plus a geometry storage buffer, and uses 16
 * bytes of push constants. 32 MiB bounds the R345 exact-row wire stream. */
typedef struct AvpWsiShaderCaps {
    uint32_t r32_optimal_sampled;
    uint32_t max_image_dimension_2d;
    uint32_t max_storage_buffer_range;
    uint32_t max_push_constants_size;
    uint32_t max_per_stage_sampled_images;
    uint32_t max_set_sampled_images;
    uint32_t max_per_stage_samplers;
    uint32_t max_set_samplers;
    uint32_t max_per_stage_storage_buffers;
    uint32_t max_set_storage_buffers;
} AvpWsiShaderCaps;
/* Exact-key selection must NOT fall back to an unrelated GPU. Empty request
 * means automatic selection by rank; returns zero for invalid candidates. */
int avp_wsi_gpu_candidate_rank(uint32_t type,int eligible,
    const char *candidate_key,const char *requested_key);
int avp_wsi_shader_caps_sufficient(const AvpWsiShaderCaps *caps,
    uint32_t required_texture_dimension);
/* physical device types as specified by the public Vulkan API */
unsigned avp_wsi_device_rank(uint32_t physical_type);
uint32_t avp_wsi_image_count(uint32_t min_count, uint32_t max_count);
uint32_t avp_wsi_composite_alpha(uint32_t supported);
/* R381: SDK-independent Vulkan present mode selection. Vulkan public enum
 * numbers are IMMEDIATE=0, MAILBOX=1, FIFO=2. A gameplay V-Sync Off request
 * prefers IMMEDIATE (unthrottled) then MAILBOX; strict FIFO otherwise.
 * Unsupported modes are never selected. */
uint32_t avp_wsi_present_mode(int vsync_enabled,uint32_t supported_mask);
/* UINT32_MAX current.width denotes app-controlled extent (Vulkan WSI).
 * Returns 0 when a resize/minimize or invalid reported bounds makes a
 * presentable swapchain impossible; otherwise clamps to surface bounds. */
int avp_wsi_extent(AvpWsiExtent requested, AvpWsiExtent current,
                   AvpWsiExtent minimum, AvpWsiExtent maximum,
                   AvpWsiExtent *result);
#endif
