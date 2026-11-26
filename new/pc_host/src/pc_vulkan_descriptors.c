/* R326. Bind the R325 uploaded integer images for Vulkan fragment sampling.
 * No engine/emulator/driver code copied; no gameplay renderer changes. */
#include "pc/pc_vulkan_descriptors.h"
#include "pc/pc_vulkan_descriptor_policy.h"
#include <string.h>

void avp_vk_sampled_pair_dispose(VkDevice device, AvpVkSampledPair *pair)
{
    if (!pair) return;
    /* The caller is responsible for waiting until all submitted GPU uses
     * have completed. Do not destroy descriptor resources in flight. */
    if (device) {
        if (pair->pool) vkDestroyDescriptorPool(device,pair->pool,NULL);
        if (pair->layout) vkDestroyDescriptorSetLayout(device,pair->layout,NULL);
        if (pair->sampler) vkDestroySampler(device,pair->sampler,NULL);
        if (pair->lut_view) vkDestroyImageView(device,pair->lut_view,NULL);
        if (pair->source_view) vkDestroyImageView(device,pair->source_view,NULL);
    }
    memset(pair,0,sizeof(*pair));
}

static int avp_vk_sampled_create_impl(VkDevice device,
    const AvpVkUploadedR32 *source, const AvpVkUploadedR32 *lut,
    const AvpVkUploadedGeometry *geometry, AvpVkSampledPair *out)
{
    AvpVkSampledPair local={0};
    VkImageViewCreateInfo view={0};
    VkSamplerCreateInfo sampler={0};
    VkDescriptorSetLayoutBinding bindings[3]={{0}};
    VkDescriptorSetLayoutCreateInfo layout={0};
    VkDescriptorPoolSize pool_size[2]={{0}};
    VkDescriptorPoolCreateInfo pool={0};
    VkDescriptorSetAllocateInfo alloc={0};
    VkDescriptorImageInfo images[2]={{0}};
    VkDescriptorBufferInfo ray_buffer={0};
    VkWriteDescriptorSet writes[3]={{0}};
    const uint32_t count=geometry?3u:2u;
    if (!out || !device || !source || !lut || !source->image || !lut->image ||
        !source->memory || !lut->memory || source->image==lut->image ||
        !avp_vk_descriptor_pair_dimensions(source->width,source->height,
                                          lut->width,lut->height) ||
        (geometry && (!geometry->buffer || !geometry->memory ||
                      geometry->bytes<56u || geometry->bytes>32u*1024u*1024u ||
                      (geometry->bytes&3u)!=0u))) return 0;
    /* Never overwrite an existing live descriptor allocation. */
    if (out->source_view || out->lut_view || out->sampler || out->layout ||
        out->pool || out->set) return 0;
    memset(out,0,sizeof(*out));

    view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.viewType=VK_IMAGE_VIEW_TYPE_2D;
    view.format=VK_FORMAT_R32_UINT;
    view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel=0u;
    view.subresourceRange.levelCount=1u;
    view.subresourceRange.baseArrayLayer=0u;
    view.subresourceRange.layerCount=1u;
    view.image=source->image;
    if (vkCreateImageView(device,&view,NULL,&local.source_view)!=VK_SUCCESS) goto fail;
    view.image=lut->image;
    if (vkCreateImageView(device,&view,NULL,&local.lut_view)!=VK_SUCCESS) goto fail;

    /* Integer texelFetch ignores filtering, but the descriptor is a
     * combined image sampler. Use valid nearest/no-anisotropy state. */
    sampler.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter=VK_FILTER_NEAREST;
    sampler.minFilter=VK_FILTER_NEAREST;
    sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.anisotropyEnable=VK_FALSE;
    sampler.compareEnable=VK_FALSE;
    sampler.maxLod=0.0f;
    if (vkCreateSampler(device,&sampler,NULL,&local.sampler)!=VK_SUCCESS) goto fail;

    for (uint32_t i=0u;i<2u;i++) {
        bindings[i].binding=i;
        bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount=1u;
        bindings[i].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if (geometry) {
        bindings[2].binding=2u;
        bindings[2].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount=1u;
        bindings[2].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    layout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.bindingCount=count;
    layout.pBindings=bindings;
    if (vkCreateDescriptorSetLayout(device,&layout,NULL,&local.layout)!=VK_SUCCESS) goto fail;
    pool_size[0].type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size[0].descriptorCount=2u;
    if (geometry) {
        pool_size[1].type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size[1].descriptorCount=1u;
    }
    pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.maxSets=1u;
    pool.poolSizeCount=geometry?2u:1u;
    pool.pPoolSizes=pool_size;
    if (vkCreateDescriptorPool(device,&pool,NULL,&local.pool)!=VK_SUCCESS) goto fail;
    alloc.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool=local.pool;
    alloc.descriptorSetCount=1u;
    alloc.pSetLayouts=&local.layout;
    if (vkAllocateDescriptorSets(device,&alloc,&local.set)!=VK_SUCCESS) goto fail;

    images[0].sampler=local.sampler;
    images[0].imageView=local.source_view;
    images[0].imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    images[1].sampler=local.sampler;
    images[1].imageView=local.lut_view;
    images[1].imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    for(uint32_t i=0u;i<2u;i++) {
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet=local.set;
        writes[i].dstBinding=i;
        writes[i].descriptorCount=1u;
        writes[i].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo=&images[i];
    }
    if (geometry) {
        ray_buffer.buffer=geometry->buffer;
        ray_buffer.offset=0u;
        ray_buffer.range=geometry->bytes;
        writes[2].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet=local.set;
        writes[2].dstBinding=2u;
        writes[2].descriptorCount=1u;
        writes[2].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo=&ray_buffer;
    }
    vkUpdateDescriptorSets(device,count,writes,0u,NULL);
    *out=local;
    return 1;
fail:
    avp_vk_sampled_pair_dispose(device,&local);
    return 0;
}

/* Source/LUT bindings remain backwards-compatible for R326/R327 tests. */
int avp_vk_sampled_pair_create(VkDevice device,
    const AvpVkUploadedR32 *source, const AvpVkUploadedR32 *lut,
    AvpVkSampledPair *out)
{
    return avp_vk_sampled_create_impl(device,source,lut,NULL,out);
}

int avp_vk_sampled_scene_create(VkDevice device,
    const AvpVkUploadedR32 *source, const AvpVkUploadedR32 *lut,
    const AvpVkUploadedGeometry *geometry, AvpVkSampledPair *out)
{
    if (!geometry) return 0;
    return avp_vk_sampled_create_impl(device,source,lut,geometry,out);
}

int avp_vk_sampled_scene_update_geometry(VkDevice device,
    const AvpVkUploadedGeometry *geometry,AvpVkSampledPair *pair)
{
    VkDescriptorBufferInfo info={0};
    VkWriteDescriptorSet write={0};
    if(!device||!geometry||!pair||!pair->set||!geometry->buffer||
       !geometry->memory||geometry->bytes<56u||
       geometry->bytes>32u*1024u*1024u||(geometry->bytes&3u))return 0;
    info.buffer=geometry->buffer;
    info.offset=0u;
    info.range=geometry->bytes;
    write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet=pair->set;
    write.dstBinding=2u;
    write.descriptorCount=1u;
    write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo=&info;
    vkUpdateDescriptorSets(device,1u,&write,0u,NULL);
    return 1;
}
