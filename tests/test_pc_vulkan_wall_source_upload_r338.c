/* RE47 R338: actual source pixels, CRY/RGB, 92-slot preflight, fail closed. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_source_upload.h"

typedef struct TestUpload {
    unsigned count, fail_at;
    uint32_t seen[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint64_t hash;
} TestUpload;
static int accept(void *ctx,uint32_t slot,const uint32_t *pixels,
                  uint32_t width,uint32_t height,uint32_t encoding,size_t bytes)
{
    TestUpload *u=ctx;
    assert(slot<AVP_WORLD_SCENE_IMAGE_SLOTS && pixels &&
           width && height && bytes==(size_t)width*height*4u &&
           (encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8 ||
            encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8));
    ++u->count;
    if (u->fail_at && u->count==u->fail_at) return 0;
    assert(!u->seen[slot]);
    u->seen[slot]=1;
    u->hash+=(uint64_t)pixels[0]+pixels[width*height-1u]+slot;
    return 1;
}
static void make_plan(AvpVkWallSlotPlan *p,unsigned count,
                      uint32_t width,uint32_t height,uint32_t encoding)
{
    memset(p,0,sizeof(*p));
    p->used_count=count;
    p->active_columns=count;
    for (unsigned i=0;i<count;i++) {
        p->order[i]=i;
        p->slots[i]=(AvpVkWallSlot){1u,1u,width,height,encoding};
    }
}
static void test_rgb_92_and_limits(void)
{
    const unsigned w=16u,h=16u;
    const size_t per_bytes=(size_t)w*h*4u;
    uint8_t rgb[w*h*3u], alpha[w*h];
    AvpGpuRgbImage source={16,16,rgb,NULL,alpha};
    const AvpGpuRgbImage *images[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    AvpVkWallSlotPlan plan;
    AvpWorldGpuStaging staging={0};
    AvpVkWallUploadStats stats={0};
    TestUpload call={0};
    for(unsigned i=0;i<w*h;i++) {
        rgb[3u*i]=(uint8_t)(i&255u);rgb[3u*i+1u]=3u;
        rgb[3u*i+2u]=4u;alpha[i]=255u;
    }
    for(unsigned i=0;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)images[i]=&source;
    make_plan(&plan,92u,w,h,AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(avp_vk_wall_source_upload(&plan,images,per_bytes,92u*per_bytes,
                                    &staging,accept,&call,&stats));
    assert(call.count==92u && stats.slots_uploaded==92u &&
           stats.source_bytes_uploaded==92u*per_bytes);
    for(unsigned i=0;i<92u;i++)assert(call.seen[i]);
    assert(!staging.valid && staging.word_capacity==w*h); /* reused */
    assert(call.hash>0u);

    /* Source bytes are not overwritten with an earlier successful frame. */
    call=(TestUpload){0};
    alpha[7]=0;
    assert(!avp_vk_wall_source_upload(&plan,images,per_bytes,92u*per_bytes,
                                     &staging,accept,&call,&stats));
    assert(call.count==0u && stats.slots_uploaded==0u && !staging.valid);
    alpha[7]=255u;
    plan.slots[91].height=15u;
    assert(!avp_vk_wall_source_upload(&plan,images,per_bytes,92u*per_bytes,
                                     &staging,accept,&call,&stats));
    assert(call.count==0u);
    plan.slots[91].height=h;
    assert(!avp_vk_wall_source_upload(&plan,images,per_bytes-1u,92u*per_bytes,
                                     &staging,accept,&call,&stats));
    assert(!avp_vk_wall_source_upload(&plan,images,per_bytes,92u*per_bytes-1u,
                                     &staging,accept,&call,&stats));
    assert(call.count==0u);
    plan.order[91u]=1u;
    assert(!avp_vk_wall_source_upload(&plan,images,per_bytes,92u*per_bytes,
                                     &staging,accept,&call,&stats));
    plan.order[91u]=91u;
    call=(TestUpload){.fail_at=2u};
    assert(!avp_vk_wall_source_upload(&plan,images,per_bytes,92u*per_bytes,
                                     &staging,accept,&call,&stats));
    assert(call.count==2u && !stats.slots_uploaded && !staging.valid);
    avp_world_gpu_staging_dispose(&staging);
}
static void test_crystal_and_hires(void)
{
    /* The existing staging contract requires an RGB shadow pointer even
     * when CRY16 source is authoritative. Exercise both representations. */
    const unsigned w=1254u,h=1254u;
    const size_t count=(size_t)w*h,bytes=count*4u;
    uint8_t *rgb=calloc(count,3u),*alpha=malloc(count);
    uint16_t *cry=malloc(count*sizeof(*cry));
    AvpGpuRgbImage source={1254,1254,rgb,cry,alpha};
    const AvpGpuRgbImage *images[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    AvpVkWallSlotPlan plan;
    AvpWorldGpuStaging scratch={0};
    AvpVkWallUploadStats stats;
    TestUpload call={0};
    assert(rgb && cry && alpha);
    for(size_t i=0;i<count;i++)cry[i]=(uint16_t)((i*7u)&65535u);
    memset(alpha,255u,count);
    make_plan(&plan,1u,w,h,AVP_WORLD_IMAGE_CRY16_ALPHA8);
    images[0]=&source;
    assert(avp_vk_wall_source_upload(&plan,images,bytes,bytes,&scratch,
                                    accept,&call,&stats));
    assert(stats.slots_uploaded==1u && call.count==1u &&
           stats.source_bytes_uploaded==bytes);
    /* Real alpha holes and RGB/CRY mismatches require unsupported paths. */
    alpha[count-1u]=254u;
    call=(TestUpload){0};
    assert(!avp_vk_wall_source_upload(&plan,images,bytes,bytes,&scratch,
                                     accept,&call,&stats));
    assert(!call.count);
    alpha[count-1u]=255u;
    source.cry16=NULL;
    assert(!avp_vk_wall_source_upload(&plan,images,bytes,bytes,&scratch,
                                     accept,&call,&stats));
    assert(!call.count);
    plan.slots[0].encoding=AVP_WORLD_IMAGE_RGB8_ALPHA8;
    assert(avp_vk_wall_source_upload(&plan,images,bytes,bytes,&scratch,
                                    accept,&call,&stats));
    assert(call.count==1u);
    /* A truly empty wall frame must not attempt stale source uploads. */
    make_plan(&plan,0u,0u,0u,0u);
    call=(TestUpload){0};
    assert(avp_vk_wall_source_upload(&plan,images,bytes,bytes,&scratch,
                                    accept,&call,&stats));
    assert(!call.count && !stats.slots_uploaded && !stats.source_bytes_uploaded);
    avp_world_gpu_staging_dispose(&scratch);
    free(rgb);free(cry);free(alpha);
}
static void test_source_derived_wall_plan(void)
{
    /* Exercise the actual R336 ray->wire->R337 slot->R338 image path,
     * without a ROM, a fake hardware backend, or a Software framebuffer. */
    const unsigned width=64u,height=228u,edge=16u;
    const size_t bytes=(size_t)edge*edge*4u;
    uint8_t rgb_a[16u*16u*3u]={0},rgb_b[16u*16u*3u]={0};
    AvpGpuRgbImage a={16,16,rgb_a,NULL,NULL},b={16,16,rgb_b,NULL,NULL};
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    AvpWorldScenePacket scene={0};
    AvpVkWallProjectionStream wire={0};
    AvpVkWallSlotPlan plan;
    AvpWorldGpuStaging scratch={0};
    AvpVkWallUploadStats stats;
    TestUpload call={0};
    memset(rgb_a,0x35,sizeof(rgb_a));
    memset(rgb_b,0xb6,sizeof(rgb_b));
    sources[5]=&a;sources[6]=&b;
    scene.width=width;scene.height=height;
    scene.map_width=scene.map_height=1u;
    scene.hit_capacity_per_column=1u;
    scene.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    scene.maze=calloc(8u,1u);
    scene.columns=calloc(width,sizeof(*scene.columns));
    scene.hits=calloc(width,sizeof(*scene.hits));
    assert(scene.maze && scene.columns && scene.hits);
    scene.wall_image[1u]=5u;scene.wall_image[2u]=6u;
    scene.image[5]=(AvpWorldImageMeta){16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    scene.image[6]=(AvpWorldImageMeta){16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    for(unsigned x=0u;x<width;x++) {
        scene.columns[x]=(AvpWorldRayColumn){1.0,1u,1u,0u};
        scene.hits[x]=(AvpWorldRayHit){1.0,(double)(x%16u)/16.0,
                     ((x/16u)&1u)?2u:1u,0u,1u,1u,0,(int32_t)height};
    }
    assert(avp_vk_wall_projection_pack(&wire,&scene));
    assert(avp_vk_wall_slots_collect(wire.data,wire.bytes,&plan));
    assert(plan.used_count==2u && plan.order[0]==5u && plan.order[1]==6u);
    assert(avp_vk_wall_source_upload(&plan,sources,bytes,2u*bytes,
                                    &scratch,accept,&call,&stats));
    assert(call.count==2u && call.seen[5] && call.seen[6] &&
           stats.source_bytes_uploaded==2u*bytes);
    /* A damaged source cannot dispatch even the FIRST provisional GPU
     * upload, despite both columns remaining source-derived and valid. */
    b.height=15;
    call=(TestUpload){0};
    assert(!avp_vk_wall_source_upload(&plan,sources,bytes,2u*bytes,
                                     &scratch,accept,&call,&stats));
    assert(!call.count && !stats.slots_uploaded && !scratch.valid);
    avp_vk_wall_projection_destroy(&wire);
    avp_world_gpu_staging_dispose(&scratch);
    free(scene.maze);free(scene.columns);free(scene.hits);
}
int main(void)
{
    test_rgb_92_and_limits();
    test_crystal_and_hires();
    test_source_derived_wall_plan();
    puts("R338 original-source opaque RGB/CRY staging, 92 slots, budgets and rollback PASS");
    return 0;
}
