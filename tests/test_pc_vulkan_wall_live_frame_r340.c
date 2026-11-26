/* RE47 R340 original state / original ray packet -> transactional source
 * upload -> validated draw schedule. No Vulkan device, art or ROM required. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_live_frame.h"
#include "pc/pc_vulkan_wall_lighting.h"
#include "jaguar/avp_gpu/maze2_column.h"

typedef struct Mock {
    uint32_t acquired,released,live,fail_at;
    uint32_t slots[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t first_source_pixel[AVP_WORLD_SCENE_IMAGE_SLOTS];
} Mock;
static int take(void *opaque,uint32_t slot,const uint32_t *pixels,
    uint32_t width,uint32_t height,uint32_t encoding,size_t bytes,
    void **resource)
{
    Mock *m=(Mock *)opaque;
    uint32_t *tag;
    assert(m && pixels && resource && slot<AVP_WORLD_SCENE_IMAGE_SLOTS);
    assert((width==16u && height==16u && bytes==1024u) ||
           (width==128u && height==720u && bytes==128u*720u*4u));
    assert(encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8 ||
           encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    assert((encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8 ?
        ((pixels[0]>>16u)&255u) : ((pixels[0]>>24u)&255u))==255u);
    m->acquired++;
    m->first_source_pixel[slot]=pixels[0];
    *resource=NULL;
    if(m->acquired==m->fail_at)return 0;
    tag=(uint32_t *)malloc(sizeof(*tag));
    assert(tag);
    *tag=slot;
    *resource=tag;
    m->slots[slot]++;
    m->live++;
    return 1;
}
static void give(void *opaque,uint32_t slot,void *resource)
{
    Mock *m=(Mock *)opaque;
    uint32_t *tag=(uint32_t *)resource;
    assert(m && tag && *tag==slot && m->live);
    m->released++;
    m->live--;
    free(tag);
}
static void test_original_sources(void)
{
    const uint32_t width=32u,height=32u;
    uint8_t rgb_a[16u*16u*3u],rgb_b[16u*16u*3u];
    uint8_t alpha[16u*16u];
    uint8_t maze[8]={0};
    AvpWorldRayColumn columns[32]={0};
    AvpWorldRayHit hits[32]={0};
    AvpWorldScenePacket p={0};
    AvpAvpGpuState *state=calloc(1u,sizeof(*state));
    AvpWorldGpuStaging scratch={0};
    AvpVkWallLiveFrame frame={0};
    Mock gpu={0};
    assert(state);
    memset(rgb_a,0x56,sizeof(rgb_a));
    memset(rgb_b,0xae,sizeof(rgb_b));
    memset(alpha,255,sizeof(alpha));
    state->panel_tex[5]=(AvpGpuRgbImage){16,16,rgb_a,NULL,NULL};
    state->panel_tex[6]=(AvpGpuRgbImage){16,16,rgb_b,NULL,NULL};
    p.width=width;p.height=height;
    p.map_width=1u;p.map_height=1u;p.maze=maze;
    p.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    p.hit_capacity_per_column=1u;
    p.columns=columns;p.hits=hits;
    p.wall_image[1u]=5u;p.wall_image[2u]=6u;
    p.image[5]=(AvpWorldImageMeta){16,16,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    p.image[6]=(AvpWorldImageMeta){16,16,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    for(uint32_t x=0u;x<width;x++){
        columns[x]=(AvpWorldRayColumn){1.0,1u,1u,0u};
        hits[x]=(AvpWorldRayHit){1.0,(double)(x%16u)/16.0,
            x<16u?1u:2u,0u,1u,1u,0,(int32_t)height};
    }
    gpu.fail_at=2u;
    assert(!avp_vk_wall_live_frame_build(&p,state,1024u,2048u,
        &scratch,take,give,&gpu,&frame));
    assert(gpu.acquired==2u && gpu.released==1u && !gpu.live);
    assert(!frame.geometry.data && !frame.images.count && !frame.draws.count);
    gpu=(Mock){0};
    assert(avp_vk_wall_live_frame_build(&p,state,1024u,2048u,
        &scratch,take,give,&gpu,&frame));
    assert(frame.geometry.bytes==32u+32u*AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES &&
           frame.draws.count==2u && frame.draws.active_columns==32u &&
           frame.draws.draws[0].slot==5u && frame.draws.draws[1].slot==6u &&
           frame.images.count==2u && frame.images.uploaded_bytes==2048u &&
           gpu.live==2u && !scratch.valid);
    /* Refuse replacing in-flight images even with the same source packet. */
    assert(!avp_vk_wall_live_frame_build(&p,state,1024u,2048u,
        &scratch,take,give,&gpu,&frame));
    assert(gpu.acquired==2u && gpu.live==2u);
    assert(!avp_vk_wall_live_frame_dispose(&frame,NULL,&gpu));
    assert(frame.images.count==2u && gpu.live==2u);
    assert(avp_vk_wall_live_frame_dispose(&frame,give,&gpu));
    assert(!gpu.live && gpu.released==2u && !frame.geometry.data);
    /* The active Software-selected image must exactly match the ray metadata.
     * A wrong source or transparent pixel cannot become a GPU opaque draw. */
    state->panel_tex[6].alpha=alpha;
    alpha[0]=0u;
    gpu=(Mock){0};
    assert(!avp_vk_wall_live_frame_build(&p,state,1024u,2048u,
        &scratch,take,give,&gpu,&frame));
    assert(!gpu.acquired && !frame.geometry.data);
    alpha[0]=255u;
    state->panel_tex[6].width=17;
    assert(!avp_vk_wall_live_frame_build(&p,state,1024u,2048u,
        &scratch,take,give,&gpu,&frame));
    assert(!gpu.acquired && !frame.geometry.data);
    state->panel_tex[6].width=16;
    /* A non-solid portal cannot be represented by the opaque-only path. */
    hits[0].wall=24u; /* wall type 3 */
    assert(!avp_vk_wall_live_frame_build(&p,state,1024u,2048u,
        &scratch,take,give,&gpu,&frame));
    assert(!gpu.acquired && !frame.geometry.data);
    hits[0].wall=1u;
    /* Match the actual Software hi-res selection: valid optional host art
     * replaces only the one requested slot, while others stay original. */
    uint8_t hires_rgb[16u*16u*3u];
    AvpGpuRgbImage hires[AVP_WORLD_SCENE_PANELS]={0};
    int hires_enabled=1;
    memset(hires_rgb,0x19,sizeof(hires_rgb));
    hires[5]=(AvpGpuRgbImage){16,16,hires_rgb,NULL,NULL};
    state->host_world_tex=hires;
    state->host_world_tex_enabled=&hires_enabled;
    assert(avp_vk_wall_live_frame_build(&p,state,1024u,2048u,
        &scratch,take,give,&gpu,&frame));
    assert(gpu.first_source_pixel[5u]==0xff191919u);
    assert(gpu.first_source_pixel[6u]==0xffaeaeaeu);
    assert(avp_vk_wall_live_frame_dispose(&frame,give,&gpu));
    assert(!gpu.live);
    state->host_world_tex_enabled=NULL;
    state->host_world_tex=NULL;
    /* A one-slot CRY source still retains actual Jaguar CRY16 encoding. */
    uint16_t cry[16u*16u];
    for(uint32_t i=0;i<16u*16u;i++)cry[i]=(uint16_t)(i*127u);
    state->panel_tex[5].cry16=cry;
    p.image[5].encoding=AVP_WORLD_IMAGE_CRY16_ALPHA8;
    assert(avp_vk_wall_live_frame_build(&p,state,1024u,2048u,
        &scratch,take,give,&gpu,&frame));
    assert(frame.draws.draws[0].encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    assert(frame.draws.draws[1].encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(avp_vk_wall_live_frame_dispose(&frame,give,&gpu));
    assert(!gpu.live);
    avp_world_gpu_staging_dispose(&scratch);
    free(state);
}
/* R345: a high-resolution discrepancy is corrected by a v3 exact-row map
 * before GPU image acquisition. Original game row selection stays intact. */
static void test_hires_exact_rows_before_upload(void)
{
    const double distance=0.5615;
    const uint32_t wall_height=1080u,texture_height=720u,texture_width=128u;
    uint8_t maze[8]={0};
    AvpWorldRayColumn column={distance,1u,1u,0u};
    AvpWorldRayHit hit={distance,0.25,1u,0u,1u,1u,0,(int32_t)wall_height};
    AvpWorldScenePacket scene={0};
    AvpVkWallLiveFrame frame={0};
    AvpVkWallProjectionStream lit={0};
    AvpWorldGpuStaging scratch={0};
    AvpAvpGpuState *state=(AvpAvpGpuState *)calloc(1u,sizeof(*state));
    uint8_t *rgb=(uint8_t *)malloc((size_t)texture_width*texture_height*3u);
    Mock gpu={0};
    uint32_t mismatches=0u;
    assert(state && rgb);
    assert(avp_gpu_maze2_project_wall_height(distance,wall_height)==(int)wall_height);
    memset(rgb,0x61,(size_t)texture_width*texture_height*3u);
    state->panel_tex[5]=(AvpGpuRgbImage){(int)texture_width,
        (int)texture_height,rgb,NULL,NULL};
    scene.width=1u;scene.height=wall_height;
    scene.maze=maze;scene.map_width=scene.map_height=1u;
    scene.columns=&column;scene.hits=&hit;
    scene.hit_capacity_per_column=1u;
    scene.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    scene.wall_image[1u]=5u;
    scene.image[5u]=(AvpWorldImageMeta){texture_width,texture_height,
        AVP_WORLD_IMAGE_RGB8_ALPHA8};
    assert(avp_vk_wall_projection_pack_lit(&lit,&scene));
    assert(avp_vk_wall_projection_row_parity(lit.data,lit.bytes,&mismatches));
    assert(mismatches>0u);
    assert(avp_vk_wall_live_frame_build(&scene,state,
        (size_t)texture_width*texture_height*4u,
        (uint64_t)texture_width*texture_height*4u,
        &scratch,take,give,&gpu,&frame));
    assert(gpu.acquired==1u && gpu.released==0u && gpu.live==1u);
    assert(frame.images.count==1u && frame.draws.count==1u);
    assert(frame.geometry.data && frame.geometry.bytes>0u);
    assert(frame.geometry.data[4u]==AVP_VK_WALL_PROJECTION_EXACT_VERSION);
    assert(avp_vk_wall_projection_row_parity(frame.geometry.data,
                              frame.geometry.bytes,&mismatches));
    assert(mismatches==0u);
    assert(avp_vk_wall_live_frame_dispose(&frame,give,&gpu));
    assert(gpu.released==1u && gpu.live==0u);
    avp_vk_wall_projection_destroy(&lit);
    avp_world_gpu_staging_dispose(&scratch);
    free(rgb);free(state);
}
/* R350: the actual Vulkan WSI consumes this exact GPU-SDK-independent
 * scene/source preparation route; this test exercises no synthetic GPU
 * image uploads and no Software output framebuffer. */
static void test_r350_live_preparation(void)
{
    uint8_t maze[8]={0};
    uint8_t *rgb0=(uint8_t *)malloc(16u*128u*3u);
    uint8_t *rgb1=(uint8_t *)malloc(16u*720u*3u);
    uint16_t *cry=(uint16_t *)malloc(16u*128u*sizeof(uint16_t));
    AvpAvpGpuState *state=(AvpAvpGpuState *)calloc(1,sizeof(*state));
    AvpWorldRayColumn cols[2]={{0}};
    AvpWorldRayHit hits[2]={{0}};
    AvpWorldScenePacket scene={0};
    AvpVkWallProjectionStream wire={0};
    AvpVkWallSlotPlan slots={0};
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    uint32_t differences=UINT32_MAX;
    assert(rgb0 && rgb1 && cry && state);
    memset(rgb0,0x32,16u*128u*3u);
    memset(rgb1,0x65,16u*720u*3u);
    for(size_t i=0;i<16u*128u;i++)cry[i]=(uint16_t)(i*107u);
    state->panel_tex[5]=(AvpGpuRgbImage){16,128,rgb0,cry,NULL};
    state->panel_tex[6]=(AvpGpuRgbImage){16,720,rgb1,NULL,NULL};
    scene.width=2u;scene.height=480u;
    scene.map_width=scene.map_height=1u;scene.maze=maze;
    scene.columns=cols;scene.hits=hits;
    scene.hit_capacity_per_column=1u;
    scene.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    scene.wall_image[1]=5u;scene.wall_image[2]=6u;
    scene.image[5]=(AvpWorldImageMeta){16u,128u,
                                     AVP_WORLD_IMAGE_CRY16_ALPHA8};
    scene.image[6]=(AvpWorldImageMeta){16u,720u,
                                     AVP_WORLD_IMAGE_RGB8_ALPHA8};
    for(uint32_t i=0;i<2;i++){
        cols[i]=(AvpWorldRayColumn){0.5615,1u,1u,0u};
        hits[i]=(AvpWorldRayHit){0.5615,0.25,i?2u:1u,0u,1u,1u,0,480};
    }
    assert(avp_vk_wall_live_prepare(&scene,state,&wire,&slots,sources));
    assert(wire.data && avp_vk_wall_projection_validate(wire.data,wire.bytes));
    assert(slots.used_count==2u && slots.active_columns==2u &&
           slots.order[0]==5u && slots.order[1]==6u);
    assert(sources[5]==&state->panel_tex[5] &&
           sources[6]==&state->panel_tex[6] && !sources[7]);
    assert(avp_vk_wall_projection_row_parity(wire.data,wire.bytes,&differences));
    assert(differences==0u);
    /* In-flight wire ownership cannot be overwritten by a new frame. */
    assert(!avp_vk_wall_live_prepare(&scene,state,&wire,&slots,sources));
    avp_vk_wall_projection_destroy(&wire);
    /* A mismatched selected source must never produce a partial draw plan. */
    state->panel_tex[6].height=719;
    assert(!avp_vk_wall_live_prepare(&scene,state,&wire,&slots,sources));
    assert(!wire.data && !slots.used_count && !sources[5] && !sources[6]);
    state->panel_tex[6].height=720;
    /* An unsupported original door/window must fail without touching GPU. */
    hits[0].wall=24u;
    assert(!avp_vk_wall_live_prepare(&scene,state,&wire,&slots,sources));
    assert(!wire.data && !slots.used_count && !sources[5] && !sources[6]);
    hits[0].wall=1u;
    assert(avp_vk_wall_live_prepare(&scene,state,&wire,&slots,sources));
    avp_vk_wall_projection_destroy(&wire);
    free(rgb0);free(rgb1);free(cry);free(state);
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--prepare-only")){
        test_r350_live_preparation();
        puts("R350 actual ray packet/source preparation and fail-closed gate PASS");
        return 0;
    }
    assert(argc==1);
    test_original_sources();
    test_hires_exact_rows_before_upload();
    puts("R345 original state, exact high-res row map, transactional texture upload PASS");
    return 0;
}
