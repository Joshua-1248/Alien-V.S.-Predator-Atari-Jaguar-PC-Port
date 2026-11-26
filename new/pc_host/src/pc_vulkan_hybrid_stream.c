/* R364: reusable, fence-serialized Hybrid Vulkan frame resources.
 * This SDK-only helper does not touch game state or the Software framebuffer.
 * The WSI caller waits for its previous frame fence BEFORE these functions. */
#include "pc/pc_vulkan_hybrid_stream.h"
#include "pc/pc_hybrid_stream_policy.h"
#include <string.h>

static int avp_stream_memory_type(const VkPhysicalDeviceMemoryProperties *p,
    uint32_t allowed,VkMemoryPropertyFlags wanted,uint32_t *out)
{
    if(!p||!out||p->memoryTypeCount>32u)return 0;
    for(uint32_t i=0u;i<p->memoryTypeCount;i++){
        if((allowed&(1u<<i))&&
           (p->memoryTypes[i].propertyFlags&wanted)==wanted){*out=i;return 1;}
    }
    return 0;
}

void avp_vk_hybrid_stream_dispose(VkDevice device,AvpVkHybridStream *s)
{
    if(!s)return;
    /* Caller must have completed ALL GPU work using these allocations. */
    if(device){
        if(s->geometry_mapped)vkUnmapMemory(device,s->geometry.memory);
        if(s->foreground_mapped)vkUnmapMemory(device,s->foreground_staging_memory);
        if(s->foreground_staging)vkDestroyBuffer(device,s->foreground_staging,NULL);
        if(s->foreground_staging_memory)vkFreeMemory(device,s->foreground_staging_memory,NULL);
        avp_vk_geometry_dispose(device,&s->geometry);
        avp_vk_uploaded_r32_dispose(device,&s->foreground);
    }
    memset(s,0,sizeof(*s));
}

int avp_vk_hybrid_stream_geometry(VkPhysicalDevice gpu,VkDevice device,
    const uint8_t *data,size_t bytes,AvpVkHybridStream *s,int *recreated)
{
    VkPhysicalDeviceMemoryProperties props={0};
    VkBuffer buffer=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    VkBufferCreateInfo bi={0};
    VkMemoryAllocateInfo ai={0};
    VkMemoryRequirements req={0};
    uint32_t idx=0u;
    void *mapped=NULL;
    size_t capacity=0u;
    int need=0;
    if(!s||!recreated||!gpu||!device||!data||
       !avp_hybrid_stream_capacity(bytes,(size_t)s->geometry_capacity,
           AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&need))return 0;
    *recreated=0;
    if(!need && s->geometry_mapped && s->geometry.buffer){
        /* R379: The world tape is freshly resolved every gameplay frame.
         * At 1080p this is ~32 MiB: searching for identical contents first
         * performs another full read, only to copy almost every moving frame.
         * For large tapes copy exactly once; preserve unchanged-buffer reuse
         * for tiny diagnostics/fixtures. Host-coherent memory and the prior
         * frame fence provide the same GPU visibility and ownership rules. */
        s->geometry_bytes_written=0u;
        if(bytes>=1024u*1024u || s->geometry.bytes!=(VkDeviceSize)bytes ||
           memcmp(s->geometry_mapped,data,bytes)!=0){
            memcpy(s->geometry_mapped,data,bytes);
            s->geometry_bytes_written=bytes;
        }
        s->geometry.bytes=(VkDeviceSize)bytes;
        return 1;
    }
    vkGetPhysicalDeviceMemoryProperties(gpu,&props);
    bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size=(VkDeviceSize)capacity;
    bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device,&bi,NULL,&buffer)!=VK_SUCCESS)goto fail;
    vkGetBufferMemoryRequirements(device,buffer,&req);
    if(req.size<capacity||!avp_stream_memory_type(&props,req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&idx))goto fail;
    ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize=req.size;
    ai.memoryTypeIndex=idx;
    if(vkAllocateMemory(device,&ai,NULL,&memory)!=VK_SUCCESS||
       vkBindBufferMemory(device,buffer,memory,0u)!=VK_SUCCESS||
       vkMapMemory(device,memory,0u,(VkDeviceSize)capacity,0u,&mapped)!=VK_SUCCESS)goto fail;
    memcpy(mapped,data,bytes);
    if(s->geometry_mapped)vkUnmapMemory(device,s->geometry.memory);
    avp_vk_geometry_dispose(device,&s->geometry);
    s->geometry.buffer=buffer;
    s->geometry.memory=memory;
    s->geometry.bytes=(VkDeviceSize)bytes;
    s->geometry_bytes_written=bytes;
    s->geometry_capacity=(VkDeviceSize)capacity;
    s->geometry_mapped=mapped;
    *recreated=1;
    return 1;
