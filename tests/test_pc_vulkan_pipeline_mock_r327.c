/* R327 isolated Vulkan API sequencing mock, not a physical GPU execution. */
#include "pc/pc_vulkan_opaque_pipeline.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define H(t,n) ((t)(uintptr_t)(n))
static int fail_step, attempt_count, live_modules, live_layout, live_pipeline;
static int next_attempt(void) {return ++attempt_count!=fail_step;}
VkResult vkCreateShaderModule(VkDevice d,const VkShaderModuleCreateInfo*i,
    const VkAllocationCallbacks*a,VkShaderModule*v)
{
    (void)d;(void)a;
    assert(i->sType==VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    assert(i->codeSize==15u*4u&&i->pCode[0]==0x07230203u);
    if(!next_attempt())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *v=H(VkShaderModule,200u+live_modules);
    live_modules++;return VK_SUCCESS;
}
void vkDestroyShaderModule(VkDevice d,VkShaderModule v,const VkAllocationCallbacks*a)
{(void)d;(void)v;(void)a;assert(live_modules>0);live_modules--;}
VkResult vkCreatePipelineLayout(VkDevice d,const VkPipelineLayoutCreateInfo*i,
    const VkAllocationCallbacks*a,VkPipelineLayout*l)
{
    (void)d;(void)a;
    assert(i->sType==VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    assert(i->setLayoutCount==1u&&*i->pSetLayouts==H(VkDescriptorSetLayout,500));
    assert(i->pushConstantRangeCount==1u&&i->pPushConstantRanges->size==16u);
    assert(i->pPushConstantRanges->stageFlags==VK_SHADER_STAGE_FRAGMENT_BIT);
    if(!next_attempt())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *l=H(VkPipelineLayout,300);live_layout++;return VK_SUCCESS;
}
void vkDestroyPipelineLayout(VkDevice d,VkPipelineLayout l,const VkAllocationCallbacks*a)
{(void)d;(void)l;(void)a;assert(live_layout==1);live_layout--;}
VkResult vkCreateGraphicsPipelines(VkDevice d,VkPipelineCache cache,uint32_t n,
    const VkGraphicsPipelineCreateInfo*i,const VkAllocationCallbacks*a,VkPipeline*v)
{
    (void)d;(void)cache;(void)a;
    assert(n==1u&&i->stageCount==2u&&i->layout==H(VkPipelineLayout,300));
    assert(i->pStages[0].stage==VK_SHADER_STAGE_VERTEX_BIT);
    assert(i->pStages[1].stage==VK_SHADER_STAGE_FRAGMENT_BIT);
    assert(i->pInputAssemblyState->topology==VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    assert(i->pMultisampleState->rasterizationSamples==VK_SAMPLE_COUNT_1_BIT);
    assert(i->pColorBlendState->attachmentCount==1u);
    assert(i->pViewportState->pViewports->width==640.0f);
    if(!next_attempt())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    *v=H(VkPipeline,400);live_pipeline++;return VK_SUCCESS;
}
void vkDestroyPipeline(VkDevice d,VkPipeline p,const VkAllocationCallbacks*a)
{(void)d;(void)p;(void)a;assert(live_pipeline==1);live_pipeline--;}
static void balanced(void)
{assert(live_modules==0&&live_layout==0&&live_pipeline==0);}
int main(void)
{
    uint32_t vertex[]={0x07230203u,0x00010000u,0u,32u,0u,
                       (2u<<16)|17u,1u,(3u<<16)|14u,0u,1u,
                       (5u<<16)|15u,0u,4u,0x6e69616du,0u};
    uint32_t fragment[15];
    AvpVkSampledPair pair={0};
    AvpVkOpaquePipeline out={0};
    VkExtent2D extent={640u,480u};
    memcpy(fragment,vertex,sizeof(fragment));fragment[11]=4u;
    pair.layout=H(VkDescriptorSetLayout,500);pair.set=H(VkDescriptorSet,700);
    assert(!avp_vk_opaque_pipeline_create(H(VkDevice,1),&pair,H(VkRenderPass,1),
         extent,fragment,sizeof(fragment),vertex,sizeof(vertex),&out));
    assert(!out.pipeline);balanced();
    for(int error=1;error<=4;error++){
        fail_step=error;attempt_count=0;
        assert(!avp_vk_opaque_pipeline_create(H(VkDevice,1),&pair,H(VkRenderPass,1),
            extent,vertex,sizeof(vertex),fragment,sizeof(fragment),&out));
        assert(!out.vertex&&!out.pipeline);balanced();
    }
    fail_step=0;attempt_count=0;
    assert(avp_vk_opaque_pipeline_create(H(VkDevice,1),&pair,H(VkRenderPass,1),
           extent,vertex,sizeof(vertex),fragment,sizeof(fragment),&out));
    assert(out.pipeline&&out.fragment&&out.layout);
    assert(!avp_vk_opaque_pipeline_create(H(VkDevice,1),&pair,H(VkRenderPass,1),
           extent,vertex,sizeof(vertex),fragment,sizeof(fragment),&out));
    avp_vk_opaque_pipeline_dispose(H(VkDevice,1),&out);
    avp_vk_opaque_pipeline_dispose(H(VkDevice,1),&out);
    balanced();
    puts("R327 Vulkan graphics pipeline API sequence/failure ownership PASS (mock)");
    return 0;
}
