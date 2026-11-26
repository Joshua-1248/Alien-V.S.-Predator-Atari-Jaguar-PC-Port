#ifndef AVP_PC_VULKAN_READBACK_POLICY_H
#define AVP_PC_VULKAN_READBACK_POLICY_H
#include <stdint.h>
#include <stddef.h>
/* Vulkan 1.0 VkFormat enum values, fixed by Khronos. The optional Vulkan
 * runtime uses _Static_assert against the installed Vulkan headers. */
#define AVP_VK_READBACK_RGBA8_UNORM 37u
#define AVP_VK_READBACK_BGRA8_UNORM 44u
int avp_vk_readback_spec(uint32_t width,uint32_t height,uint32_t format,size_t *bytes);
int avp_vk_readback_check(const uint8_t *pixels,size_t bytes,uint32_t width,
                           uint32_t height,uint32_t format,uint32_t *mismatches);
/* R335: independent CPU oracle for the synthetic multi-column shader. */
int avp_vk_readback_check_ray_columns(const uint8_t *pixels,size_t bytes,
    uint32_t width,uint32_t height,uint32_t format,uint32_t *mismatches);
#endif
