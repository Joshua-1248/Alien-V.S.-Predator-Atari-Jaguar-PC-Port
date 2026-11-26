/* Independently written Vulkan 1.0 diagnostic graphics pipeline. Unlike the
 * earlier clear-only WSI path, this binds and samples GPU-resident R32_UINT
 * images with fragment shader instructions. Not yet world geometry drawing. */
#include "pc/pc_vulkan_opaque_pipeline.h"
#include "pc/pc_vulkan_spirv_contract.h"
#include <string.h>

void avp_vk_opaque_pipeline_dispose(VkDevice device, AvpVkOpaquePipeline *p)
{
    if (!p) return;
    /* Destroy only after all submitted uses have completed. */
    if (device) {
        if (p->pipeline) vkDestroyPipeline(device,p->pipeline,NULL);
        if (p->layout) vkDestroyPipelineLayout(device,p->layout,NULL);
        if (p->fragment) vkDestroyShaderModule(device,p->fragment,NULL);
        if (p->vertex) vkDestroyShaderModule(device,p->vertex,NULL);
    }
    memset(p,0,sizeof(*p));
}

int avp_vk_opaque_pipeline_create_region(VkDevice device,
    const AvpVkSampledPair *pair, VkRenderPass pass, VkExtent2D extent,
    VkRect2D region,uint32_t push_constant_bytes,int dynamic_scissor,
    const uint32_t *vs, size_t vs_bytes,
    const uint32_t *fs, size_t fs_bytes, AvpVkOpaquePipeline *out)
{
    AvpVkOpaquePipeline p={0};
    VkShaderModuleCreateInfo info={0};
    VkPushConstantRange push={0};
    VkPipelineLayoutCreateInfo layout={0};
    VkPipelineShaderStageCreateInfo shaders[2]={{0}};
    VkPipelineVertexInputStateCreateInfo vertex={0};
    VkPipelineInputAssemblyStateCreateInfo assembly={0};
    VkViewport viewport={0};
    VkRect2D scissor={0};
    VkPipelineViewportStateCreateInfo view={0};
    VkPipelineDynamicStateCreateInfo dynamic={0};
    VkDynamicState scissor_state=VK_DYNAMIC_STATE_SCISSOR;
    VkPipelineRasterizationStateCreateInfo raster={0};
    VkPipelineMultisampleStateCreateInfo multisample={0};
    VkPipelineColorBlendAttachmentState attachment={0};
    VkPipelineColorBlendStateCreateInfo blending={0};
    VkGraphicsPipelineCreateInfo pipeline={0};

    if (!device || !pair || !pair->layout || !pair->set || !pass || !out ||
        out->vertex || out->fragment || out->layout || out->pipeline ||
        !extent.width || !extent.height ||
        region.offset.x<0 || region.offset.y<0 ||
        !region.extent.width || !region.extent.height ||
        (uint32_t)region.offset.x>extent.width ||
        (uint32_t)region.offset.y>extent.height ||
        region.extent.width>extent.width-(uint32_t)region.offset.x ||
        region.extent.height>extent.height-(uint32_t)region.offset.y ||
        push_constant_bytes<sizeof(AvpVkSamplingPush) ||
        push_constant_bytes>128u || (push_constant_bytes&3u)!=0u ||
        vs_bytes<20u || fs_bytes<20u || vs_bytes%4u || fs_bytes%4u ||
        !avp_spirv_stage_contract(vs,vs_bytes/4u,AVP_SPIRV_VERTEX) ||
        !avp_spirv_stage_contract(fs,fs_bytes/4u,AVP_SPIRV_FRAGMENT)) return 0;
    info.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize=vs_bytes;
    info.pCode=vs;
    if (vkCreateShaderModule(device,&info,NULL,&p.vertex)!=VK_SUCCESS) goto fail;
    info.codeSize=fs_bytes;
    info.pCode=fs;
    if (vkCreateShaderModule(device,&info,NULL,&p.fragment)!=VK_SUCCESS) goto fail;
    push.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset=0u;
    push.size=push_constant_bytes;
    layout.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount=1u;
    layout.pSetLayouts=&pair->layout;
    layout.pushConstantRangeCount=1u;
    layout.pPushConstantRanges=&push;
    if (vkCreatePipelineLayout(device,&layout,NULL,&p.layout)!=VK_SUCCESS) goto fail;
    shaders[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[0].stage=VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module=p.vertex;
    shaders[0].pName="main";
    shaders[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaders[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module=p.fragment;
    shaders[1].pName="main";
    vertex.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    viewport.x=(float)region.offset.x;
    viewport.y=(float)region.offset.y;
    viewport.width=(float)region.extent.width;
    viewport.height=(float)region.extent.height;
    viewport.maxDepth=1.0f;
    scissor=region;
    view.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    view.viewportCount=1u;
    view.pViewports=&viewport;
    view.scissorCount=1u;
    view.pScissors=dynamic_scissor?NULL:&scissor;
    if(dynamic_scissor){
        dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount=1u;
        dynamic.pDynamicStates=&scissor_state;
    }
    raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode=VK_POLYGON_MODE_FILL;
    raster.cullMode=VK_CULL_MODE_NONE;
    raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth=1.0f;
    multisample.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|
                              VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    blending.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount=1u;
    blending.pAttachments=&attachment;
    pipeline.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount=2u;
    pipeline.pStages=shaders;
    pipeline.pVertexInputState=&vertex;
    pipeline.pInputAssemblyState=&assembly;
    pipeline.pViewportState=&view;
    pipeline.pRasterizationState=&raster;
    pipeline.pMultisampleState=&multisample;
    pipeline.pColorBlendState=&blending;
    pipeline.pDynamicState=dynamic_scissor?&dynamic:NULL;
    pipeline.layout=p.layout;
    pipeline.renderPass=pass;
    pipeline.subpass=0u;
    if (vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1u,&pipeline,NULL,&p.pipeline)!=VK_SUCCESS)
        goto fail;
    *out=p;
    return 1;
fail:
    avp_vk_opaque_pipeline_dispose(device,&p);
    return 0;
}

/* Existing diagnostic callers always render the entire swapchain extent. */
int avp_vk_opaque_pipeline_create(VkDevice device,
    const AvpVkSampledPair *pair,VkRenderPass pass,VkExtent2D extent,
    const uint32_t *vertex_words,size_t vertex_bytes,
    const uint32_t *fragment_words,size_t fragment_bytes,
    AvpVkOpaquePipeline *out)
{
    VkRect2D full={{0,0},{0,0}};
    full.extent=extent;
    return avp_vk_opaque_pipeline_create_region(device,pair,pass,extent,full,
        (uint32_t)sizeof(AvpVkSamplingPush),0,vertex_words,vertex_bytes,fragment_words,fragment_bytes,out);
}
