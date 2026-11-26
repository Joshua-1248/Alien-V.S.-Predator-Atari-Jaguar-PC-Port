/* R326 test for public Vulkan API-call sequencing. Requires official Vulkan
 * headers in normal optional SDK builds. No GPU is emulated. */
#include "pc/pc_vulkan_descriptors.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define H(t,n) ((t)(uintptr_t)(n))
static int fail_at, calls, live_views, live_samplers, live_layouts, live_pools;
static unsigned writes_seen, views_seen;
static unsigned expect_scene;
static int attempt(void) {return ++calls!=fail_at;}
static void balanced(void) {assert(!live_views&&!live_samplers&&!live_layouts&&!live_pools);}
VkResult vkCreateImageView(VkDevice d,const VkImageViewCreateInfo*i,
    const VkAllocationCallbacks*a,VkImageView*v)
{
    (void)d;(void)a;
    assert(i->sType==VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    assert(i->viewType==VK_IMAGE_VIEW_TYPE_2D&&i->format==VK_FORMAT_R32_UINT);
    assert(i->subresourceRange.aspectMask==VK_IMAGE_ASPECT_COLOR_BIT);
    assert(i->subresourceRange.levelCount==1u&&i->subresourceRange.layerCount==1u);
    assert(i->image==H(VkImage,100)||i->image==H(VkImage,200));
    if (!attempt()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *v=H(VkImageView,300+(unsigned)live_views);live_views++;views_seen++;
    return VK_SUCCESS;
}
void vkDestroyImageView(VkDevice d,VkImageView v,const VkAllocationCallbacks*a)
{(void)d;(void)v;(void)a;assert(live_views>0);live_views--;}
VkResult vkCreateSampler(VkDevice d,const VkSamplerCreateInfo*i,const VkAllocationCallbacks*a,VkSampler*s)
{
    (void)d;(void)a;assert(i->magFilter==VK_FILTER_NEAREST&&i->minFilter==VK_FILTER_NEAREST);
    assert(i->anisotropyEnable==VK_FALSE&&i->compareEnable==VK_FALSE);
    assert(i->addressModeU==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    if(!attempt())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *s=H(VkSampler,400);live_samplers++;return VK_SUCCESS;
}
void vkDestroySampler(VkDevice d,VkSampler s,const VkAllocationCallbacks*a)
{(void)d;(void)s;(void)a;assert(live_samplers==1);live_samplers--;}
VkResult vkCreateDescriptorSetLayout(VkDevice d,const VkDescriptorSetLayoutCreateInfo*i,const VkAllocationCallbacks*a,VkDescriptorSetLayout*l)
{
    (void)d;(void)a;assert(i->bindingCount==(expect_scene?3u:2u));
    for(unsigned k=0;k<2;k++){
        assert(i->pBindings[k].binding==k);
        assert(i->pBindings[k].descriptorType==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        assert(i->pBindings[k].descriptorCount==1u);
        assert(i->pBindings[k].stageFlags==VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    if(expect_scene) {
        assert(i->pBindings[2].binding==2u);
        assert(i->pBindings[2].descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        assert(i->pBindings[2].descriptorCount==1u);
        assert(i->pBindings[2].stageFlags==VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    if(!attempt())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *l=H(VkDescriptorSetLayout,500);live_layouts++;return VK_SUCCESS;
}
void vkDestroyDescriptorSetLayout(VkDevice d,VkDescriptorSetLayout l,const VkAllocationCallbacks*a)
{(void)d;(void)l;(void)a;assert(live_layouts==1);live_layouts--;}
VkResult vkCreateDescriptorPool(VkDevice d,const VkDescriptorPoolCreateInfo*i,const VkAllocationCallbacks*a,VkDescriptorPool*p)
{
    (void)d;(void)a;assert(i->maxSets==1u&&i->poolSizeCount==(expect_scene?2u:1u));
    assert(i->pPoolSizes[0].descriptorCount==2u);
    assert(i->pPoolSizes[0].type==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    if(expect_scene){
        assert(i->pPoolSizes[1].descriptorCount==1u);
        assert(i->pPoolSizes[1].type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    if(!attempt())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *p=H(VkDescriptorPool,600);live_pools++;return VK_SUCCESS;
}
void vkDestroyDescriptorPool(VkDevice d,VkDescriptorPool p,const VkAllocationCallbacks*a)
{(void)d;(void)p;(void)a;assert(live_pools==1);live_pools--;}
VkResult vkAllocateDescriptorSets(VkDevice d,const VkDescriptorSetAllocateInfo*i,VkDescriptorSet*s)
{
    (void)d;assert(i->descriptorSetCount==1u&&i->descriptorPool==H(VkDescriptorPool,600));
    assert(*i->pSetLayouts==H(VkDescriptorSetLayout,500));
    if(!attempt())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *s=H(VkDescriptorSet,700);return VK_SUCCESS;
}
void vkUpdateDescriptorSets(VkDevice d,uint32_t count,const VkWriteDescriptorSet*w,uint32_t copies,const VkCopyDescriptorSet*c)
{
    (void)d;(void)c;assert(count==(expect_scene?3u:2u)&&copies==0u);
    for(unsigned k=0;k<2;k++){
        assert(w[k].dstSet==H(VkDescriptorSet,700));
        assert(w[k].dstBinding==k&&w[k].descriptorCount==1u);
        assert(w[k].descriptorType==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        assert(w[k].pImageInfo->imageLayout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        assert(w[k].pImageInfo->sampler==H(VkSampler,400));
        assert(w[k].pImageInfo->imageView==H(VkImageView,300+k));
    }
    if(expect_scene){
        const VkDescriptorBufferInfo *ray=(const VkDescriptorBufferInfo*)w[2].pBufferInfo;
        assert(w[2].dstBinding==2u);
        assert(w[2].descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        assert(ray && ray->buffer==H(VkBuffer,800) && ray->offset==0u && ray->range==96u);
    }
    writes_seen++;
}
int main(void)
{
    AvpVkUploadedR32 src={H(VkImage,100),H(VkDeviceMemory,101),1254u,1254u};
    AvpVkUploadedR32 lut={H(VkImage,200),H(VkDeviceMemory,201),256u,256u};
    AvpVkSampledPair out={0};
    AvpVkUploadedGeometry geom={H(VkBuffer,800),H(VkDeviceMemory,801),96u};
    assert(!avp_vk_sampled_pair_create(H(VkDevice,1),&src,&src,&out));
    lut.width=255u;
    assert(!avp_vk_sampled_pair_create(H(VkDevice,1),&src,&lut,&out));
    lut.width=256u;
    for(int failure=1;failure<=6;failure++){
        calls=0;fail_at=failure;
        assert(!avp_vk_sampled_pair_create(H(VkDevice,1),&src,&lut,&out));
        assert(!out.set);balanced();
    }
    fail_at=0;calls=0;
    assert(avp_vk_sampled_pair_create(H(VkDevice,1),&src,&lut,&out));
    assert(out.set&&out.layout&&writes_seen==1u&&views_seen>=2u);
    assert(!avp_vk_sampled_pair_create(H(VkDevice,1),&src,&lut,&out));
    avp_vk_sampled_pair_dispose(H(VkDevice,1),&out);
    avp_vk_sampled_pair_dispose(H(VkDevice,1),&out);
    balanced();
    /* R333: invalid geometry must not allocate or replace a working set. */
    expect_scene=1u;
    geom.bytes=52u;
    assert(!avp_vk_sampled_scene_create(H(VkDevice,1),&src,&lut,&geom,&out));
    geom.bytes=96u;
    geom.buffer=0;
    assert(!avp_vk_sampled_scene_create(H(VkDevice,1),&src,&lut,&geom,&out));
    geom.buffer=H(VkBuffer,800);
    assert(!avp_vk_sampled_scene_create(H(VkDevice,1),&src,&lut,NULL,&out));
    for(int failure=1; failure<=6; failure++){
        calls=0;fail_at=failure;
        assert(!avp_vk_sampled_scene_create(H(VkDevice,1),&src,&lut,&geom,&out));
        assert(!out.set);balanced();
    }
    calls=0;fail_at=0;
    assert(avp_vk_sampled_scene_create(H(VkDevice,1),&src,&lut,&geom,&out));
    assert(out.set && writes_seen==2u);
    assert(!avp_vk_sampled_scene_create(H(VkDevice,1),&src,&lut,&geom,&out));
    avp_vk_sampled_pair_dispose(H(VkDevice,1),&out);
    balanced();
    puts("R333 Vulkan sampled-pair and ray-storage descriptor mock PASS");
    return 0;
}
