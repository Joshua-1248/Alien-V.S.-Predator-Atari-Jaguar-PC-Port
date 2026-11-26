/* RE46 R325. First Vulkan-side copy of a source integer texture.
 * No original ROM or art, third-party engine code or embedded shader blobs.
 * Standalone X11 Vulkan diagnostic only: Software and game are unaffected. */
#include "pc/pc_vulkan_upload.h"
#include "pc/pc_vulkan_upload_policy.h"
#include <limits.h>
#include <string.h>

static int avp_vk_memory_type(const VkPhysicalDeviceMemoryProperties *m,
    uint32_t supported, VkMemoryPropertyFlags required, uint32_t *index)
{
    uint32_t flags[32]={0};
    if (!m || m->memoryTypeCount>32u) return 0;
    for (uint32_t i=0;i<m->memoryTypeCount;i++)
        flags[i]=m->memoryTypes[i].propertyFlags;
    return avp_vk_upload_choose_memory(supported,(uint32_t)required,
        flags,m->memoryTypeCount,index);
}

void avp_vk_uploaded_r32_dispose(VkDevice device, AvpVkUploadedR32 *uploaded)
{
    if (!uploaded) return;
    if (device) {
        if (uploaded->image) vkDestroyImage(device,uploaded->image,NULL);
        if (uploaded->memory) vkFreeMemory(device,uploaded->memory,NULL);
    }
    memset(uploaded,0,sizeof(*uploaded));
}

int avp_vk_upload_r32uint(VkPhysicalDevice gpu, VkDevice device, VkQueue queue,
                         uint32_t family, const uint32_t *words,
                         uint32_t width, uint32_t height, size_t bytes,
                         AvpVkUploadedR32 *out)
{
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties mem;
    VkFormatProperties fmt;
    VkBuffer staging=VK_NULL_HANDLE;
    VkDeviceMemory staging_memory=VK_NULL_HANDLE;
    VkImage image=VK_NULL_HANDLE;
    VkDeviceMemory image_memory=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkCommandBuffer command=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    VkMemoryRequirements req;
    VkMemoryAllocateInfo alloc={0};
    VkBufferCreateInfo buffer={0};
    VkImageCreateInfo info={0};
    VkCommandPoolCreateInfo pool_info={0};
    VkCommandBufferAllocateInfo cmd_alloc={0};
    VkCommandBufferBeginInfo begin={0};
    VkImageMemoryBarrier before={0},after;
    VkBufferImageCopy copy={0};
    VkSubmitInfo submit={0};
    VkFenceCreateInfo fence_info={0};
    uint32_t memory_index;
    VkResult result;
    void *mapped=NULL;
    int submitted=0,complete=0,ok=0;

    if (!out || out->image || out->memory) return 0;
    memset(out,0,sizeof(*out));
    if (!gpu || !device || !queue || !words) return 0;
    { const uint16_t one=1u; if (*(const uint8_t *)&one!=1u) return 0; }
    vkGetPhysicalDeviceProperties(gpu,&props);
    if (!avp_vk_upload_dimensions(width,height,bytes,props.limits.maxImageDimension2D)) return 0;
    vkGetPhysicalDeviceFormatProperties(gpu,VK_FORMAT_R32_UINT,&fmt);
    if (!(fmt.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        return 0;
    vkGetPhysicalDeviceMemoryProperties(gpu,&mem);

    buffer.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size=(VkDeviceSize)bytes;
    buffer.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device,&buffer,NULL,&staging)!=VK_SUCCESS) goto done;
    vkGetBufferMemoryRequirements(device,staging,&req);
    if (!avp_vk_memory_type(&mem,req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &memory_index)) goto done;
    alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize=req.size;
    alloc.memoryTypeIndex=memory_index;
    if (vkAllocateMemory(device,&alloc,NULL,&staging_memory)!=VK_SUCCESS) goto done;
    if (vkBindBufferMemory(device,staging,staging_memory,0u)!=VK_SUCCESS) goto done;
    if (vkMapMemory(device,staging_memory,0u,(VkDeviceSize)bytes,0u,&mapped)!=VK_SUCCESS)
        goto done;
    memcpy(mapped,words,bytes);
    vkUnmapMemory(device,staging_memory);
    mapped=NULL;

    info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType=VK_IMAGE_TYPE_2D;
    info.format=VK_FORMAT_R32_UINT;
    info.extent.width=width;
    info.extent.height=height;
    info.extent.depth=1u;
    info.mipLevels=1u;
    info.arrayLayers=1u;
    info.samples=VK_SAMPLE_COUNT_1_BIT;
    info.tiling=VK_IMAGE_TILING_OPTIMAL;
    info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device,&info,NULL,&image)!=VK_SUCCESS) goto done;
    vkGetImageMemoryRequirements(device,image,&req);
    /* Prefer DEVICE_LOCAL; a device with unified memory may use either. */
    if (!avp_vk_memory_type(&mem,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &memory_index) &&
        !avp_vk_memory_type(&mem,req.memoryTypeBits,0u,&memory_index)) goto done;
    alloc.allocationSize=req.size;
    alloc.memoryTypeIndex=memory_index;
    if (vkAllocateMemory(device,&alloc,NULL,&image_memory)!=VK_SUCCESS) goto done;
    if (vkBindImageMemory(device,image,image_memory,0u)!=VK_SUCCESS) goto done;

    pool_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex=family;
    if (vkCreateCommandPool(device,&pool_info,NULL,&pool)!=VK_SUCCESS) goto done;
    cmd_alloc.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc.commandPool=pool;
    cmd_alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc.commandBufferCount=1u;
    if (vkAllocateCommandBuffers(device,&cmd_alloc,&command)!=VK_SUCCESS) goto done;
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command,&begin)!=VK_SUCCESS) goto done;
    before.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    before.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    before.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    before.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    before.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    before.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    before.image=image;
    before.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    before.subresourceRange.levelCount=1u;
    before.subresourceRange.layerCount=1u;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,0u,0u,NULL,0u,NULL,1u,&before);
    copy.bufferOffset=0u;
    copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel=0u;
    copy.imageSubresource.baseArrayLayer=0u;
    copy.imageSubresource.layerCount=1u;
    copy.imageExtent.width=width;
    copy.imageExtent.height=height;
    copy.imageExtent.depth=1u;
    vkCmdCopyBufferToImage(command,staging,image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1u,&copy);
    after=before;
    after.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    after.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    after.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    after.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0u,0u,NULL,0u,NULL,1u,&after);
    if (vkEndCommandBuffer(command)!=VK_SUCCESS) goto done;
    fence_info.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(device,&fence_info,NULL,&fence)!=VK_SUCCESS) goto done;
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount=1u;
    submit.pCommandBuffers=&command;
    result=vkQueueSubmit(queue,1u,&submit,fence);
    if (result!=VK_SUCCESS) goto done;
    submitted=1;
    result=vkWaitForFences(device,1u,&fence,VK_TRUE,UINT64_MAX);
    if (result!=VK_SUCCESS) goto done;
    complete=1;
    out->image=image;
    out->memory=image_memory;
    out->width=width;
    out->height=height;
    image=VK_NULL_HANDLE;
    image_memory=VK_NULL_HANDLE;
    ok=1;
