/* R348: a real source->wire->GPU-input test for two DIFFERENT encodings.
 * All source texels are original test patterns, not cartridge assets. */
#include "pc/pc_vulkan_wall_mixed_fixture.h"
#include "pc/pc_vulkan_wall_lighting.h"
#include "pc/pc_vulkan_wall_slots.h"
#include "jaguar/avp_gpu/maze2_column.h"
#include "jaguar/avp_gpu/world_texture_transfer.h"
#include <stdlib.h>
#include <string.h>

void avp_vk_wall_mixed_fixture_dispose(AvpVkWallMixedFixture *f)
{
    if(!f)return;
    avp_vk_wall_projection_destroy(&f->geometry);
    free(f->rgb);free(f->cry);free(f->cry_lut);
    memset(f,0,sizeof(*f));
}
int avp_vk_wall_mixed_fixture_build(uint32_t width,uint32_t height,
                                   AvpVkWallMixedFixture *out)
{
    AvpVkWallMixedFixture candidate={0};
    AvpWorldScenePacket scene={0};
    AvpVkWallSlotPlan plan={0};
    uint8_t maze[8]={0};
    uint32_t differences=UINT32_MAX;
    int ok=0;
    if(!out || out->geometry.data || out->geometry.bytes ||
       out->geometry.capacity || out->rgb || out->cry || out->cry_lut ||
       out->mapped_columns || out->mapped_rows ||
       width<32u || width>1024u || height<240u || height>1024u)return 0;
    for(uint32_t i=0;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        if(out->sources[i])return 0;
    /* The inverse of the original projection, chosen to produce an
     * unclipped 122-line wall at the usual 480-line diagnostic viewport.
     * 720/122 is a non-exact double DDA, exercising v3 row maps. */
    const double projected_target=122.0;
    const double distance=128.0*(double)height/(228.0*(projected_target-0.5));
    const int projected=avp_gpu_maze2_project_wall_height(distance,height);
    if(projected<=0 || projected>32768)return 0;
    candidate.rgb=malloc(AVP_VK_MIXED_TEX_WIDTH*AVP_VK_MIXED_RGB_HEIGHT*3u);
    candidate.cry=malloc(AVP_VK_MIXED_TEX_WIDTH*AVP_VK_MIXED_CRY_HEIGHT*sizeof(uint16_t));
    candidate.cry_lut=malloc(65536u*sizeof(uint32_t));
    if(!candidate.rgb || !candidate.cry || !candidate.cry_lut ||
       !avp_world_gpu_build_cry_lut(candidate.cry_lut,65536u))goto done;
    for(uint32_t y=0;y<AVP_VK_MIXED_RGB_HEIGHT;y++)
        for(uint32_t x=0;x<AVP_VK_MIXED_TEX_WIDTH;x++){
            size_t i=((size_t)y*AVP_VK_MIXED_TEX_WIDTH+x)*3u;
            candidate.rgb[i]=(uint8_t)(0x32u+(x*5u+y)%180u);
            candidate.rgb[i+1u]=(uint8_t)(0x45u+(y*3u)%160u);
            candidate.rgb[i+2u]=(uint8_t)(0x50u+(x*7u+y*5u)%150u);
        }
    for(uint32_t y=0;y<AVP_VK_MIXED_CRY_HEIGHT;y++)
        for(uint32_t x=0;x<AVP_VK_MIXED_TEX_WIDTH;x++)
            candidate.cry[(size_t)y*AVP_VK_MIXED_TEX_WIDTH+x]=
                (uint16_t)(((0x30u+(x&15u))<<8u)|((3u*y+20u)&255u));
    /* cry16 supplies the primary original texels, while rgb is only the
     * required non-null source pointer in AvpGpuRgbImage. */
    candidate.owned[0]=(AvpGpuRgbImage){16,128,candidate.rgb,candidate.cry,NULL};
    candidate.owned[1]=(AvpGpuRgbImage){16,720,candidate.rgb,NULL,NULL};
    candidate.sources[AVP_VK_MIXED_CRY_SLOT]=&candidate.owned[0];
    candidate.sources[AVP_VK_MIXED_RGB_SLOT]=&candidate.owned[1];
    scene.width=width;scene.height=height;
    scene.map_width=scene.map_height=1u;
    scene.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    scene.hit_capacity_per_column=1u;scene.maze=maze;
    scene.image[AVP_VK_MIXED_CRY_SLOT]=(AvpWorldImageMeta){16u,128u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    scene.image[AVP_VK_MIXED_RGB_SLOT]=(AvpWorldImageMeta){16u,720u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    scene.wall_image[1u]=AVP_VK_MIXED_CRY_SLOT;
    scene.wall_image[2u]=AVP_VK_MIXED_RGB_SLOT;
    scene.columns=calloc(width,sizeof(*scene.columns));
    scene.hits=calloc(width,sizeof(*scene.hits));
    if(!scene.columns || !scene.hits)goto done;
    for(uint32_t x=0;x<width;x++){
        uint32_t wall=((x/16u)&1u)?2u:1u;
        scene.columns[x]=(AvpWorldRayColumn){distance,1u,1u,0u};
        scene.hits[x]=(AvpWorldRayHit){distance,(double)(x%16u)/16.0,
                                     wall,0u,1u,1u,0,(int32_t)height};
    }
    if(!avp_vk_wall_projection_pack_lit(&candidate.geometry,&scene) ||
       !avp_vk_wall_projection_upgrade_exact_rows(&candidate.geometry,
           &candidate.mapped_columns,&candidate.mapped_rows) ||
       !avp_vk_wall_projection_row_parity(candidate.geometry.data,
           candidate.geometry.bytes,&differences) || differences ||
       !avp_vk_wall_slots_collect(candidate.geometry.data,candidate.geometry.bytes,&plan) ||
       plan.used_count!=2u ||
       plan.slots[AVP_VK_MIXED_CRY_SLOT].encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 ||
       plan.slots[AVP_VK_MIXED_RGB_SLOT].encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8)
        goto done;
    /* For the standard 480-line test a v3 map is mandatory, not merely
     * best-effort: this guards against accidentally testing integer rows. */
    if(height==480u && (!candidate.mapped_columns || !candidate.mapped_rows ||
       candidate.geometry.data[4u]!=AVP_VK_WALL_PROJECTION_EXACT_VERSION))goto done;
    *out=candidate;
    /* The borrowed source table must point at the PUBLISHED fixture's
     * embedded image descriptors, never the temporary stack candidate. */
    out->sources[AVP_VK_MIXED_CRY_SLOT]=&out->owned[0];
    out->sources[AVP_VK_MIXED_RGB_SLOT]=&out->owned[1];
    memset(&candidate,0,sizeof(candidate));
    ok=1;
done:
    free(scene.columns);free(scene.hits);
    avp_vk_wall_mixed_fixture_dispose(&candidate);
    return ok;
}