fail:
    if(mapped)vkUnmapMemory(device,memory);
    if(buffer)vkDestroyBuffer(device,buffer,NULL);
    if(memory)vkFreeMemory(device,memory,NULL);
    return 0;
}

int avp_vk_hybrid_stream_foreground(VkPhysicalDevice gpu,VkDevice device,
    const uint32_t *pixels,uint32_t width,uint32_t height,
    AvpVkHybridStream *s,int *recreated)
{
    VkPhysicalDeviceMemoryProperties props={0};
    VkPhysicalDeviceProperties limits={0};
    VkFormatProperties format_props={0};
    VkBuffer stage=VK_NULL_HANDLE;
    VkDeviceMemory stage_mem=VK_NULL_HANDLE;
    VkImage image=VK_NULL_HANDLE;
    VkDeviceMemory image_mem=VK_NULL_HANDLE;
    VkBufferCreateInfo bi={0};
    VkImageCreateInfo ii={0};
    VkMemoryAllocateInfo ai={0};
    VkMemoryRequirements req={0};
    uint32_t idx=0u;
    void *mapped=NULL;
    size_t bytes=0u;
    if(!s||!recreated||!gpu||!device||!pixels||
       !avp_hybrid_stream_foreground_size(width,height,&bytes))return 0;
    *recreated=0;
    if(s->foreground.image&&s->foreground.width==width&&
       s->foreground.height==height&&s->foreground_mapped&&
       s->foreground_bytes==bytes){
        int action=avp_hybrid_stream_foreground_upload_needed(
            (const uint32_t*)s->foreground_mapped,pixels,width,height,
            s->foreground_bytes,s->foreground_shader_layout && !s->foreground_dirty);
        if(action<0)return 0;
        if(action>0){
            /* R379: upload_needed already compared the exact bytes when the
             * GPU image was valid. A second full-size memcmp against mapped
             * staging wastes another 8 MiB read at 1080p every time the HUD
             * changes. For a fresh/unsubmitted image, rewriting identical
             * staging is harmless and preserves the mandatory first copy. */
            memcpy(s->foreground_mapped,pixels,bytes);
            s->foreground_dirty=1;
        }
        return 1;
    }
    vkGetPhysicalDeviceProperties(gpu,&limits);
    if(width>limits.limits.maxImageDimension2D||height>limits.limits.maxImageDimension2D)return 0;
    vkGetPhysicalDeviceFormatProperties(gpu,VK_FORMAT_R32_UINT,&format_props);
    if(!(format_props.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))return 0;
    vkGetPhysicalDeviceMemoryProperties(gpu,&props);
    bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size=(VkDeviceSize)bytes;
    bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device,&bi,NULL,&stage)!=VK_SUCCESS)goto fail;
    vkGetBufferMemoryRequirements(device,stage,&req);
    if(req.size<bytes||!avp_stream_memory_type(&props,req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&idx))goto fail;
    ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize=req.size;ai.memoryTypeIndex=idx;
    if(vkAllocateMemory(device,&ai,NULL,&stage_mem)!=VK_SUCCESS||
       vkBindBufferMemory(device,stage,stage_mem,0u)!=VK_SUCCESS||
       vkMapMemory(device,stage_mem,0u,(VkDeviceSize)bytes,0u,&mapped)!=VK_SUCCESS)goto fail;
    memcpy(mapped,pixels,bytes);
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
    if(vkCreateImage(device,&ii,NULL,&image)!=VK_SUCCESS)goto fail;
    vkGetImageMemoryRequirements(device,image,&req);
    if(!avp_stream_memory_type(&props,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&idx) &&
       !avp_stream_memory_type(&props,req.memoryTypeBits,0u,&idx))goto fail;
    ai.allocationSize=req.size;ai.memoryTypeIndex=idx;
    if(vkAllocateMemory(device,&ai,NULL,&image_mem)!=VK_SUCCESS||
       vkBindImageMemory(device,image,image_mem,0u)!=VK_SUCCESS)goto fail;
    /* All existing descriptor users have finished at WSI's frame fence. */
    if(s->foreground_mapped)vkUnmapMemory(device,s->foreground_staging_memory);
    if(s->foreground_staging)vkDestroyBuffer(device,s->foreground_staging,NULL);
    if(s->foreground_staging_memory)vkFreeMemory(device,s->foreground_staging_memory,NULL);
    avp_vk_uploaded_r32_dispose(device,&s->foreground);
    s->foreground.image=image;s->foreground.memory=image_mem;
    s->foreground.width=width;s->foreground.height=height;
    s->foreground_staging=stage;s->foreground_staging_memory=stage_mem;
    s->foreground_mapped=mapped;s->foreground_bytes=bytes;
    s->foreground_shader_layout=0;
    s->foreground_dirty=1; /* new image has undefined GPU contents */
    *recreated=1;
    return 1;
