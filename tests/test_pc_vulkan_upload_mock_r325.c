/* RE46 R325: independent mock of the Vulkan *calls*, not a mock SDK.
 * Build ONLY with official Vulkan-Headers (or a private syntax test header).
 * No GPU is simulated, no pixel-equivalence or live driver claim is made. */
#include "pc/pc_vulkan_upload.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HANDLE(T, n) ((T)(uintptr_t)(n))
static int fail_at, call_number, live_buffers, live_images, live_memory;
static int live_pool, live_fence, copied, barrier1, barrier2, submissions, waits, idles;
static uint32_t current_width,current_height;
static uint8_t memory_bytes[8192];
static uint32_t source[16*16];
static int next_ok(void){return ++call_number!=fail_at;}
static void balanced(void)
{ assert(!live_buffers&&!live_images&&!live_memory&&!live_pool&&!live_fence); }

void vkGetPhysicalDeviceProperties(VkPhysicalDevice g,VkPhysicalDeviceProperties*p)
{(void)g;memset(p,0,sizeof(*p));p->limits.maxImageDimension2D=8192;}
void vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice g,VkFormat f,VkFormatProperties*p)
{(void)g;assert(f==VK_FORMAT_R32_UINT);memset(p,0,sizeof(*p));p->optimalTilingFeatures=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;}
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice g,VkPhysicalDeviceMemoryProperties*p)
{(void)g;memset(p,0,sizeof(*p));p->memoryTypeCount=2;p->memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;p->memoryTypes[1].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;}
VkResult vkCreateBuffer(VkDevice d,const VkBufferCreateInfo*i,const VkAllocationCallbacks*a,VkBuffer*b)
{(void)d;(void)a;assert(i->size==sizeof(source));assert(i->usage==VK_BUFFER_USAGE_TRANSFER_SRC_BIT);if(!next_ok())return VK_ERROR_OUT_OF_HOST_MEMORY;*b=HANDLE(VkBuffer,100);live_buffers++;return VK_SUCCESS;}
void vkDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks*a)
{(void)d;(void)b;(void)a;assert(live_buffers==1);live_buffers--;}
void vkGetBufferMemoryRequirements(VkDevice d,VkBuffer b,VkMemoryRequirements*r)
{(void)d;(void)b;r->size=sizeof(memory_bytes);r->alignment=1u;r->memoryTypeBits=1u;}
VkResult vkAllocateMemory(VkDevice d,const VkMemoryAllocateInfo*i,const VkAllocationCallbacks*a,VkDeviceMemory*m)
{(void)d;(void)a;assert(i->allocationSize==sizeof(memory_bytes));if(!next_ok())return VK_ERROR_OUT_OF_HOST_MEMORY;*m=HANDLE(VkDeviceMemory,200+live_memory);live_memory++;return VK_SUCCESS;}
void vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks*a)
{(void)d;(void)m;(void)a;assert(live_memory>0);live_memory--;}
VkResult vkBindBufferMemory(VkDevice d,VkBuffer b,VkDeviceMemory m,VkDeviceSize o)
{(void)d;(void)b;(void)m;assert(o==0);return next_ok()?VK_SUCCESS:VK_ERROR_OUT_OF_DEVICE_MEMORY;}
VkResult vkBindImageMemory(VkDevice d,VkImage b,VkDeviceMemory m,VkDeviceSize o)
{(void)d;(void)b;(void)m;assert(o==0);return next_ok()?VK_SUCCESS:VK_ERROR_OUT_OF_DEVICE_MEMORY;}
VkResult vkMapMemory(VkDevice d,VkDeviceMemory m,VkDeviceSize o,VkDeviceSize s,VkMemoryMapFlags f,void**p)
{(void)d;(void)m;(void)f;assert(o==0&&s==sizeof(source));if(!next_ok())return VK_ERROR_MEMORY_MAP_FAILED;*p=memory_bytes;return VK_SUCCESS;}
void vkUnmapMemory(VkDevice d,VkDeviceMemory m){(void)d;(void)m;}
VkResult vkCreateImage(VkDevice d,const VkImageCreateInfo*i,const VkAllocationCallbacks*a,VkImage*h)
{(void)d;(void)a;assert(i->format==VK_FORMAT_R32_UINT&&i->tiling==VK_IMAGE_TILING_OPTIMAL);assert(i->usage==(VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT));assert(i->extent.width==16&&i->extent.height==16);current_width=i->extent.width;current_height=i->extent.height;if(!next_ok())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*h=HANDLE(VkImage,300);live_images++;return VK_SUCCESS;}
void vkDestroyImage(VkDevice d,VkImage i,const VkAllocationCallbacks*a)
{(void)d;(void)i;(void)a;assert(live_images==1);live_images--;}
void vkGetImageMemoryRequirements(VkDevice d,VkImage i,VkMemoryRequirements*r)
{(void)d;(void)i;r->size=sizeof(memory_bytes);r->alignment=1u;r->memoryTypeBits=2u;}
VkResult vkCreateCommandPool(VkDevice d,const VkCommandPoolCreateInfo*i,const VkAllocationCallbacks*a,VkCommandPool*p)
{(void)d;(void)a;assert(i->queueFamilyIndex==2u);if(!next_ok())return VK_ERROR_OUT_OF_HOST_MEMORY;*p=HANDLE(VkCommandPool,400);live_pool++;return VK_SUCCESS;}
void vkDestroyCommandPool(VkDevice d,VkCommandPool p,const VkAllocationCallbacks*a)
{(void)d;(void)p;(void)a;assert(live_pool==1);live_pool--;}
VkResult vkAllocateCommandBuffers(VkDevice d,const VkCommandBufferAllocateInfo*i,VkCommandBuffer*c)
{(void)d;assert(i->commandBufferCount==1u&&i->level==VK_COMMAND_BUFFER_LEVEL_PRIMARY);if(!next_ok())return VK_ERROR_OUT_OF_HOST_MEMORY;*c=HANDLE(VkCommandBuffer,500);return VK_SUCCESS;}
VkResult vkBeginCommandBuffer(VkCommandBuffer c,const VkCommandBufferBeginInfo*i)
{(void)c;(void)i;return next_ok()?VK_SUCCESS:VK_ERROR_DEVICE_LOST;}
void vkCmdPipelineBarrier(VkCommandBuffer c,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkDependencyFlags flags,uint32_t a,const VkMemoryBarrier*b,uint32_t d,const VkBufferMemoryBarrier*e,uint32_t n,const VkImageMemoryBarrier*f)
{(void)c;(void)flags;(void)a;(void)b;(void)d;(void)e;assert(n==1);if(!barrier1) {assert(src==VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT&&dst==VK_PIPELINE_STAGE_TRANSFER_BIT);assert(f->oldLayout==VK_IMAGE_LAYOUT_UNDEFINED&&f->newLayout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);barrier1=1;} else {assert(src==VK_PIPELINE_STAGE_TRANSFER_BIT&&dst==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);assert(f->oldLayout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL&&f->newLayout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);assert(f->srcAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT&&f->dstAccessMask==VK_ACCESS_SHADER_READ_BIT);barrier2=1;}}
void vkCmdCopyBufferToImage(VkCommandBuffer c,VkBuffer b,VkImage i,VkImageLayout layout,uint32_t n,const VkBufferImageCopy*region)
{(void)c;(void)b;(void)i;assert(layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL&&n==1);assert(region->imageExtent.width==current_width&&region->imageExtent.height==current_height);assert(region->bufferRowLength==0&&region->bufferImageHeight==0);assert(!memcmp(memory_bytes,source,sizeof(source)));copied++;}
VkResult vkEndCommandBuffer(VkCommandBuffer c){(void)c;return next_ok()?VK_SUCCESS:VK_ERROR_DEVICE_LOST;}
VkResult vkCreateFence(VkDevice d,const VkFenceCreateInfo*i,const VkAllocationCallbacks*a,VkFence*f)
{(void)d;(void)i;(void)a;if(!next_ok())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*f=HANDLE(VkFence,600);live_fence++;return VK_SUCCESS;}
void vkDestroyFence(VkDevice d,VkFence f,const VkAllocationCallbacks*a)
{(void)d;(void)f;(void)a;assert(live_fence==1);live_fence--;}
VkResult vkQueueSubmit(VkQueue q,uint32_t n,const VkSubmitInfo*i,VkFence f)
{(void)q;(void)f;assert(n==1&&i->commandBufferCount==1u);submissions++;return next_ok()?VK_SUCCESS:VK_ERROR_DEVICE_LOST;}
VkResult vkWaitForFences(VkDevice d,uint32_t n,const VkFence*f,VkBool32 all,uint64_t timeout)
{(void)d;(void)f;assert(n==1&&all==VK_TRUE&&timeout==UINT64_MAX);waits++;return next_ok()?VK_SUCCESS:VK_ERROR_DEVICE_LOST;}
VkResult vkDeviceWaitIdle(VkDevice d){(void)d;idles++;return VK_SUCCESS;}
int main(void)
{
    AvpVkUploadedR32 uploaded={0};
    for(size_t i=0;i<16u*16u;i++)source[i]=0xff123400u|(uint32_t)i;
    assert(!avp_vk_upload_r32uint(HANDLE(VkPhysicalDevice,1),HANDLE(VkDevice,2),
       HANDLE(VkQueue,3),2,source,16,16,sizeof(source)-1,&uploaded));
    balanced();
    for(int failure=1;failure<=14;failure++) {
        fail_at=failure;call_number=0;barrier1=barrier2=0;
        assert(!avp_vk_upload_r32uint(HANDLE(VkPhysicalDevice,1),HANDLE(VkDevice,2),
            HANDLE(VkQueue,3),2,source,16,16,sizeof(source),&uploaded));
        balanced();assert(!uploaded.image&&!uploaded.memory);
    }
    fail_at=0;call_number=0;barrier1=barrier2=0;
    assert(avp_vk_upload_r32uint(HANDLE(VkPhysicalDevice,1),HANDLE(VkDevice,2),
        HANDLE(VkQueue,3),2,source,16,16,sizeof(source),&uploaded));
    assert(uploaded.image&&uploaded.memory&&uploaded.width==16&&uploaded.height==16);
    /* A second upload into live output must NOT replace/leak its allocation. */
    assert(!avp_vk_upload_r32uint(HANDLE(VkPhysicalDevice,1),HANDLE(VkDevice,2),
        HANDLE(VkQueue,3),2,source,16,16,sizeof(source),&uploaded));
    assert(copied>0&&barrier1&&barrier2&&submissions>0&&waits>0);
    avp_vk_uploaded_r32_dispose(HANDLE(VkDevice,2),&uploaded);
    avp_vk_uploaded_r32_dispose(HANDLE(VkDevice,2),&uploaded);
    balanced();
    /* R378: game Hybrid uses the SAME exact copy/layout transitions but
     * defers them to the frame command. No queue submits, waits or pools. */
    {
        AvpVkDeferredR32Upload deferred={0};
        for(int failure=1;failure<=7;failure++){
            fail_at=failure;call_number=0;barrier1=barrier2=0;
            assert(!avp_vk_r32_deferred_prepare(HANDLE(VkPhysicalDevice,1),
                HANDLE(VkDevice,2),source,16,16,sizeof(source),&deferred));
            assert(!deferred.staging&&!deferred.staging_memory&&!deferred.target);
            balanced();
        }
        fail_at=0;call_number=0;barrier1=barrier2=0;
        int before_submits=submissions,before_waits=waits;
        assert(avp_vk_r32_deferred_prepare(HANDLE(VkPhysicalDevice,1),
            HANDLE(VkDevice,2),source,16,16,sizeof(source),&deferred));
        assert(deferred.staging&&deferred.staging_memory&&deferred.image.image&&
               deferred.image.memory&&deferred.target==deferred.image.image);
        assert(!avp_vk_r32_deferred_prepare(HANDLE(VkPhysicalDevice,1),
            HANDLE(VkDevice,2),source,16,16,sizeof(source),&deferred));
        assert(avp_vk_r32_deferred_record(HANDLE(VkCommandBuffer,500),&deferred));
        assert(barrier1&&barrier2&&copied>0);
        assert(submissions==before_submits&&waits==before_waits&&
               !live_pool&&!live_fence);
        avp_vk_r32_deferred_dispose(HANDLE(VkDevice,2),&deferred);
        balanced();
    }
    puts("R325/R378 Vulkan mock: synchronous diagnostics + batched game transfers PASS");
    return 0;
}
