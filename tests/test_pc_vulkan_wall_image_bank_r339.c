/* RE47 R339: mocked resource owners, ORIGINAL pixel staging, no mock GPU.
 * Proves upload transactions cannot publish partial per-slot resources. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_image_bank.h"

typedef struct FakeGpu {
    uint32_t creations,destructions,fail_at,partial_at,duplicate_at;
    uint32_t last_enc,seen[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t live;
    void *first;
} FakeGpu;
static int fake_create(void *user,uint32_t slot,const uint32_t *words,
    uint32_t w,uint32_t h,uint32_t encoding,size_t bytes,void **resource)
{
    FakeGpu *g=user;
    unsigned *token;
    assert(g && resource && words && w && h &&
           bytes==(size_t)w*h*4u && slot<AVP_WORLD_SCENE_IMAGE_SLOTS);
    assert(encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8 ||
           encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    assert((words[0] & 0xff000000u)==0xff000000u ||
           encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    *resource=NULL;
    g->creations++;
    if (g->creations==g->duplicate_at) {
        *resource=g->first;
        return 1;
    }
    if (g->creations==g->fail_at)return 0;
    token=malloc(sizeof(*token));
    assert(token);
    *token=slot+1u;
    *resource=token;
    if (!g->first)g->first=token;
    g->live++;
    g->seen[slot]++;
    g->last_enc=encoding;
    if (g->creations==g->partial_at)return 0;
    return 1;
}
static void fake_release(void *user,uint32_t slot,void *resource)
{
    FakeGpu *g=user;
    assert(g && resource && slot<AVP_WORLD_SCENE_IMAGE_SLOTS);
    assert(*(unsigned *)resource==slot+1u);
    assert(g->live>0u);
    g->live--;
    g->destructions++;
    if (g->first==resource)g->first=NULL;
    free(resource);
}
static void make_plan(AvpVkWallSlotPlan *plan,uint32_t slots,
    uint32_t width,uint32_t height,uint32_t enc)
{
    memset(plan,0,sizeof(*plan));
    plan->used_count=slots;
    plan->active_columns=slots;
    for(uint32_t i=0u;i<slots;i++) {
        plan->order[i]=i;
        plan->slots[i]=(AvpVkWallSlot){1u,1u,width,height,enc};
    }
}
static void test_atomicity(void)
{
    uint8_t rgb[8u*8u*3u],alpha[8u*8u];
    AvpGpuRgbImage img={8,8,rgb,NULL,alpha};
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    AvpVkWallSlotPlan plan;
    AvpVkWallImageBank bank={0};
    AvpWorldGpuStaging scratch={0};
    FakeGpu gpu={0};
    memset(rgb,0x67,sizeof(rgb));
    memset(alpha,255,sizeof(alpha));
    for (uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)sources[i]=&img;
    make_plan(&plan,3u,8u,8u,AVP_WORLD_IMAGE_RGB8_ALPHA8);
    gpu.fail_at=2u;
    assert(!avp_vk_wall_image_bank_build(&plan,sources,256u,768u,&scratch,
        fake_create,fake_release,&gpu,&bank));
    assert(gpu.creations==2u && gpu.destructions==1u && gpu.live==0u);
    assert(!bank.count && !bank.slots[0].resource && !scratch.valid);
    gpu=(FakeGpu){.partial_at=2u};
    assert(!avp_vk_wall_image_bank_build(&plan,sources,256u,768u,&scratch,
        fake_create,fake_release,&gpu,&bank));
    assert(gpu.creations==2u && gpu.destructions==2u && !gpu.live);
    gpu=(FakeGpu){.duplicate_at=2u};
    assert(!avp_vk_wall_image_bank_build(&plan,sources,256u,768u,&scratch,
        fake_create,fake_release,&gpu,&bank));
    assert(gpu.creations==2u && gpu.destructions==1u && !gpu.live);
    gpu=(FakeGpu){0};
    alpha[0]=0u; /* preflight refuses ANY callbacks */
    assert(!avp_vk_wall_image_bank_build(&plan,sources,256u,768u,&scratch,
        fake_create,fake_release,&gpu,&bank));
    assert(!gpu.creations);
    alpha[0]=255u;
    assert(avp_vk_wall_image_bank_build(&plan,sources,256u,768u,&scratch,
        fake_create,fake_release,&gpu,&bank));
    assert(bank.count==3u && bank.uploaded_bytes==768u && gpu.live==3u);
    for(uint32_t i=0u;i<3u;i++) {
        assert(bank.order[i]==i && bank.slots[i].resource &&
               bank.slots[i].width==8u && bank.slots[i].height==8u);
    }
    /* Do not silently replace a live bank, even if a fresh image is valid. */
    assert(!avp_vk_wall_image_bank_build(&plan,sources,256u,768u,&scratch,
        fake_create,fake_release,&gpu,&bank));
    assert(gpu.creations==3u && gpu.live==3u && bank.count==3u);
    avp_vk_wall_image_bank_dispose(&bank,NULL,&gpu);
    assert(gpu.live==3u && bank.count==3u); /* cannot orphan handles */
    avp_vk_wall_image_bank_dispose(&bank,fake_release,&gpu);
    assert(!gpu.live && gpu.destructions==3u && !bank.count);
    avp_vk_wall_image_bank_dispose(&bank,fake_release,&gpu);
    assert(gpu.destructions==3u);
    make_plan(&plan,0u,8u,8u,AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(avp_vk_wall_image_bank_build(&plan,sources,256u,768u,&scratch,
        fake_create,fake_release,&gpu,&bank));
    assert(!bank.count && !bank.uploaded_bytes && gpu.creations==3u);
    avp_vk_wall_image_bank_dispose(&bank,fake_release,&gpu);
    avp_world_gpu_staging_dispose(&scratch);
}
static void test_all_slots_and_cry(void)
{
    uint8_t rgb[16u*16u*3u]={0};
    uint16_t cry[16u*16u];
    AvpGpuRgbImage img={16,16,rgb,cry,NULL};
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    AvpVkWallSlotPlan plan;
    AvpVkWallImageBank bank={0};
    AvpWorldGpuStaging scratch={0};
    FakeGpu gpu={0};
    for(uint32_t i=0u;i<16u*16u;i++)cry[i]=(uint16_t)i;
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)sources[i]=&img;
    make_plan(&plan,AVP_WORLD_SCENE_IMAGE_SLOTS,16u,16u,
              AVP_WORLD_IMAGE_CRY16_ALPHA8);
    assert(avp_vk_wall_image_bank_build(&plan,sources,1024u,
           AVP_WORLD_SCENE_IMAGE_SLOTS*1024u,&scratch,
           fake_create,fake_release,&gpu,&bank));
    assert(bank.count==AVP_WORLD_SCENE_IMAGE_SLOTS &&
           bank.uploaded_bytes==AVP_WORLD_SCENE_IMAGE_SLOTS*1024u &&
           gpu.live==AVP_WORLD_SCENE_IMAGE_SLOTS &&
           gpu.last_enc==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        assert(gpu.seen[i]==1u && bank.slots[i].resource);
    avp_vk_wall_image_bank_dispose(&bank,fake_release,&gpu);
    assert(gpu.destructions==AVP_WORLD_SCENE_IMAGE_SLOTS && !gpu.live);
    avp_world_gpu_staging_dispose(&scratch);
}
int main(void)
{
    test_atomicity();
    test_all_slots_and_cry();
    puts("R339 atomic 92-slot image ownership, preflight and upload rollback PASS");
    return 0;
}