fail:
    if(mapped)vkUnmapMemory(device,stage_mem);
    if(image)vkDestroyImage(device,image,NULL);
    if(image_mem)vkFreeMemory(device,image_mem,NULL);
    if(stage)vkDestroyBuffer(device,stage,NULL);
    if(stage_mem)vkFreeMemory(device,stage_mem,NULL);
    return 0;
}

void avp_vk_hybrid_stream_record_foreground(VkCommandBuffer command,
    AvpVkHybridStream *s)
{
    VkImageMemoryBarrier before={0};
    VkImageMemoryBarrier after={0};
    VkBufferImageCopy copy={0};
    if(!command||!s||!s->foreground.image||!s->foreground_staging)return;
    /* Exact duplicate: previous transfer finished before the persistent
     * staging storage was inspected/updated, and the image is still in
     * SHADER_READ_ONLY_OPTIMAL. Record NO copy or barriers for this frame. */
    if(s->foreground_shader_layout && !s->foreground_dirty)return;
    before.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    before.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    before.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    before.image=s->foreground.image;
    before.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    before.subresourceRange.levelCount=1u;
    before.subresourceRange.layerCount=1u;
    before.oldLayout=s->foreground_shader_layout?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        VK_IMAGE_LAYOUT_UNDEFINED;
    before.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    before.srcAccessMask=s->foreground_shader_layout?VK_ACCESS_SHADER_READ_BIT:0u;
    before.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(command,s->foreground_shader_layout?
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,0u,0u,NULL,0u,NULL,1u,&before);
    copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount=1u;
    copy.imageExtent.width=s->foreground.width;
    copy.imageExtent.height=s->foreground.height;
    copy.imageExtent.depth=1u;
    vkCmdCopyBufferToImage(command,s->foreground_staging,s->foreground.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1u,&copy);
    after=before;
    after.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    after.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    after.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    after.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0u,0u,NULL,0u,NULL,1u,&after);
    s->foreground_shader_layout=1;
    s->foreground_dirty=0;
}
