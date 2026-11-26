/* R344: exact high-res CPU stepping vs Vulkan shader row fraction; no GPU
 * runtime available here. This regression must catch a REAL discrepancy. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_projection.h"
#include "pc/pc_vulkan_wall_lighting.h"
#include "jaguar/avp_gpu/texture_filter.h"

static void put32(uint8_t *p,uint32_t v)
{
    for(unsigned i=0u;i<4u;i++)p[i]=(uint8_t)(v>>(8u*i));
}
static void test_row_mismatches(void)
{
    uint8_t wire[AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                 AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES]={0};
    uint8_t *r=wire+32u;
    uint32_t mismatches=UINT32_MAX;
    put32(wire+0u,AVP_VK_WALL_PROJECTION_MAGIC);
    put32(wire+4u,AVP_VK_WALL_PROJECTION_LIT_VERSION);
    put32(wire+8u,1u);put32(wire+12u,1080u);
    put32(wire+16u,AVP_VK_WALL_PROJECTION_LIT_COLUMN_BYTES);
    put32(wire+20u,AVP_VK_WALL_PROJECTION_HEADER_BYTES);
    put32(wire+24u,1u);
    put32(r,1u);put32(r+4u,5u);put32(r+8u,1u);
    put32(r+12u,0u);put32(r+16u,1080u);
    put32(r+20u,AVP_VK_WALL_PROJECTION_BASE_BIAS);
    put32(r+24u,1080u);put32(r+28u,0u);
    put32(r+32u,128u);put32(r+36u,720u);
    put32(r+40u,AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(avp_vk_wall_projection_validate(wire,sizeof(wire)));
    assert(avp_vk_wall_projection_row_parity(wire,sizeof(wire),&mismatches));
    /* Existing integer GLSL sourceY and Software's repeated double step
     * select different texels at real pixel positions: do not hide this. */
    assert(mismatches>0u);
    put32(r+36u,128u); /* retail source rows: integer-sampled */
    assert(avp_vk_wall_projection_row_parity(wire,sizeof(wire),&mismatches));
    assert(mismatches==0u);
    put32(r+36u,1254u);
    put32(r+24u,128u);put32(r+16u,128u);
    assert(avp_vk_wall_projection_row_parity(wire,sizeof(wire),&mismatches));
    assert(mismatches==0u);
    put32(r+24u,1080u);put32(r+16u,1080u);
    put32(r+12u,1081u);
    assert(!avp_vk_wall_projection_row_parity(wire,sizeof(wire),&mismatches));
    assert(!avp_vk_wall_projection_row_parity(wire,sizeof(wire),NULL));
}
static void test_filtered_scene_rejection(void)
{
    uint8_t maze[8]={0};
    AvpWorldRayColumn column={1.0,1u,1u,0u};
    AvpWorldRayHit hit={1.0,0.25,1u,0u,1u,1u,0,228};
    AvpWorldScenePacket scene={0};
    AvpVkWallProjectionStream lit={0};
    scene.maze=maze;scene.map_width=scene.map_height=1u;
    scene.width=1u;scene.height=228u;scene.columns=&column;scene.hits=&hit;
    scene.hit_capacity_per_column=1u;
    scene.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    scene.wall_image[1u]=5u;
    scene.image[5u]=(AvpWorldImageMeta){128u,128u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    assert(avp_vk_wall_projection_pack_lit(&lit,&scene));
    scene.filter_mask=1u<<AVP_FILTER_GEOMETRY;
    scene.filter_mode=AVP_TEXFILTER_BILINEAR;
    assert(!avp_vk_wall_projection_pack_lit(&lit,&scene) && !lit.bytes);
    scene.filter_mode=AVP_TEXFILTER_NEAREST_MIP;
    assert(!avp_vk_wall_projection_pack_lit(&lit,&scene));
    scene.filter_mode=AVP_TEXFILTER_NEAREST;
    scene.anisotropic_taps=4u;
    assert(!avp_vk_wall_projection_pack_lit(&lit,&scene));
    scene.filter_mask=1u<<AVP_FILTER_PROPS;
    assert(avp_vk_wall_projection_pack_lit(&lit,&scene));
    scene.filter_mask=1u<<AVP_FILTER_GEOMETRY;
    scene.anisotropic_taps=1u;
    assert(avp_vk_wall_projection_pack_lit(&lit,&scene));
    assert(avp_vk_wall_projection_validate(lit.data,lit.bytes));
    avp_vk_wall_projection_destroy(&lit);
}
int main(void)
{
    test_row_mismatches();
    test_filtered_scene_rejection();
    puts("R344 verified hi-res row discrepancy and filtered wall fail-closed PASS");
    return 0;
}
