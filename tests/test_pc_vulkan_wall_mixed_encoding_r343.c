/* RE47 R343: mixed CRY16/RGB8 multi-pass Vulkan diagnostic contract.
 * CPU-side validation ONLY. GLSL compilation/GPU execution still required. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_slots.h"
#include "jaguar/avp_gpu/maze2_column.h"

static void put32(uint8_t *p,uint32_t v)
{
    p[0]=(uint8_t)v;
    p[1]=(uint8_t)(v>>8u);
    p[2]=(uint8_t)(v>>16u);
    p[3]=(uint8_t)(v>>24u);
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|
           ((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
static void test_mixed_wire_and_bounds(void)
{
    AvpWorldScenePacket p={0};
    AvpWorldRayColumn columns[2]={0};
    AvpWorldRayHit hits[2]={0};
    AvpVkWallProjectionStream v1={0};
    AvpVkWallSlotPlan plan={0};
    uint8_t maze[8]={0};
    uint8_t *r;
    uint32_t good_top,good_bottom,good_base,good_height;
    p.maze=maze;p.map_width=p.map_height=1u;
    p.width=2u;p.height=228u;p.columns=columns;p.hits=hits;
    p.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    p.hit_capacity_per_column=1u;
    p.wall_image[1u]=5u;p.wall_image[2u]=6u;
    p.image[5u]=(AvpWorldImageMeta){128u,128u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    p.image[6u]=(AvpWorldImageMeta){1254u,1254u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    for(unsigned x=0u;x<2u;x++){
        columns[x]=(AvpWorldRayColumn){3.75,1u,1u,0u};
        hits[x]=(AvpWorldRayHit){3.75,0.25,x+1u,0u,1u,1u,0,228};
    }
    assert(avp_vk_wall_projection_pack(&v1,&p));
    assert(avp_vk_wall_projection_validate(v1.data,v1.bytes));
    assert(avp_vk_wall_slots_collect(v1.data,v1.bytes,&plan));
    assert(plan.used_count==2u && plan.active_columns==2u);
    assert(plan.order[0]==5u && plan.order[1]==6u);
    assert(plan.slots[5u].encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    assert(plan.slots[6u].encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8);

    r=v1.data+AVP_VK_WALL_PROJECTION_HEADER_BYTES;
    good_top=get32(r+12u);good_bottom=get32(r+16u);
    good_base=get32(r+20u);good_height=get32(r+24u);
    assert(good_top>0u && good_bottom<p.height);
    /* Each field is individually legal yet the combination is not. */
    put32(r+12u,good_top-1u);
    assert(!avp_vk_wall_projection_validate(v1.data,v1.bytes));
    put32(r+12u,good_top);
    put32(r+16u,good_bottom+1u);
    assert(!avp_vk_wall_projection_validate(v1.data,v1.bytes));
    put32(r+16u,good_bottom);
    put32(r+20u,good_base+1u);
    assert(!avp_vk_wall_projection_validate(v1.data,v1.bytes));
    put32(r+20u,good_base);
    put32(r+24u,good_height-1u);
    assert(!avp_vk_wall_projection_validate(v1.data,v1.bytes));
    put32(r+24u,good_height);
    assert(avp_vk_wall_projection_validate(v1.data,v1.bytes));
    /* Both legacy v1 and v2 shaded-wire validators must reject it. */
    {
        uint8_t v2[AVP_VK_WALL_PROJECTION_HEADER_BYTES+
            2u*AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES]={0};
        memcpy(v2,v1.data,AVP_VK_WALL_PROJECTION_HEADER_BYTES);
        put32(v2+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
        put32(v2+16u,AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES);
        for(unsigned x=0u;x<2u;x++){
            memcpy(v2+32u+x*48u,v1.data+32u+x*44u,44u);
        }
        assert(avp_vk_wall_projection_validate(v2,sizeof v2));
        put32(v2+32u+12u,good_top-1u);
        assert(!avp_vk_wall_projection_validate(v2,sizeof v2));
    }
    avp_vk_wall_projection_destroy(&v1);
}
/* Until glslang is available, also lock down the shader's pass predicate
 * ordering. CPU-only textual checks DO NOT certify valid SPIR-V. */
static void test_shader_order(const char *file)
{
    FILE *fp=fopen(file,"rb");
    char text[32768];
    size_t n;
    const char *slot,*encoding,*active;
    assert(fp!=NULL);
    n=fread(text,1u,sizeof(text)-1u,fp);
    assert(!ferror(fp) && feof(fp));
    fclose(fp);
    text[n]='\0';
    /* R351 fixed reserved GLSL identifier `active`: it is now `wallState`.
     * Keep the genuine invalid-state -> per-slot -> format guard order. */
    active=strstr(text,"if(wallState!=1u)");
    slot=strstr(text,"if(avp_projection.words[base+1u]!=avp_params.wx) discard;");
    encoding=strstr(text,"if(avp_projection.words[base+10u]!=avp_params.encoding");
    assert(active && slot && encoding);
    assert(active<slot && slot<encoding);
    assert(strstr(text,"if(alpha!=255u)discard;")!=NULL);
}
int main(int argc,char **argv)
{
    assert(argc==2);
    test_mixed_wire_and_bounds();
    test_shader_order(argv[1]);
    puts("R343 mixed CRY/RGB slot ordering and clipped-wall wire bounds PASS (CPU/static only)");
    return 0;
}
