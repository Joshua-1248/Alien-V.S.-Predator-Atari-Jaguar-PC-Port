#ifndef AVP_PC_VULKAN_UPLOAD_H
#define AVP_PC_VULKAN_UPLOAD_H
/* RE46 R325. Genuine Vulkan R32_UINT transfer, diagnostic only.
 * This module requires the official Vulkan SDK if built; it never replaces
 * the Jaguar software renderer, and it does not bundle driver or SDK code. */
#include <stdint.h>
#include <stddef.h>
#include <vulkan/vulkan.h>

typedef struct AvpVkUploadedR32 {
    VkImage image;
    VkDeviceMemory memory;
    uint32_t width, height;
} AvpVkUploadedR32;

/* Synchronous upload. Validates that VK_FORMAT_R32_UINT is sampleable,
 * creates host-visible coherent transfer staging, copies via a GPU command,
 * transitions to SHADER_READ_ONLY_OPTIMAL and waits for GPU completion.
 * The result owns an image and device memory until explicitly disposed.
 * Source bytes are copied before this returns; no pointer is retained.
 * Output must be zero-initialized or disposed before a new upload;
 * an already-live output is rejected to prevent leaking its GPU allocation.
 * A null/failed output owns no resources. Caller supplies coherent,
 * little-endian R32_UINT packed words from world_texture_transfer.c. */
int avp_vk_upload_r32uint(VkPhysicalDevice gpu, VkDevice device, VkQueue queue,
                          uint32_t family, const uint32_t *words,
                          uint32_t width, uint32_t height, size_t bytes,
                          AvpVkUploadedR32 *out);
void avp_vk_uploaded_r32_dispose(VkDevice device, AvpVkUploadedR32 *uploaded);

/* R378: deferred GPU texture transfer for the game path only. Unlike the
 * diagnostic uploader, prepare never submits a queue or waits on a fence.
 * Caller owns the staging buffer until the game-frame fence completes.
 * The uploaded image may be transferred to the persistent source bank; after
 * transfer, set upload.image to zero, retaining upload.target for recording.
 * Record must be called inside the SAME graphics command buffer as the first
 * sampled draw, BEFORE vkCmdBeginRenderPass. Only HOST_COHERENT staging is
 * used; the original packed texels remain byte-identical. */
typedef struct AvpVkDeferredR32Upload {
    VkBuffer staging;
    VkDeviceMemory staging_memory;
    AvpVkUploadedR32 image;
    VkImage target;
    uint32_t width, height;
} AvpVkDeferredR32Upload;
int avp_vk_r32_deferred_prepare(VkPhysicalDevice gpu,VkDevice device,
    const uint32_t *words,uint32_t width,uint32_t height,size_t bytes,
    AvpVkDeferredR32Upload *upload);
int avp_vk_r32_deferred_record(VkCommandBuffer command,
    const AvpVkDeferredR32Upload *upload);
void avp_vk_r32_deferred_dispose(VkDevice device,AvpVkDeferredR32Upload *upload);

#endif
