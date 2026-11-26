/* Independently authored Vulkan WSI selection helpers.
 * No SDK headers are required; passed enumeration values come from Vulkan. */
#include "pc/pc_vulkan_wsi_policy.h"
#include <limits.h>
#include <string.h>
unsigned avp_wsi_device_rank(uint32_t type)
{
    if (type == 2u) return 30u; /* DISCRETE */
    if (type == 1u) return 20u; /* INTEGRATED */
    if (type == 3u) return 10u; /* VIRTUAL */
    return 0u; /* CPU or OTHER is not Hardware */
}
uint32_t avp_wsi_image_count(uint32_t lo, uint32_t hi)
{
    uint32_t n = lo > 2u ? lo : 2u;
    if (hi != 0u && n > hi) n = hi;
    return n < lo ? 0u : n;
}
uint32_t avp_wsi_composite_alpha(uint32_t bits)
{
    if (bits & 1u) return 1u; /* OPAQUE preferred */
    return bits & (0u - bits); /* else lowest supported composite flag */
}
uint32_t avp_wsi_present_mode(int vsync_enabled,uint32_t mask)
{
    if(vsync_enabled || !(mask&((1u<<0)|(1u<<1))))return 2u; /* FIFO */
    if(mask&(1u<<0))return 0u; /* IMMEDIATE */
    return 1u; /* MAILBOX */
}
static uint32_t clamp(uint32_t n, uint32_t lo, uint32_t hi)
{
    return n < lo ? lo : (n > hi ? hi : n);
}
int avp_wsi_extent(AvpWsiExtent requested, AvpWsiExtent current,
                   AvpWsiExtent minimum, AvpWsiExtent maximum,
                   AvpWsiExtent *result)
{
    if (!result || !requested.width || !requested.height ||
        minimum.width > maximum.width || minimum.height > maximum.height) return 0;
    if (current.width != UINT32_MAX) *result = current;
    else {
        result->width = clamp(requested.width, minimum.width, maximum.width);
        result->height = clamp(requested.height, minimum.height, maximum.height);
    }
    return result->width != 0u && result->height != 0u;
}

/* An explicit device key may be unavailable after driver changes. Never
 * select another physical GPU under its identity; let the caller report it. */
int avp_wsi_gpu_candidate_rank(uint32_t type,int eligible,
    const char *candidate_key,const char *requested_key)
{
    unsigned rank=avp_wsi_device_rank(type);
    if(!eligible || !rank || !candidate_key || !*candidate_key)return 0;
    if(requested_key && *requested_key && strcmp(candidate_key,requested_key)!=0)
        return 0;
    return (int)rank;
}
int avp_wsi_shader_caps_sufficient(const AvpWsiShaderCaps *caps,
    uint32_t required_texture_dimension)
{
    if(!caps || !caps->r32_optimal_sampled ||
       required_texture_dimension<256u ||
       caps->max_image_dimension_2d<required_texture_dimension ||
       caps->max_storage_buffer_range<32u*1024u*1024u ||
       caps->max_push_constants_size<16u ||
       caps->max_per_stage_sampled_images<2u ||
       caps->max_set_sampled_images<2u ||
       caps->max_per_stage_samplers<2u ||
       caps->max_set_samplers<2u ||
       caps->max_per_stage_storage_buffers<1u ||
       caps->max_set_storage_buffers<1u)return 0;
    return 1;
}
