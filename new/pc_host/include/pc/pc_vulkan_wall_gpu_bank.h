#ifndef AVP_PC_VULKAN_WALL_GPU_BANK_H
#define AVP_PC_VULKAN_WALL_GPU_BANK_H
/* RE47 R339: optional Vulkan-SDK-only adapter. The game Software build
 * never compiles this header or its implementation. */
#include "pc/pc_vulkan_wall_image_bank.h"
#include "pc/pc_vulkan_upload.h"
typedef struct AvpVkWallGpuBank {
    AvpVkWallImageBank bank;
    VkDevice owner_device;
} AvpVkWallGpuBank;
/* Synchronous, all-or-none GPU-owned R32_UINT source textures. Each image
 * ends in SHADER_READ_ONLY_OPTIMAL (R325); caller may construct per-slot
 * sampled-scene descriptors after success. Existing live output is refused.
 * No renderer, swapchain, or gameplay backend is activated by this call. */
int avp_vk_wall_gpu_bank_build(VkPhysicalDevice gpu,VkDevice device,
    VkQueue queue,uint32_t family,const AvpVkWallSlotPlan *plan,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    size_t max_texture_bytes,uint64_t max_total_bytes,
    AvpWorldGpuStaging *scratch,AvpVkWallGpuBank *out);
/* Caller guarantees GPU completion for any later shader reads of this bank
 * before disposal. It does NOT call vkDeviceWaitIdle on the game queue. */
/* R376: update one changed visible slot; leave every other GPU image
 * resident even if its slot is temporarily outside the camera view.
 * Caller has waited on the frame fence and removed the old slot descriptor.
 * Packed pixel words are the independently captured, validated R32 values. */
int avp_vk_wall_gpu_bank_replace_slot(VkPhysicalDevice gpu,VkDevice device,
    VkQueue queue,uint32_t family,uint32_t slot,
    const uint32_t *words,uint32_t width,uint32_t height,
    uint32_t encoding,size_t bytes,AvpVkWallGpuBank *bank);
/* R378: take ownership of an allocated but NOT YET UPLOADED image. Only
 * call after the preceding game fence has completed. Caller must record its
 * deferred transfer in the current frame before shader sampling. On success
 * source is zeroed: the bank exclusively owns its GPU memory. */
int avp_vk_wall_gpu_bank_adopt_slot(VkDevice device,uint32_t slot,
    uint32_t encoding,AvpVkUploadedR32 *source,AvpVkWallGpuBank *bank);
void avp_vk_wall_gpu_bank_dispose(AvpVkWallGpuBank *bank);
const AvpVkUploadedR32 *avp_vk_wall_gpu_bank_image(
    const AvpVkWallGpuBank *bank,uint32_t slot);
#endif
