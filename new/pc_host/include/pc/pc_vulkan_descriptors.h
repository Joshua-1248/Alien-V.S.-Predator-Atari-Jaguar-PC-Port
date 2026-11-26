#ifndef AVP_PC_VULKAN_DESCRIPTORS_H
#define AVP_PC_VULKAN_DESCRIPTORS_H
/* R326 independent Vulkan image views and fragment shader descriptor pair.
 * Must build against official Khronos Vulkan headers when enabled. No
 * compiled shaders or gameplay Vulkan pipeline are provided by this module. */
#include "pc/pc_vulkan_upload.h"
#include "pc/pc_vulkan_geometry_upload.h"

typedef struct AvpVkSampledPair {
    VkImageView source_view;
    VkImageView lut_view;
    VkSampler sampler;
    VkDescriptorSetLayout layout;
    VkDescriptorPool pool;
    VkDescriptorSet set; /* Owned by pool; not freed separately. */
} AvpVkSampledPair;

/* Set 0 / binding 0: R32_UINT source; binding 1: R32_UINT 256x256 lookup.
 * Both are combined image samplers, texelFetch uses integer coordinates.
 * Caller must outlive these descriptors with BOTH source images and must
 * finish all GPU uses before disposing or updating them. */
int avp_vk_sampled_pair_create(VkDevice device,
    const AvpVkUploadedR32 *source, const AvpVkUploadedR32 *lut,
    AvpVkSampledPair *out);
/* R333: retain binding 0/1 for source and LUT, add a GPU-owned ray-stream
 * storage buffer at binding 2. Buffer and images must outlive this set. */
int avp_vk_sampled_scene_create(VkDevice device,
    const AvpVkUploadedR32 *source, const AvpVkUploadedR32 *lut,
    const AvpVkUploadedGeometry *geometry, AvpVkSampledPair *out);
/* R364: persistent geometry may change its used range while retaining the
 * same VkBuffer. Update binding 2 after the previous frame fence completes. */
int avp_vk_sampled_scene_update_geometry(VkDevice device,
    const AvpVkUploadedGeometry *geometry,AvpVkSampledPair *pair);
void avp_vk_sampled_pair_dispose(VkDevice device, AvpVkSampledPair *pair);
#endif
