#ifndef AVP_PC_VULKAN_HYBRID_STREAM_H
#define AVP_PC_VULKAN_HYBRID_STREAM_H
/* R364 optional Vulkan-SDK-only reusable gameplay world/foreground buffers.
 * World data is ORIGINAL AVFL source references, not Software framebuffer.
 * A completed frame fence is mandatory before updating either buffer. */
#include "pc/pc_vulkan_geometry_upload.h"
#include "pc/pc_vulkan_upload.h"
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stddef.h>

typedef struct AvpVkHybridStream {
    AvpVkUploadedGeometry geometry;
    VkDeviceSize geometry_capacity;
    void *geometry_mapped;
    size_t geometry_bytes_written; /* R367: 0 for exact-repeat frames */
    AvpVkUploadedR32 foreground;
    VkBuffer foreground_staging;
    VkDeviceMemory foreground_staging_memory;
    void *foreground_mapped;
    size_t foreground_bytes;
    int foreground_shader_layout; /* 0 = undefined, 1 = shader read */
    int foreground_dirty; /* R367: 1 until current exact pixels are recorded for upload */
} AvpVkHybridStream;

/* Returns 1 on successfully updating live frame content. A resize/format
 * change installs a new allocation; if creation fails, all old allocations
 * are preserved until caller suspends the context. */
int avp_vk_hybrid_stream_geometry(VkPhysicalDevice gpu,VkDevice device,
    const uint8_t *data,size_t bytes,AvpVkHybridStream *stream,
    int *recreated);
int avp_vk_hybrid_stream_foreground(VkPhysicalDevice gpu,VkDevice device,
    const uint32_t *pixels,uint32_t width,uint32_t height,
    AvpVkHybridStream *stream,int *recreated);
/* Must be called inside the same frame command buffer, BEFORE render pass.
 * Transfer and shader-read visibility happen inside that one submission. */
void avp_vk_hybrid_stream_record_foreground(VkCommandBuffer command,
    AvpVkHybridStream *stream);
void avp_vk_hybrid_stream_dispose(VkDevice device,AvpVkHybridStream *stream);
#endif
