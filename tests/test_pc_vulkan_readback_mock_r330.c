/* R330 standalone resource/copy sequencing mock. Build only against the
 * official SDK or an EXTERNAL disposable test header. Not a GPU test. */
#include "pc/pc_vulkan_readback.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define H(T,n) ((T)(uintptr_t)(n))
static int fail_at, attempts, live_buffers, live_memories, maps, copies, barriers;
static uint8_t pixels[4u*16u*16u];
static int step(void){return ++attempts!=fail_at;}
VkResult vkCreateBuffer(VkDevice d,const VkBufferCreateInfo*i,
    const VkAllocationCallbacks*a,VkBuffer*h)
{
    (void)d;(void)a;
    assert(i->sType==VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    assert(i->usage==VK_BUFFER_USAGE_TRANSFER_DST_BIT&&i->size==sizeof(pixels));
    if(!step())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *h=H(VkBuffer,1);live_buffers++;return VK_SUCCESS;
}
void vkDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks*a)
{(void)d;(void)a;assert(b==H(VkBuffer,1));assert(live_buffers==1);live_buffers--;}
void vkGetBufferMemoryRequirements(VkDevice d,VkBuffer b,VkMemoryRequirements*r)
{(void)d;(void)b;r->size=sizeof(pixels);r->alignment=1;r->memoryTypeBits=1;}
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice d,VkPhysicalDeviceMemoryProperties*m)
{(void)d;memset(m,0,sizeof(*m));m->memoryTypeCount=1;m->memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;}
VkResult vkAllocateMemory(VkDevice d,const VkMemoryAllocateInfo*i,
    const VkAllocationCallbacks*a,VkDeviceMemory*m)
{(void)d;(void)a;assert(i->allocationSize==sizeof(pixels)&&i->memoryTypeIndex==0);
    if(!step())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *m=H(VkDeviceMemory,2);live_memories++;return VK_SUCCESS;}
void vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks*a)
{(void)d;(void)a;assert(m==H(VkDeviceMemory,2));assert(live_memories==1);live_memories--;}
VkResult vkBindBufferMemory(VkDevice d,VkBuffer b,VkDeviceMemory m,VkDeviceSize o)
{(void)d;assert(b==H(VkBuffer,1)&&m==H(VkDeviceMemory,2)&&o==0u);
    return step()?VK_SUCCESS:VK_ERROR_OUT_OF_DEVICE_MEMORY;}
VkResult vkMapMemory(VkDevice d,VkDeviceMemory m,VkDeviceSize o,
    VkDeviceSize n,VkMemoryMapFlags f,void**p)
{(void)d;(void)f;assert(m==H(VkDeviceMemory,2)&&o==0u&&n==sizeof(pixels));
    if(!step())return VK_ERROR_MEMORY_MAP_FAILED;
    maps++;*p=pixels;return VK_SUCCESS;}
void vkUnmapMemory(VkDevice d,VkDeviceMemory m)
{(void)d;assert(m==H(VkDeviceMemory,2));assert(maps>0);maps--;}
void vkCmdCopyImageToBuffer(VkCommandBuffer c,VkImage i,VkImageLayout layout,
    VkBuffer b,uint32_t count,const VkBufferImageCopy*r)
{assert(c==H(VkCommandBuffer,3)&&i==H(VkImage,4));
 assert(layout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL&&b==H(VkBuffer,1));
 assert(count==1&&r->imageExtent.width==16&&r->imageExtent.height==16&&
        r->imageExtent.depth==1&&r->bufferRowLength==0&&r->bufferImageHeight==0);
 copies++;}
void vkCmdPipelineBarrier(VkCommandBuffer c,VkPipelineStageFlags src,
    VkPipelineStageFlags dst,VkDependencyFlags flags,uint32_t m,
    const VkMemoryBarrier*a,uint32_t n,const VkBufferMemoryBarrier*b,
    uint32_t count,const VkImageMemoryBarrier*r)
{(void)flags;(void)m;(void)a;(void)n;(void)b;
 assert(c==H(VkCommandBuffer,3)&&src==VK_PIPELINE_STAGE_TRANSFER_BIT&&
        dst==VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT&&count==1);
 assert(r->srcAccessMask==VK_ACCESS_TRANSFER_READ_BIT&&
        r->oldLayout==VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL&&
        r->newLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR&&
        r->srcQueueFamilyIndex==VK_QUEUE_FAMILY_IGNORED);
 barriers++;}
static void balanced(void)
{assert(live_buffers==0&&live_memories==0&&maps==0);}
int main(void)
{
    AvpVkReadback rb={0};
    uint32_t diffs=0;
    assert(!avp_vk_readback_create(H(VkPhysicalDevice,1),H(VkDevice,2),
        0,16,VK_FORMAT_B8G8R8A8_UNORM,&rb));
    for(int f=1;f<=3;f++) {
        fail_at=f;attempts=0;
        assert(!avp_vk_readback_create(H(VkPhysicalDevice,1),H(VkDevice,2),
            16,16,VK_FORMAT_B8G8R8A8_UNORM,&rb));
        balanced();
    }
    fail_at=0;attempts=0;
    assert(avp_vk_readback_create(H(VkPhysicalDevice,1),H(VkDevice,2),
        16,16,VK_FORMAT_B8G8R8A8_UNORM,&rb));
    assert(!avp_vk_readback_create(H(VkPhysicalDevice,1),H(VkDevice,2),
        16,16,VK_FORMAT_B8G8R8A8_UNORM,&rb));
    avp_vk_readback_record(H(VkCommandBuffer,3),H(VkImage,4),&rb);
    assert(copies==1&&barriers==1);
    for(uint32_t y=0;y<16u;y++)for(uint32_t x=0;x<16u;x++) {
        uint32_t rgb=((x^y)&1u)?0x00336699u:0x00bb8855u;
        size_t off=(y*16u+x)*4u;
        pixels[off+0]=(uint8_t)rgb;
        pixels[off+1]=(uint8_t)(rgb>>8u);
        pixels[off+2]=(uint8_t)(rgb>>16u);
        pixels[off+3]=255u;
    }
    assert(avp_vk_readback_verify(H(VkDevice,2),&rb,&diffs)&&diffs==0);
    pixels[19u]^=1u;
    assert(!avp_vk_readback_verify(H(VkDevice,2),&rb,&diffs)&&diffs==1u);
    assert(maps==0);
    avp_vk_readback_destroy(H(VkDevice,2),&rb);
    avp_vk_readback_destroy(H(VkDevice,2),&rb);
    balanced();
    puts("R330 mocked Vulkan output copy/map/error cleanup PASS (not real GPU)");
    return 0;
}
