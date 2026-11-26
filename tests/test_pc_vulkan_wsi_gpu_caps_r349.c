/* R349: CPU-only policy tests for selecting the ACTUAL requested Vulkan GPU.
 * These are checks of the discovery/selection contract, NOT a Vulkan driver
 * build, SPIR-V compile, render, or proof of GPU capabilities on this host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pc/pc_vulkan_wsi_policy.h"

static AvpWsiShaderCaps good_caps(void)
{
    AvpWsiShaderCaps c={0};
    c.r32_optimal_sampled=1u;
    c.max_image_dimension_2d=4096u;
    c.max_storage_buffer_range=128u*1024u*1024u;
    c.max_push_constants_size=128u;
    c.max_per_stage_sampled_images=16u;
    c.max_set_sampled_images=96u;
    c.max_per_stage_samplers=16u;
    c.max_set_samplers=96u;
    c.max_per_stage_storage_buffers=4u;
    c.max_set_storage_buffers=16u;
    return c;
}

int main(void)
{
    AvpWsiShaderCaps c=good_caps();
    const char *nvidia="10de:2504:01aabbcc:0";
    const char *intel="8086:1234:02aabbcc:0";
    assert(avp_wsi_gpu_candidate_rank(2u,1,nvidia,NULL)==30);
    assert(avp_wsi_gpu_candidate_rank(1u,1,intel,"")==20);
    assert(avp_wsi_gpu_candidate_rank(3u,1,intel,intel)==10);
    assert(avp_wsi_gpu_candidate_rank(2u,1,nvidia,nvidia)==30);
    assert(avp_wsi_gpu_candidate_rank(2u,1,nvidia,intel)==0);
    assert(avp_wsi_gpu_candidate_rank(1u,1,intel,nvidia)==0);
    assert(!avp_wsi_gpu_candidate_rank(4u,1,nvidia,NULL));
    assert(!avp_wsi_gpu_candidate_rank(0u,1,nvidia,NULL));
    assert(!avp_wsi_gpu_candidate_rank(2u,0,nvidia,NULL));
    assert(!avp_wsi_gpu_candidate_rank(2u,1,NULL,NULL));
    assert(!avp_wsi_gpu_candidate_rank(2u,1,"",NULL));
    /* A stale explicit key must NOT match either an integrated or discrete
     * available GPU; automatic choice still ranks a discrete GPU higher. */
    assert(!avp_wsi_gpu_candidate_rank(2u,1,nvidia,"stale:gpu:0"));
    assert(!avp_wsi_gpu_candidate_rank(1u,1,intel,"stale:gpu:0"));
    assert(avp_wsi_gpu_candidate_rank(2u,1,nvidia,NULL) >
           avp_wsi_gpu_candidate_rank(1u,1,intel,NULL));
    assert(avp_wsi_shader_caps_sufficient(&c,256u));
    assert(avp_wsi_shader_caps_sufficient(&c,720u));
    assert(!avp_wsi_shader_caps_sufficient(NULL,256u));
    assert(!avp_wsi_shader_caps_sufficient(&c,0u));
    assert(!avp_wsi_shader_caps_sufficient(&c,255u));
    assert(!avp_wsi_shader_caps_sufficient(&c,4097u));
    c.r32_optimal_sampled=0u;assert(!avp_wsi_shader_caps_sufficient(&c,720u));c=good_caps();
    c.max_image_dimension_2d=719u;assert(!avp_wsi_shader_caps_sufficient(&c,720u));c=good_caps();
    c.max_storage_buffer_range=(32u*1024u*1024u)-1u;
    assert(!avp_wsi_shader_caps_sufficient(&c,256u));c=good_caps();
    c.max_push_constants_size=15u;assert(!avp_wsi_shader_caps_sufficient(&c,256u));c=good_caps();
    c.max_per_stage_sampled_images=1u;assert(!avp_wsi_shader_caps_sufficient(&c,256u));c=good_caps();
    c.max_set_sampled_images=1u;assert(!avp_wsi_shader_caps_sufficient(&c,256u));c=good_caps();
    c.max_per_stage_samplers=1u;assert(!avp_wsi_shader_caps_sufficient(&c,256u));c=good_caps();
    c.max_set_samplers=1u;assert(!avp_wsi_shader_caps_sufficient(&c,256u));c=good_caps();
    c.max_per_stage_storage_buffers=0u;assert(!avp_wsi_shader_caps_sufficient(&c,256u));c=good_caps();
    c.max_set_storage_buffers=0u;assert(!avp_wsi_shader_caps_sufficient(&c,256u));c=good_caps();
    c.max_image_dimension_2d=720u;
    c.max_storage_buffer_range=32u*1024u*1024u;
    c.max_push_constants_size=16u;
    c.max_per_stage_sampled_images=2u;
    c.max_set_sampled_images=2u;
    c.max_per_stage_samplers=2u;
    c.max_set_samplers=2u;
    c.max_per_stage_storage_buffers=1u;
    c.max_set_storage_buffers=1u;
    assert(avp_wsi_shader_caps_sufficient(&c,720u));
    puts("R349 explicit Vulkan GPU identity and shader-limit preflight PASS (CPU only)");
    return 0;
}
