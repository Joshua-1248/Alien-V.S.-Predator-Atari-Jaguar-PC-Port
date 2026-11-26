/* R330: GPU output -> transfer-destination host buffer -> exact CPU compare.
 * Opt-in Vulkan diagnostic ONLY; not the game's hardware backend.
 * No driver, emulator or engine source incorporated. */
#include "pc/pc_vulkan_readback.h"
#include "pc/pc_vulkan_readback_policy.h"
#include "pc/pc_vulkan_wall_projection.h"
#include <limits.h>
#include <string.h>

_Static_assert(VK_FORMAT_B8G8R8A8_UNORM == AVP_VK_READBACK_BGRA8_UNORM, "BGRA enum mismatch");
_Static_assert(VK_FORMAT_R8G8B8A8_UNORM == AVP_VK_READBACK_RGBA8_UNORM, "RGBA enum mismatch");

void avp_vk_readback_destroy(VkDevice device,AvpVkReadback *r)
{
    if (!r) return;
    if (device) {
        if (r->buffer) vkDestroyBuffer(device,r->buffer,NULL);
        if (r->memory) vkFreeMemory(device,r->memory,NULL);
    }
    memset(r,0,sizeof(*r));
}

int avp_vk_readback_create(VkPhysicalDevice gpu,VkDevice device,
                           uint32_t width,uint32_t height,VkFormat format,
                           AvpVkReadback *out)
{
    VkPhysicalDeviceMemoryProperties mem;
    VkBufferCreateInfo desc={0};
    VkMemoryRequirements req={0};
    VkMemoryAllocateInfo alloc={0};
    AvpVkReadback local={0};
    size_t bytes=0u;
    uint32_t type=UINT32_MAX;
    if (!gpu || !device || !out || out->buffer || out->memory ||
        !avp_vk_readback_spec(width,height,format,&bytes)) return 0;
    desc.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    desc.size=(VkDeviceSize)bytes;
    desc.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    desc.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device,&desc,NULL,&local.buffer)!=VK_SUCCESS) goto fail;
    vkGetBufferMemoryRequirements(device,local.buffer,&req);
    vkGetPhysicalDeviceMemoryProperties(gpu,&mem);
    if (mem.memoryTypeCount>32u || req.size<(VkDeviceSize)bytes) goto fail;
    for(uint32_t i=0u;i<mem.memoryTypeCount;i++) {
        if ((req.memoryTypeBits&(1u<<i)) &&
            (mem.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            type=i;break;
        }
    }
    if (type==UINT32_MAX) goto fail;
    alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize=req.size;
    alloc.memoryTypeIndex=type;
    if (vkAllocateMemory(device,&alloc,NULL,&local.memory)!=VK_SUCCESS) goto fail;
    if (vkBindBufferMemory(device,local.buffer,local.memory,0u)!=VK_SUCCESS) goto fail;
    local.width=width;local.height=height;local.format=format;local.bytes=(VkDeviceSize)bytes;
    *out=local;
    return 1;
fail:
    avp_vk_readback_destroy(device,&local);
    return 0;
}

/* Call AFTER ending the COLOR_ATTACHMENT render pass, which transitions the
 * swapchain image to TRANSFER_SRC_OPTIMAL. A later barrier returns it to
 * PRESENT_SRC_KHR. The GPU writes exactly width*height*4 packed bytes. */
void avp_vk_readback_record(VkCommandBuffer cmd,VkImage image,
                            const AvpVkReadback *rb)
{
    VkBufferImageCopy region={0};
    VkImageMemoryBarrier barrier={0};
    if (!cmd || !image || !rb || !rb->buffer) return;
    region.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount=1u;
    region.imageExtent.width=rb->width;
    region.imageExtent.height=rb->height;
    region.imageExtent.depth=1u;
    vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           rb->buffer,1u,&region);
    barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=image;
    barrier.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount=1u;
    barrier.subresourceRange.layerCount=1u;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0u,
                         0u,NULL,0u,NULL,1u,&barrier);
}

/* Caller MUST wait for the GPU submission fence before calling this.
 * HOST_COHERENT memory requires no invalidate; noncoherent is excluded. */
int avp_vk_readback_verify(VkDevice device,const AvpVkReadback *rb,
                           uint32_t *mismatches)
{
    void *mapped=NULL;
    int ok=0;
    if (!device || !rb || !rb->memory || !mismatches) return 0;
    if (vkMapMemory(device,rb->memory,0u,rb->bytes,0u,&mapped)!=VK_SUCCESS)
        return 0;
    ok=avp_vk_readback_check(mapped,(size_t)rb->bytes,rb->width,
                             rb->height,(uint32_t)rb->format,mismatches);
    vkUnmapMemory(device,rb->memory);
    return ok;
}

/* Read exactly the same Vulkan readback buffer with a geometry-dependent CPU
 * reference. Must be called only after the GPU completion fence signals. */
int avp_vk_readback_verify_ray_columns(VkDevice device,const AvpVkReadback *rb,
                                       uint32_t *mismatches)
{
    void *mapped=NULL;
    int ok=0;
    if (!device || !rb || !rb->memory || !mismatches)return 0;
    if (vkMapMemory(device,rb->memory,0u,rb->bytes,0u,&mapped)!=VK_SUCCESS)
        return 0;
    ok=avp_vk_readback_check_ray_columns(mapped,(size_t)rb->bytes,
        rb->width,rb->height,(uint32_t)rb->format,mismatches);
    vkUnmapMemory(device,rb->memory);
    return ok;
}

/* R336: GPU readback of the independently constructed wall projection shader.
 * No completed CPU game framebuffer is uploaded to Vulkan. */
int avp_vk_readback_verify_projected_wall(VkDevice device,
   const AvpVkReadback *rb,uint32_t *mismatches)
{
    void *mapped=NULL;
    int ok=0;
    if(!device || !rb || !rb->memory || !mismatches)return 0;
    if(vkMapMemory(device,rb->memory,0u,rb->bytes,0u,&mapped)!=VK_SUCCESS)
        return 0;
    ok=avp_vk_wall_projection_check_readback(mapped,(size_t)rb->bytes,
         rb->width,rb->height,(uint32_t)rb->format,mismatches);
    vkUnmapMemory(device,rb->memory);
    return ok;
}

/* R337: verify a genuine GPU two-source, two-draw image via host-coherent
 * readback. The separately constructed CPU oracle detects missing passes. */
int avp_vk_readback_verify_two_slot_wall(VkDevice device,
   const AvpVkReadback *rb,uint32_t *mismatches)
{
    void *mapped=NULL;
    int ok=0;
    if(!device || !rb || !rb->memory || !mismatches)return 0;
    if(vkMapMemory(device,rb->memory,0u,rb->bytes,0u,&mapped)!=VK_SUCCESS)
        return 0;
    ok=avp_vk_wall_projection_check_two_slots(mapped,(size_t)rb->bytes,
         rb->width,rb->height,(uint32_t)rb->format,mismatches);
    vkUnmapMemory(device,rb->memory);
    return ok;
}

