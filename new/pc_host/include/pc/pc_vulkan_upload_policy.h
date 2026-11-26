#ifndef AVP_PC_VULKAN_UPLOAD_POLICY_H
#define AVP_PC_VULKAN_UPLOAD_POLICY_H
/* SDK-independent checks shared by Vulkan upload and deterministic tests.
 * Does not assign any Vulkan enum values or ship a private Vulkan ABI. */
#include <stdint.h>
#include <stddef.h>
int avp_vk_upload_dimensions(uint32_t width,uint32_t height,size_t bytes,
                             uint32_t max_device_dim);
int avp_vk_upload_choose_memory(uint32_t supported,uint32_t required,
                                const uint32_t *property_flags,uint32_t count,
                                uint32_t *index);
#endif
