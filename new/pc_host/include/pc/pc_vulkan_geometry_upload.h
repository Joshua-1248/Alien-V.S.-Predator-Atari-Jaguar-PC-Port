#ifndef AVP_PC_VULKAN_GEOMETRY_UPLOAD_H
#define AVP_PC_VULKAN_GEOMETRY_UPLOAD_H
/* R332: upload R331's ray geometry stream into a GPU-owned STORAGE_BUFFER.
 * Optional Vulkan SDK diagnostic only. Does NOT activate Hardware gameplay. */
#include <vulkan/vulkan.h>
#include <stddef.h>
#include <stdint.h>

typedef struct AvpVkUploadedGeometry {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize bytes;
} AvpVkUploadedGeometry;

/* This function validates the complete R331 wire stream before any GPU call.
 * A successful return means a transfer and fence wait succeeded, NOT that a
 * shader has used the buffer or that pixels match Software. */
int avp_vk_geometry_upload(VkPhysicalDevice gpu, VkDevice device, VkQueue queue,
    uint32_t family, const uint8_t *wire, size_t bytes, AvpVkUploadedGeometry *out);
void avp_vk_geometry_dispose(VkDevice device, AvpVkUploadedGeometry *out);
#endif
