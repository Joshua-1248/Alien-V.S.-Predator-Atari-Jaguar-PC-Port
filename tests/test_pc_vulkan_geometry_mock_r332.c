/* Vulkan API-call mock, compiled only with an official Vulkan SDK or a
 * private non-redistributed syntax-shape test header. No real GPU simulated. */
#include "pc/pc_vulkan_geometry_upload.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define H(T,n) ((T)(uintptr_t)(n))
static int fail_at,call_no,live_buffers,live_memory,live_pool,live_fence;
static int copy_count,barrier_count,submit_count,wait_count;
static unsigned char gpu_staging[256];
static uint8_t source[96];
static int ok(void){return ++call_no!=fail_at;}
static void balanced(void)
{assert(live_buffers==0 && live_memory==0 && live_pool==0 && live_fence==0);}
static void set32(uint8_t*p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8u*i));}
static void set64(uint8_t*p,double d)
{uint64_t n;memcpy(&n,&d,sizeof(n));set32(p,(uint32_t)n);set32(p+4,(uint32_t)(n>>32));}
static void make_source(void)
{
    memset(source,0,sizeof(source));
    set32(source,0x57525641u);set32(source+4,1u);
    set32(source+8,1u);set32(source+12,1u);set32(source+16,1u);
    set32(source+20,32u);set32(source+24,56u);
    set32(source+36,1u);set64(source+40,1.0);set32(source+48,1u);
    set64(source+56,1.0);set64(source+64,0.5);
    set32(source+72,12u);set32(source+84,1u);
}
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice g,VkPhysicalDeviceMemoryProperties*p)
{
    (void)g;memset(p,0,sizeof(*p));p->memoryTypeCount=2;
    p->memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    p->memoryTypes[1].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}
