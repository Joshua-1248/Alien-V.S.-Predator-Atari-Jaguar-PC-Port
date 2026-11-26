/* R337: per-texture Vulkan draw plan + two distinct image parity fixtures. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pc/pc_vulkan_wall_slots.h"
#include "jaguar/avp_gpu/maze2_column.h"
static void le32(uint8_t *p,uint32_t v)
{
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
static void plan_check(unsigned width,unsigned height)
{
    AvpWorldScenePacket sc={0};
    AvpVkWallProjectionStream stream={0};
    AvpVkWallSlotPlan plan;
    uint32_t a=0,b=0;
    sc.width=width;sc.height=height;
    sc.map_width=sc.map_height=1u;sc.hit_capacity_per_column=1u;
    sc.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    sc.maze=calloc(8u,1u);
    sc.hits=calloc(width,sizeof(*sc.hits));
    sc.columns=calloc(width,sizeof(*sc.columns));
    assert(sc.maze && sc.hits && sc.columns);
    sc.wall_image[1u]=5u;sc.wall_image[2u]=6u;
    sc.image[5]=(AvpWorldImageMeta){16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    sc.image[6]=(AvpWorldImageMeta){16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    for(uint32_t x=0;x<width;x++) {
        const uint32_t wall=((x/16u)&1u)?2u:1u;
        sc.columns[x]=(AvpWorldRayColumn){1.0,1u,1u,0u};
        sc.hits[x]=(AvpWorldRayHit){1.0,(double)(x%16u)/16.0,
                            wall,0u,1u,1u,0,(int32_t)height};
    }
    assert(avp_vk_wall_projection_pack(&stream,&sc));
    assert(avp_vk_wall_slots_collect(stream.data,stream.bytes,&plan));
    assert(plan.used_count==2u && plan.order[0]==5u && plan.order[1]==6u);
    for(uint32_t x=0;x<width;x++)
        if((x/16u)&1u)b++;else a++;
    assert(plan.active_columns==width);
    assert(plan.slots[5].columns==a && plan.slots[6].columns==b);
    assert(plan.slots[5].encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(plan.slots[6].width==16u && plan.slots[6].height==16u);
    assert(plan.slots[7].used==0u);
    /* Worst-case descriptor plan: every one of the 74 panel and 18 sign
     * slots can be represented without truncation, aliasing or reordering.
     * This is a wire-contract stress case, not a genuine Jaguar maze. */
    if(width>=AVP_WORLD_SCENE_IMAGE_SLOTS){
        for(uint32_t x=0;x<AVP_WORLD_SCENE_IMAGE_SLOTS;x++)
            le32(stream.data+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                 (size_t)x*AVP_VK_WALL_PROJECTION_COLUMN_BYTES+4u,x);
        assert(avp_vk_wall_projection_validate(stream.data,stream.bytes));
        assert(avp_vk_wall_slots_collect(stream.data,stream.bytes,&plan));
        assert(plan.used_count==AVP_WORLD_SCENE_IMAGE_SLOTS);
        for(uint32_t x=0;x<AVP_WORLD_SCENE_IMAGE_SLOTS;x++)
            assert(plan.order[x]==x && plan.slots[x].used);
        for(uint32_t x=0;x<AVP_WORLD_SCENE_IMAGE_SLOTS;x++)
            le32(stream.data+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                 (size_t)x*AVP_VK_WALL_PROJECTION_COLUMN_BYTES+4u,
                 ((x/16u)&1u)?6u:5u);
        assert(avp_vk_wall_slots_collect(stream.data,stream.bytes,&plan));
        assert(plan.used_count==2u);
    }
    /* Reject inconsistent source metadata for the SAME slot even though
     * each column individually satisfies R336's original wire validator. */
    if(width>=32u){
        uint8_t *rec=stream.data+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                         2u*AVP_VK_WALL_PROJECTION_COLUMN_BYTES;
        le32(rec+32u,32u);
        assert(avp_vk_wall_projection_validate(stream.data,stream.bytes));
        assert(!avp_vk_wall_slots_collect(stream.data,stream.bytes,&plan));
        assert(plan.used_count==0u && plan.active_columns==0u);
        le32(rec+32u,16u);
    }
    assert(avp_vk_wall_slots_collect(stream.data,stream.bytes,&plan));
    /* Tampering with source slot beyond the 92 available slots is rejected. */
    le32(stream.data+AVP_VK_WALL_PROJECTION_HEADER_BYTES+4u,92u);
    assert(!avp_vk_wall_slots_collect(stream.data,stream.bytes,&plan));
    assert(plan.used_count==0u);
    avp_vk_wall_projection_destroy(&stream);
    free(sc.maze);free(sc.hits);free(sc.columns);
}
static void readback_check(unsigned width,unsigned height,uint32_t format)
{
    size_t bytes=(size_t)width*height*4u;
    uint8_t *pixels=malloc(bytes);
    uint32_t mismatches=UINT32_MAX;
    int y0=0,y1=0;
    int ph=avp_gpu_maze2_project_wall_height(1.0,height);
    assert(pixels && avp_gpu_maze2_centered_wall_bounds(height,ph,&y0,&y1));
    ph=y1-y0;
    for(uint32_t y=0;y<height;y++)for(uint32_t x=0;x<width;x++) {
        uint32_t color=0u;
        if((int)y>=y0 && (int)y<y1){
            int tx=(int)(x%16u);
            int ty=avp_gpu_maze2_host_source_row((int)y-y0,ph,0)*16/128;
            const int parity=(tx^ty)&1;
            color=((x/16u)&1u)?
                (parity?0x00226644u:0x00ddaa66u):
                (parity?0x00336699u:0x00bb8855u);
        }
        size_t off=((size_t)y*width+x)*4u;
        pixels[off]=(uint8_t)(color>>(format==44u?0u:16u));
        pixels[off+1u]=(uint8_t)(color>>8u);
        pixels[off+2u]=(uint8_t)(color>>(format==44u?16u:0u));
        pixels[off+3u]=255u;
    }
    assert(avp_vk_wall_projection_check_two_slots(pixels,bytes,width,height,format,&mismatches));
    assert(mismatches==0u);
    pixels[((size_t)height/2u*width+17u)*4u]^=3u;
    assert(!avp_vk_wall_projection_check_two_slots(pixels,bytes,width,height,format,&mismatches));
    assert(mismatches==1u);
    /* Failure mode specific to the new second pass: an absent or incorrectly
     * bound slot-6 texture must not pass even when slot 5 rendered properly. */
    pixels[((size_t)height/2u*width+17u)*4u]=0u;
    assert(!avp_vk_wall_projection_check_two_slots(pixels,bytes,width,height,format,&mismatches));
    assert(mismatches>=1u);
    assert(!avp_vk_wall_projection_check_two_slots(pixels,bytes-1u,width,height,format,&mismatches));
    free(pixels);
}
int main(void)
{
    plan_check(320u,228u);plan_check(853u,480u);
    plan_check(1920u,1080u);
    readback_check(319u,257u,37u);readback_check(319u,257u,44u);
    readback_check(640u,480u,37u);readback_check(640u,480u,44u);
    puts("R337 two-slot wall stream planning and RGBA/BGRA parity oracles PASS");
    return 0;
}
