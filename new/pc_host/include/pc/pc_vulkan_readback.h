#ifndef AVP_PC_VULKAN_READBACK_H
#define AVP_PC_VULKAN_READBACK_H
/* R330 diagnostic-only Vulkan shader readback. Source/outputs are synthetic,
 * never a software game framebuffer or copyrighted asset. Official Vulkan SDK
 * is required when compiling this module. */
#include <vulkan/vulkan.h>
#include <stddef.h>
#include <stdint.h>
#include "pc/pc_vulkan_wall_oracle_capture.h"

typedef struct AvpVkReadback {
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint32_t width, height;
    VkFormat format;
    VkDeviceSize bytes;
} AvpVkReadback;

int avp_vk_readback_create(VkPhysicalDevice gpu, VkDevice device,
                           uint32_t w, uint32_t h, VkFormat format,
                           AvpVkReadback *out);
void avp_vk_readback_record(VkCommandBuffer cmd, VkImage image,
                            const AvpVkReadback *rb);
int avp_vk_readback_verify(VkDevice device, const AvpVkReadback *rb,
                           uint32_t *mismatches);
int avp_vk_readback_verify_ray_columns(VkDevice device, const AvpVkReadback *rb,
                                      uint32_t *mismatches);
int avp_vk_readback_verify_two_slot_wall(VkDevice device,
   const AvpVkReadback *rb,uint32_t *mismatches);
int avp_vk_readback_verify_projected_wall(VkDevice device,
    const AvpVkReadback *rb,uint32_t *mismatches);
/* R347: source-backed comparison of mapped Vulkan HOST_COHERENT image output.
 * Only valid AFTER GPU submission fence completion. */
int avp_vk_readback_verify_wall_capture(VkDevice device,
    const AvpVkReadback *rb,const AvpVkWallOracleCapture *capture,
    uint32_t *mismatches);
void avp_vk_readback_destroy(VkDevice device, AvpVkReadback *rb);
#endif