done:
    /* Do not release submitted GPU resources before execution finishes. On
     * a failed fence wait, attempt device idle before cleanup; a lost device
     * is a fatal diagnostic failure and must never activate Hardware mode. */
    if (submitted && !complete) (void)vkDeviceWaitIdle(device);
    if (mapped) vkUnmapMemory(device,staging_memory);
    if (fence) vkDestroyFence(device,fence,NULL);
    if (pool) vkDestroyCommandPool(device,pool,NULL);
    if (image) vkDestroyImage(device,image,NULL);
    if (image_memory) vkFreeMemory(device,image_memory,NULL);
    if (staging) vkDestroyBuffer(device,staging,NULL);
    if (staging_memory) vkFreeMemory(device,staging_memory,NULL);
    return ok;
}


/* R378 asynchronous *recording*, not an asynchronous operation on the caller's
 * source pointer. The host copy is complete on return. The caller joins the
 * transfer into the existing per-frame command buffer and retains staging
 * until that frame fence completes. This avoids per-texture command pools,
 * queue submissions, and vkWaitForFences during camera turns. */
int avp_vk_r32_deferred_prepare(VkPhysicalDevice gpu,VkDevice device,
    const uint32_t *words,uint32_t width,uint32_t height,size_t bytes,
    AvpVkDeferredR32Upload *u)
{
    VkPhysicalDeviceProperties limits={0};
    VkPhysicalDeviceMemoryProperties mem={0};
    VkFormatProperties fmt={0};
    VkMemoryRequirements req={0};
    VkMemoryAllocateInfo ai={0};
    VkBufferCreateInfo bi={0};
    VkImageCreateInfo ii={0};
    uint32_t index=0u;
    void *mapped=NULL;
    if(!u||u->staging||u->staging_memory||u->image.image||u->target||
       !gpu||!device||!words)return 0;
    vkGetPhysicalDeviceProperties(gpu,&limits);
    if(!avp_vk_upload_dimensions(width,height,bytes,limits.limits.maxImageDimension2D))return 0;
    vkGetPhysicalDeviceFormatProperties(gpu,VK_FORMAT_R32_UINT,&fmt);
    if(!(fmt.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))return 0;
    vkGetPhysicalDeviceMemoryProperties(gpu,&mem);
    bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size=(VkDeviceSize)bytes;
    bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device,&bi,NULL,&u->staging)!=VK_SUCCESS)goto fail;
    vkGetBufferMemoryRequirements(device,u->staging,&req);
    if(!avp_vk_memory_type(&mem,req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&index))goto fail;
    ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize=req.size;ai.memoryTypeIndex=index;
    if(vkAllocateMemory(device,&ai,NULL,&u->staging_memory)!=VK_SUCCESS||
       vkBindBufferMemory(device,u->staging,u->staging_memory,0u)!=VK_SUCCESS||
       vkMapMemory(device,u->staging_memory,0u,(VkDeviceSize)bytes,0u,&mapped)!=VK_SUCCESS)goto fail;
    memcpy(mapped,words,bytes);
    vkUnmapMemory(device,u->staging_memory);mapped=NULL;
    ii.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType=VK_IMAGE_TYPE_2D;
    ii.format=VK_FORMAT_R32_UINT;
    ii.extent.width=width;ii.extent.height=height;ii.extent.depth=1u;
    ii.mipLevels=1u;ii.arrayLayers=1u;
    ii.samples=VK_SAMPLE_COUNT_1_BIT;
    ii.tiling=VK_IMAGE_TILING_OPTIMAL;
    ii.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    if(vkCreateImage(device,&ii,NULL,&u->image.image)!=VK_SUCCESS)goto fail;
    vkGetImageMemoryRequirements(device,u->image.image,&req);
    if(!avp_vk_memory_type(&mem,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&index)&&
       !avp_vk_memory_type(&mem,req.memoryTypeBits,0u,&index))goto fail;
    ai.allocationSize=req.size;ai.memoryTypeIndex=index;
    if(vkAllocateMemory(device,&ai,NULL,&u->image.memory)!=VK_SUCCESS||
       vkBindImageMemory(device,u->image.image,u->image.memory,0u)!=VK_SUCCESS)goto fail;
    u->image.width=width;u->image.height=height;
    u->target=u->image.image;u->width=width;u->height=height;
    return 1;
