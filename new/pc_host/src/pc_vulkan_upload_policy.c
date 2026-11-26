#include "pc/pc_vulkan_upload_policy.h"
#include <stdint.h>
int avp_vk_upload_dimensions(uint32_t width,uint32_t height,size_t bytes,
                             uint32_t max_device_dim)
{
    if (!width||!height||!max_device_dim||width>8192u||height>8192u||
        width>max_device_dim||height>max_device_dim) return 0;
    return bytes==(size_t)width*(size_t)height*4u;
}
int avp_vk_upload_choose_memory(uint32_t supported,uint32_t required,
                                const uint32_t *property_flags,uint32_t count,
                                uint32_t *index)
{
    if (!index||!property_flags||count>32u)return 0;
    for (uint32_t i=0;i<count;i++) {
        if ((supported&(1u<<i))&&
            (property_flags[i]&required)==required) { *index=i;return 1; }
    }
    return 0;
}
