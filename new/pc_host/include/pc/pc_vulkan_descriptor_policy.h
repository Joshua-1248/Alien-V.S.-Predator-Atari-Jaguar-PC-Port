#ifndef AVP_PC_VULKAN_DESCRIPTOR_POLICY_H
#define AVP_PC_VULKAN_DESCRIPTOR_POLICY_H
/* R326: SDK-independent input contract. Vulkan sampler descriptors read
 * one integer source image and one 256x256 CRY16->RGB lookup image. */
#include <stdint.h>
int avp_vk_descriptor_pair_dimensions(uint32_t width, uint32_t height,
                                     uint32_t lut_width, uint32_t lut_height);
#endif