fail:
    if(mapped)vkUnmapMemory(device,u->staging_memory);
    avp_vk_r32_deferred_dispose(device,u);
    return 0;
}

int avp_vk_r32_deferred_record(VkCommandBuffer command,
    const AvpVkDeferredR32Upload *u)
{
    VkImageMemoryBarrier before={0},after={0};
    VkBufferImageCopy copy={0};
    if(!command||!u||!u->staging||!u->target||!u->width||!u->height)return 0;
    before.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    before.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    before.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    before.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    before.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    before.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    before.image=u->target;
    before.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    before.subresourceRange.levelCount=1u;
    before.subresourceRange.layerCount=1u;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,0u,0u,NULL,0u,NULL,1u,&before);
    copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount=1u;
    copy.imageExtent.width=u->width;
    copy.imageExtent.height=u->height;
    copy.imageExtent.depth=1u;
    vkCmdCopyBufferToImage(command,u->staging,u->target,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1u,&copy);
    after=before;
    after.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    after.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    after.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    after.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0u,0u,NULL,0u,NULL,1u,&after);
    return 1;
}

void avp_vk_r32_deferred_dispose(VkDevice device,AvpVkDeferredR32Upload *u)
{
    if(!u)return;
    if(device){
        /* When adopted, the bank clears image.* and alone owns the image.
         * Destroy only this upload's temporary transfer staging here. */
        if(u->staging)vkDestroyBuffer(device,u->staging,NULL);
        if(u->staging_memory)vkFreeMemory(device,u->staging_memory,NULL);
        avp_vk_uploaded_r32_dispose(device,&u->image);
    }
    memset(u,0,sizeof(*u));
}
