/* R332: experimental GPU-owned storage-buffer transfer for source ray hits.
 * Builds only when optional Vulkan SDK diagnostic is enabled.
 * No proprietary engine, emulator or driver implementation is copied. */
#include "pc/pc_vulkan_geometry_upload.h"
#include "pc/pc_vulkan_geometry_policy.h"
#include "pc/pc_vulkan_wall_projection.h"
#include <string.h>

static int memory_type(const VkPhysicalDeviceMemoryProperties *props,
    uint32_t allowed,VkMemoryPropertyFlags required,uint32_t *index)
{
    uint32_t flags[32]={0};
    if (!props || props->memoryTypeCount>32u) return 0;
    for(uint32_t i=0;i<props->memoryTypeCount;i++)
        flags[i]=props->memoryTypes[i].propertyFlags;
    return avp_vk_geometry_choose_memory(allowed,(uint32_t)required,
        flags,props->memoryTypeCount,index);
}
void avp_vk_geometry_dispose(VkDevice device,AvpVkUploadedGeometry *out)
{
    if(!out)return;
    /* Calling code MUST ensure GPU work has completed before destruction. */
    if(device){
        if(out->buffer)vkDestroyBuffer(device,out->buffer,NULL);
        if(out->memory)vkFreeMemory(device,out->memory,NULL);
    }
    memset(out,0,sizeof(*out));
}
int avp_vk_geometry_upload(VkPhysicalDevice gpu,VkDevice device,VkQueue queue,
    uint32_t family,const uint8_t *wire,size_t bytes,AvpVkUploadedGeometry *out)
{
    VkPhysicalDeviceMemoryProperties mem;
    VkBuffer staging=VK_NULL_HANDLE,geometry=VK_NULL_HANDLE;
    VkDeviceMemory staging_mem=VK_NULL_HANDLE,geometry_mem=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkCommandBuffer cmd=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    VkMemoryRequirements req;
    VkBufferCreateInfo info={0};
    VkMemoryAllocateInfo alloc={0};
    VkCommandPoolCreateInfo pool_info={0};
    VkCommandBufferAllocateInfo command_info={0};
    VkCommandBufferBeginInfo begin={0};
    VkBufferCopy copy={0};
    VkBufferMemoryBarrier barrier={0};
    VkSubmitInfo submit={0};
    VkFenceCreateInfo fence_info={0};
    uint32_t type;
    void *mapped=NULL;
    int submitted=0,complete=0,ok=0;
    if (!out || out->buffer || out->memory || out->bytes) return 0;
    memset(out,0,sizeof(*out));
    if (!gpu || !device || !queue || !(avp_vk_geometry_wire_valid(wire,bytes) ||
      avp_vk_wall_projection_validate(wire,bytes)))return 0;
    vkGetPhysicalDeviceMemoryProperties(gpu,&mem);

    info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size=(VkDeviceSize)bytes;
    info.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device,&info,NULL,&staging)!=VK_SUCCESS)goto done;
    vkGetBufferMemoryRequirements(device,staging,&req);
    if(req.size<bytes || !memory_type(&mem,req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&type))goto done;
    alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize=req.size;
    alloc.memoryTypeIndex=type;
    if(vkAllocateMemory(device,&alloc,NULL,&staging_mem)!=VK_SUCCESS)goto done;
    if(vkBindBufferMemory(device,staging,staging_mem,0u)!=VK_SUCCESS)goto done;
    if(vkMapMemory(device,staging_mem,0u,(VkDeviceSize)bytes,0u,&mapped)!=VK_SUCCESS)goto done;
    memcpy(mapped,wire,bytes);
    vkUnmapMemory(device,staging_mem);mapped=NULL;

    info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if(vkCreateBuffer(device,&info,NULL,&geometry)!=VK_SUCCESS)goto done;
    vkGetBufferMemoryRequirements(device,geometry,&req);
    if(req.size<bytes)goto done;
    if(!memory_type(&mem,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&type) &&
       !memory_type(&mem,req.memoryTypeBits,0u,&type))goto done;
    alloc.allocationSize=req.size;
    alloc.memoryTypeIndex=type;
    if(vkAllocateMemory(device,&alloc,NULL,&geometry_mem)!=VK_SUCCESS)goto done;
    if(vkBindBufferMemory(device,geometry,geometry_mem,0u)!=VK_SUCCESS)goto done;
    pool_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex=family;
    if(vkCreateCommandPool(device,&pool_info,NULL,&pool)!=VK_SUCCESS)goto done;
    command_info.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_info.commandPool=pool;
    command_info.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_info.commandBufferCount=1u;
    if(vkAllocateCommandBuffers(device,&command_info,&cmd)!=VK_SUCCESS)goto done;
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if(vkBeginCommandBuffer(cmd,&begin)!=VK_SUCCESS)goto done;
    copy.size=(VkDeviceSize)bytes;
    vkCmdCopyBuffer(cmd,staging,geometry,1u,&copy);
    barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer=geometry;
    barrier.offset=0u;
    barrier.size=(VkDeviceSize)bytes;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0u,0u,NULL,1u,&barrier,0u,NULL);
    if(vkEndCommandBuffer(cmd)!=VK_SUCCESS)goto done;
    fence_info.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if(vkCreateFence(device,&fence_info,NULL,&fence)!=VK_SUCCESS)goto done;
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount=1u;submit.pCommandBuffers=&cmd;
    if(vkQueueSubmit(queue,1u,&submit,fence)!=VK_SUCCESS)goto done;
    submitted=1;
    if(vkWaitForFences(device,1u,&fence,VK_TRUE,UINT64_MAX)!=VK_SUCCESS)goto done;
    complete=1;
    out->buffer=geometry;out->memory=geometry_mem;out->bytes=(VkDeviceSize)bytes;
    geometry=VK_NULL_HANDLE;geometry_mem=VK_NULL_HANDLE;
    ok=1;
done:
    if(submitted&&!complete)(void)vkDeviceWaitIdle(device);
    if(mapped)vkUnmapMemory(device,staging_mem);
    if(fence)vkDestroyFence(device,fence,NULL);
    if(pool)vkDestroyCommandPool(device,pool,NULL);
    if(geometry)vkDestroyBuffer(device,geometry,NULL);
    if(geometry_mem)vkFreeMemory(device,geometry_mem,NULL);
    if(staging)vkDestroyBuffer(device,staging,NULL);
    if(staging_mem)vkFreeMemory(device,staging_mem,NULL);
    return ok;
}