VkResult vkCreateBuffer(VkDevice d,const VkBufferCreateInfo*i,const VkAllocationCallbacks*a,VkBuffer*b)
{
    (void)d;(void)a;
    assert(i->size==sizeof(source));
    assert((i->usage==VK_BUFFER_USAGE_TRANSFER_SRC_BIT&&live_buffers==0) ||
           (i->usage==(VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)&&live_buffers==1));
    if(!ok())return -1;
    *b=H(VkBuffer,100+live_buffers);live_buffers++;return VK_SUCCESS;
}
void vkDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks*a)
{(void)d;(void)b;(void)a;assert(live_buffers>0);live_buffers--;}
void vkGetBufferMemoryRequirements(VkDevice d,VkBuffer b,VkMemoryRequirements*r)
{
    (void)d;memset(r,0,sizeof(*r));r->size=sizeof(gpu_staging);r->memoryTypeBits=(b==H(VkBuffer,100))?1u:2u;
}
VkResult vkAllocateMemory(VkDevice d,const VkMemoryAllocateInfo*i,const VkAllocationCallbacks*a,VkDeviceMemory*m)
{
    (void)d;(void)a;assert(i->allocationSize==sizeof(gpu_staging));
    assert((i->memoryTypeIndex==0 && live_memory==0)||(i->memoryTypeIndex==1 && live_memory==1));
    if(!ok())return -1;
    *m=H(VkDeviceMemory,200+live_memory);live_memory++;return VK_SUCCESS;
}
void vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks*a)
{(void)d;(void)m;(void)a;assert(live_memory>0);live_memory--;}
VkResult vkBindBufferMemory(VkDevice d,VkBuffer b,VkDeviceMemory m,VkDeviceSize offset)
{(void)d;(void)b;(void)m;assert(offset==0);return ok()?VK_SUCCESS:-1;}
VkResult vkMapMemory(VkDevice d,VkDeviceMemory m,VkDeviceSize offset,VkDeviceSize size,VkMemoryMapFlags flags,void**p)
{
    (void)d;(void)m;(void)flags;assert(offset==0&&size==sizeof(source));
    if(!ok())return -1;
    *p=gpu_staging;return VK_SUCCESS;
}
void vkUnmapMemory(VkDevice d,VkDeviceMemory m){(void)d;(void)m;}
VkResult vkCreateCommandPool(VkDevice d,const VkCommandPoolCreateInfo*i,const VkAllocationCallbacks*a,VkCommandPool*p)
{(void)d;(void)a;assert(i->queueFamilyIndex==2u);if(!ok())return -1;*p=H(VkCommandPool,400);live_pool++;return VK_SUCCESS;}
void vkDestroyCommandPool(VkDevice d,VkCommandPool p,const VkAllocationCallbacks*a)
{(void)d;(void)p;(void)a;assert(live_pool>0);live_pool--;}
VkResult vkAllocateCommandBuffers(VkDevice d,const VkCommandBufferAllocateInfo*i,VkCommandBuffer*c)
{(void)d;assert(i->commandBufferCount==1u);if(!ok())return -1;*c=H(VkCommandBuffer,500);return VK_SUCCESS;}
VkResult vkBeginCommandBuffer(VkCommandBuffer c,const VkCommandBufferBeginInfo*i)
{(void)c;(void)i;return ok()?VK_SUCCESS:-1;}
void vkCmdCopyBuffer(VkCommandBuffer c,VkBuffer src,VkBuffer dst,uint32_t count,const VkBufferCopy*regions)
{
    (void)c;assert(src==H(VkBuffer,100)&&dst==H(VkBuffer,101));
    assert(count==1 && regions->size==sizeof(source));
    assert(!memcmp(source,gpu_staging,sizeof(source)));copy_count++;
}
void vkCmdPipelineBarrier(VkCommandBuffer c,VkPipelineStageFlags src,VkPipelineStageFlags dst,
    VkDependencyFlags flags,uint32_t a,const VkMemoryBarrier*b,uint32_t n,
    const VkBufferMemoryBarrier*e,uint32_t f,const VkImageMemoryBarrier*g)
{
    (void)c;(void)flags;(void)a;(void)b;(void)f;(void)g;
    assert(src==VK_PIPELINE_STAGE_TRANSFER_BIT && dst==VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    assert(n==1 && e->buffer==H(VkBuffer,101) && e->size==sizeof(source));
    assert(e->srcAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT &&
           e->dstAccessMask==VK_ACCESS_SHADER_READ_BIT);
    barrier_count++;
}
VkResult vkEndCommandBuffer(VkCommandBuffer c){(void)c;return ok()?VK_SUCCESS:-1;}
VkResult vkCreateFence(VkDevice d,const VkFenceCreateInfo*i,const VkAllocationCallbacks*a,VkFence*f)
{(void)d;(void)i;(void)a;if(!ok())return -1;*f=H(VkFence,600);live_fence++;return VK_SUCCESS;}
void vkDestroyFence(VkDevice d,VkFence f,const VkAllocationCallbacks*a)
{(void)d;(void)f;(void)a;assert(live_fence>0);live_fence--;}
VkResult vkQueueSubmit(VkQueue q,uint32_t n,const VkSubmitInfo*i,VkFence f)
{(void)q;(void)f;assert(n==1&&i->commandBufferCount==1u);submit_count++;return ok()?VK_SUCCESS:-1;}
VkResult vkWaitForFences(VkDevice d,uint32_t n,const VkFence*f,VkBool32 all,uint64_t timeout)
{(void)d;(void)f;assert(n==1&&all==VK_TRUE&&timeout==UINT64_MAX);wait_count++;return ok()?VK_SUCCESS:-1;}
VkResult vkDeviceWaitIdle(VkDevice d){(void)d;return VK_SUCCESS;}
int main(void)
{
    AvpVkUploadedGeometry geometry={0};
    make_source();
    assert(!avp_vk_geometry_upload(H(VkPhysicalDevice,1),H(VkDevice,2),
        H(VkQueue,3),2u,source,sizeof(source)-1u,&geometry));balanced();
    for(int failure=1;failure<=14;failure++){
        fail_at=failure;call_no=0;
        assert(!avp_vk_geometry_upload(H(VkPhysicalDevice,1),H(VkDevice,2),
            H(VkQueue,3),2u,source,sizeof(source),&geometry));
        balanced();assert(!geometry.buffer&&!geometry.memory);
    }
    fail_at=0;call_no=0;
    assert(avp_vk_geometry_upload(H(VkPhysicalDevice,1),H(VkDevice,2),
        H(VkQueue,3),2u,source,sizeof(source),&geometry));
    assert(geometry.buffer && geometry.memory && geometry.bytes==sizeof(source));
    assert(!avp_vk_geometry_upload(H(VkPhysicalDevice,1),H(VkDevice,2),
        H(VkQueue,3),2u,source,sizeof(source),&geometry));
    assert(copy_count && barrier_count && submit_count && wait_count);
    avp_vk_geometry_dispose(H(VkDevice,2),&geometry);
    avp_vk_geometry_dispose(H(VkDevice,2),&geometry);
    balanced();
    puts("R332 Vulkan geometry STORAGE_BUFFER mock: copy, barrier, fence and failure cleanup PASS");
    return 0;
}
