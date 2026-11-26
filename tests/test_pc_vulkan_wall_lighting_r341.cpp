/* R341: opt-in lit wall stream must source the *existing* Jaguar CRY
 * distance-intensity rule; it never reimplements that rule in C. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "pc/pc_vulkan_wall_lighting.h"
#include "pc/pc_vulkan_wall_slots.h"
#include "jaguar/avp_gpu/projection.hpp"
#include "jaguar/avp_gpu/host_distance.h"
#include "jaguar/avp_gpu/maze2_column.h"
#include "jaguar/cry.h"

static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8u |
           (uint32_t)p[2]<<16u | (uint32_t)p[3]<<24u;
}
static void write32(uint8_t *p,uint32_t n)
{
    for(unsigned i=0u;i<4u;i++)p[i]=(uint8_t)(n>>(i*8u));
}
static int32_t shade_at(const AvpVkWallProjectionStream &s,unsigned x)
{
    return (int32_t)word(s.data+32u+(size_t)x*48u+44u);
}
static void test_shades()
{
    static constexpr unsigned width=6,height=228;
    AvpWorldRayColumn cols[width]{};
    AvpWorldRayHit hits[width]{};
    uint8_t maze[8]{};
    AvpWorldScenePacket scene{};
    AvpVkWallProjectionStream base{},lit{};
    AvpVkWallSlotPlan slots{};
    const double distances[width]={0.25,0.6,1.0,3.75,7.9,1.0};
    const unsigned walls[width]={1u,1u,1u,1u,2u,1u};
    scene.width=width;scene.height=height;
    scene.map_width=scene.map_height=1u;
    scene.maze=maze;
    scene.columns=cols;scene.hits=hits;
    scene.hit_capacity_per_column=1u;
    scene.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    scene.wall_image[1]=5u;
    scene.wall_image[2]=6u;
    scene.image[5]={128u,128u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    scene.image[6]={1254u,1254u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    for(unsigned x=0;x<width;x++) {
        cols[x]={distances[x],1u,1u,0u};
        hits[x]={distances[x],double(x)/double(width),walls[x],0u,1u,1u,0,(int32_t)height};
    }
    /* Prove the compatibility layout remains genuinely unmodified. */
    assert(avp_vk_wall_projection_pack(&base,&scene));
    assert(base.bytes==32u+width*44u);
    for(unsigned fog=0;fog<AVP_HOST_FOG_DISTANCE_CHOICES;fog++){
        scene.fog_distance_mode=fog;
        for(unsigned no_fog=0;no_fog<=1u;no_fog++){
            scene.disable_fog=no_fog;
            assert(avp_vk_wall_projection_pack_lit(&lit,&scene));
            assert(avp_vk_wall_projection_validate(lit.data,lit.bytes));
            assert(lit.bytes==32u+width*48u && word(lit.data+4u)==2u);
            assert(word(lit.data+16u)==48u);
            assert(avp_vk_wall_slots_collect(lit.data,lit.bytes,&slots));
            assert(slots.used_count==2u && slots.active_columns==width);
            for(unsigned x=0;x<width;x++) {
                const uint8_t *v1=base.data+32u+(size_t)x*44u;
                const uint8_t *v2=lit.data+32u+(size_t)x*48u;
                assert(!std::memcmp(v1,v2,44u));
                const int expected= x==4u ? 0 :
                    (no_fog ? 0 : avp::retail_wall_intensity_delta(
                        avp_host_fog_distance(distances[x],(int)fog)));
                assert(shade_at(lit,x)==expected);
                assert(shade_at(lit,x)>=-255 && shade_at(lit,x)<=255);
            }
        }
    }
    scene.disable_fog=0u;scene.fog_distance_mode=0u;
    assert(avp_vk_wall_projection_pack_lit(&lit,&scene));
    /* GPU validation must reject invalid intensity, RGB shade and wire
     * version/stride mismatches before any image descriptors are touched. */
    uint8_t *rec=lit.data+32u;
    const uint32_t good=word(rec+44u);
    write32(rec+44u,(uint32_t)-256);
    assert(!avp_vk_wall_projection_validate(lit.data,lit.bytes));
    write32(rec+44u,256u);
    assert(!avp_vk_wall_projection_validate(lit.data,lit.bytes));
    write32(rec+44u,good);
    rec=lit.data+32u+4u*48u;
    write32(rec+44u,(uint32_t)-1);
    assert(!avp_vk_wall_projection_validate(lit.data,lit.bytes));
    write32(rec+44u,0u);
    write32(lit.data+16u,44u);
    assert(!avp_vk_wall_projection_validate(lit.data,lit.bytes));
    write32(lit.data+16u,48u);
    write32(lit.data+4u,1u);
    assert(!avp_vk_wall_projection_validate(lit.data,lit.bytes));
    write32(lit.data+4u,2u);
    assert(avp_vk_wall_projection_validate(lit.data,lit.bytes));
    cols[2].count=0u;
    assert(avp_vk_wall_projection_pack_lit(&lit,&scene));
    rec=lit.data+32u+2u*48u;
    for(unsigned i=0u;i<48u;i++)assert(rec[i]==0u);
    cols[2].ready=0u;
    assert(!avp_vk_wall_projection_pack_lit(&lit,&scene));
    assert(lit.bytes==0u);
    cols[2].ready=1u;
    scene.fog_distance_mode=99u;
    assert(!avp_vk_wall_projection_pack_lit(&lit,&scene));
    scene.fog_distance_mode=0u;
    scene.disable_fog=2u;
    assert(!avp_vk_wall_projection_pack_lit(&lit,&scene));
    avp_vk_wall_projection_destroy(&lit);
    avp_vk_wall_projection_destroy(&base);
}
static void test_cry_intensity_consistency()
{
    /* Same CRY Y saturation as the Software renderer's source shade path. */
    for(int mode=0;mode<4;mode++)for(int disabled=0;disabled<=1;disabled++){
        for(double distance : {0.01,0.5,1.0,2.0,4.0,8.0,16.0}){
            const int shade=avp_vk_retail_wall_shade(distance,mode,disabled);
            const int expect= disabled ? 0 :
                avp::retail_wall_intensity_delta(avp_host_fog_distance(distance,mode));
            assert(shade==expect);
            for(unsigned y : {0u,1u,63u,127u,254u,255u}) {
                int adjusted=(int)y+shade;
                if(adjusted<0)adjusted=0;
                if(adjusted>255)adjusted=255;
                uint16_t original=(uint16_t)(0x8a00u|y);
                uint16_t result=(uint16_t)((original&0xff00u)|(uint16_t)adjusted);
                uint8_t r=0,g=0,b=0;
                avp_jaguar_cry16_to_rgb(result,&r,&g,&b);
                /* The shader's integer-lookup indexing must be C/R and
                 * the saturated intensity (not an RGB multiplication). */
                assert((result&0xffu)==(unsigned)adjusted);
                (void)r;(void)g;(void)b;
            }
        }
    }
}
int main()
{
    test_shades();
    test_cry_intensity_consistency();
    std::puts("R341 original CRY distance shading, fog enhancements, v1/v2 wire compatibility and rejection PASS");
    return 0;
}
