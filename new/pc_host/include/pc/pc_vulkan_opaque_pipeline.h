#ifndef AVP_PC_VULKAN_OPAQUE_PIPELINE_H
#define AVP_PC_VULKAN_OPAQUE_PIPELINE_H
/* R327 experimental GPU opaque-sampling graphics pipeline; not game renderer. */
#include "pc/pc_vulkan_descriptors.h"
#include "pc_scanlines.h"
#include <stddef.h>
#include <stdint.h>

typedef struct AvpVkOpaquePipeline {
    VkShaderModule vertex;
    VkShaderModule fragment;
    VkPipelineLayout layout;
    VkPipeline pipeline;
} AvpVkOpaquePipeline;

typedef struct AvpVkSamplingPush {
    uint32_t encoding; /* 1: CRY16 + 8-bit alpha; 2: RGB888 + 8-bit alpha */
    uint32_t wx,wy; /* 0..1024, 10-bit interpolation weights */
    int32_t shade;
} AvpVkSamplingPush;
/* R361: exact integer nearest-neighbor image mapping, in absolute swapchain
 * pixel coordinates. No floating-point UV boundary errors on odd window sizes. */
typedef struct AvpVkHybridPush {
    uint32_t encoding,slot;
    uint32_t source_width,source_height;
    uint32_t region_x,region_y,region_width,region_height;
    AvpPcScanlines scanlines;
} AvpVkHybridPush;


/* Render-pass uses one color attachment in the current swapchain format.
 * Source and CRY LUT descriptor pair must stay alive until GPU completion.
 * Caller supplies SPIR-V from glslangValidator; never arbitrary GLSL text.
 * No pipeline cache and no geometry buffers are necessary for diagnostic. */
int avp_vk_opaque_pipeline_create(VkDevice device,
    const AvpVkSampledPair *pair, VkRenderPass pass, VkExtent2D extent,
    const uint32_t *vertex_words, size_t vertex_bytes,
    const uint32_t *fragment_words, size_t fragment_bytes,
    AvpVkOpaquePipeline *out);
/* R361: world/foreground draw into the centered existing-game image rect.
 * The VkViewport+scissor are both restricted to region, so letterbox pixels
 * retain the render pass black clear and the UVs map to the source raster. */
int avp_vk_opaque_pipeline_create_region(VkDevice device,
    const AvpVkSampledPair *pair,VkRenderPass pass,VkExtent2D surface,
    VkRect2D region,uint32_t push_constant_bytes,int dynamic_scissor,
    const uint32_t *vertex_words,size_t vertex_bytes,
    const uint32_t *fragment_words,size_t fragment_bytes,
    AvpVkOpaquePipeline *out);
void avp_vk_opaque_pipeline_dispose(VkDevice device, AvpVkOpaquePipeline *pipe);
#endif
