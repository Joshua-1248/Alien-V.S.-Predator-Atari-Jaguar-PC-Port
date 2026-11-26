#include "pc/pc_vulkan_descriptor_policy.h"
int avp_vk_descriptor_pair_dimensions(uint32_t width, uint32_t height,
                                     uint32_t lut_width, uint32_t lut_height)
{
    return width > 0u && height > 0u && width <= 16384u && height <= 16384u &&
           lut_width == 256u && lut_height == 256u;
}
