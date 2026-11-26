/* R323 host-only GPU scene transfer contract. Mock lookup functions below
 * exercise the SOURCE ROUTING, while production code links real original
 * LEVELS/GetPanels mappings from visual_bridge.cpp. No ROM art embedded. */
#include "jaguar/avp_gpu/world_scene_packet.h"
#include "jaguar/avp_gpu/state.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int avp_gpu_wall_texture_file(unsigned level,u8 wall,unsigned *file)
{
    if(level!=1u || wall>=74u)return 0;
    if(file)*file=wall;
    return 1;
}
int avp_gpu_wall_notice_index(unsigned level,u8 wall,unsigned *index)
{
    if(level!=1u || wall!=16u)return 0;
    if(index)*index=8u;
    return 1;
}
int avp_gpu_flat_texture_file(unsigned level,u8 flat,unsigned *file)
{
    if(level!=1u || flat>=74u)return 0;
    if(file)*file=flat;
    return 1;
}
unsigned avp_gpu_flat_texture_rotation(unsigned level,u8 flat)
{
    return level==1u ? flat&3u : 0u;
}
int avp_gpu_wall_window_pair_file(unsigned level,u8 wall,unsigned *file)
{
    if(level!=1u || wall!=16u)return 0;
    if(file)*file=74u;
    return 1;
}

int main(void)
{
    /* Allocate large state on heap; its source ROM image pointers are test
     * fixtures, NOT owned by this frame packet. */
    AvpAvpGpuState *s=calloc(1,sizeof(*s));
    AvpWorldScenePacket *p=calloc(1,sizeof(*p));
    uint8_t *maze=calloc(64u*64u*8u,1u);
    static uint8_t dummy_rgb[3]={18u,72u,202u};
    static uint16_t cry[4]={0x1234u,0x55ffu,0x00ffu,0xf01fu};
    static uint8_t alpha[4]={0u,16u,128u,255u};
    uint32_t packed[4]={0};
    AvpWorldImageEncoding encoding=AVP_WORLD_IMAGE_ABSENT;
    int enabled=1;
    assert(s&&p&&maze);
    for(unsigned i=0;i<74u;i++){
        s->panel_tex[i].rgb=dummy_rgb;
        s->panel_tex[i].width=s->panel_tex[i].height=128;
    }
    AvpGpuRgbImage replacement[74]={0},signs[18]={0};
    for(unsigned i=0;i<74u;i++){
        replacement[i].rgb=dummy_rgb;
        replacement[i].width=replacement[i].height=1254;
    }
    for(unsigned i=0;i<18u;i++){
        s->notice_tex[i].rgb=dummy_rgb;
        s->notice_tex[i].width=s->notice_tex[i].height=128;
        signs[i].rgb=dummy_rgb;
        signs[i].width=signs[i].height=1254;
    }
    s->host_world_tex=replacement;
    s->host_notice_tex=signs;
    s->host_world_tex_enabled=&enabled;
    s->window_pair_loaded[0]=1u;
    s->window_pairs[0][0]=0xf0u;
    s->host_filter_mode=2u;
    s->host_aniso=8u;
    s->host_filter_mask=0x1fu;
    s->host_stereo_projection_shift=0.125;
    maze[4]=(uint8_t)34;
    assert(!avp_world_scene_init(p,0,1080));
    assert(!avp_world_scene_init(p,8193,1080));
    assert(avp_world_scene_init(p,1920,1080));
    assert(!avp_world_scene_begin(p,s,maze,0,64,1,1,2,1,0,0,1,960));
    assert(!avp_world_scene_begin(p,s,maze,64,64,16,1,2,1,0,0,1,960));
    assert(avp_world_scene_begin(p,s,maze,64,64,1,1,2,1,0,0,1,960));
    assert(p->image_slot_count==92u);
    assert(p->filter_mode==2u && p->anisotropic_taps==8u);
    assert(p->disable_fog==0u && p->fog_distance_mode==0u);
    assert(p->camera_x==1.0 && p->camera_y==2.0);
    assert(p->stereo_shift==0.125);
    assert(p->wall_image[16]==74u+8u);
    assert(p->wall_window_pair[16]==0u);
    assert(p->flat_image[16]==16u);
    assert(p->flat_rotation[19]==3u);
    assert(p->wall_image[73]==73u);
    assert(p->wall_image[74]==AVP_WORLD_SCENE_INVALID_SLOT);
    assert(p->image[73].width==1254u && p->image[74+8].height==1254u);
    assert(p->window_pairs[0][0]==0xf0u);
    assert(p->maze[4]==34u);
    maze[4]=42u;
    assert(p->maze[4]==34u); /* immutable frame snapshot */
    assert(!avp_world_scene_complete(p));
    /* Disjoint writes model concurrent ownership of separate columns. */
    for(uint32_t x=0;x<1920u;x++){
        if((x&1u)==0u)
            assert(avp_world_scene_hit(p,x,1.5,0.33,16u,63u,1u,0u,0,1080));
        assert(avp_world_scene_hit(p,x,3.0,0.5,33u,0u,0u,1u,100,980));
        avp_world_scene_end_column(p,x,3.0);
    }
    assert(p->columns[0].count==2u && p->columns[1].count==1u);
    assert(p->hits[0].wall==16u && p->hits[0].is_final==0u);
    assert(p->hits[1].wall==33u && p->hits[1].is_final==1u);
    assert(p->columns[1919].wall_depth==3.0);
    assert(avp_world_scene_complete(p));
    assert(!avp_world_scene_hit(p,1,2.0,0.5,0,0,0,1,0,500));
    assert(!avp_world_scene_complete(p)); /* late mutation must invalidate */
    avp_world_scene_invalidate(p);
    assert(!avp_world_scene_complete(p));
    assert(avp_world_scene_begin(p,s,maze,64,64,1,1,2,1,0,0,1,960));
    for(uint32_t j=0;j<49u;j++)
        assert(avp_world_scene_hit(p,2,j+1.0,0.0,1,0,1,0,0,1080));
    assert(!avp_world_scene_hit(p,2,50.0,0.0,1,0,1,0,0,1080));
    avp_world_scene_end_column(p,2,3.0);
    assert(p->columns[2].overflow);
    assert(!avp_world_scene_complete(p));
    assert(avp_world_scene_begin(p,s,maze,64,64,1,1,2,1,0,0,1,960));
    /* Fully clipped portals are valid source data, not malformed packets. */
    assert(avp_world_scene_hit(p,3,4.0,0.5,16,0,1,0,900,800));
    assert(!avp_world_scene_hit(p,4,-1.0,0.5,16,0,1,0,0,1080));
    assert(p->columns[4].overflow);
    /* Both original CRY and replacement RGB fallback formats are defined
     * explicitly as unsigned integers. Transparency is never discarded. */
    AvpGpuRgbImage tiny={2,2,dummy_rgb,cry,alpha};
    assert(!avp_world_scene_pack_image(&tiny,packed,3u,&encoding));
    assert(avp_world_scene_pack_image(&tiny,packed,4u,&encoding));
    assert(encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    assert(packed[0]==0x00001234u && packed[1]==0x001055ffu);
    assert(packed[2]==0x008000ffu && packed[3]==0x00fff01fu);
    tiny.width=tiny.height=1;
    tiny.cry16=NULL;
    tiny.alpha=NULL;
    assert(avp_world_scene_pack_image(&tiny,packed,1u,&encoding));
    assert(encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(packed[0]==0xff1248cau);
    tiny.width=0;
    assert(!avp_world_scene_pack_image(&tiny,packed,1u,&encoding));
    assert(avp_world_scene_source(s,3u)==&replacement[3]);
    assert(avp_world_scene_source(s,74u+8u)==&signs[8]);
    enabled=0;
    assert(avp_world_scene_source(s,3u)==&s->panel_tex[3]);
    assert(avp_world_scene_source(s,74u+8u)==&s->notice_tex[8]);
    assert(avp_world_scene_begin(p,s,maze,64,64,1,1,2,1,0,0,1,960));
    assert(p->image[3].width==128u && p->image[74u+8].height==128u);
    s->notice_tex[8].rgb=NULL;
    assert(avp_world_scene_begin(p,s,maze,64,64,1,1,2,1,0,0,1,960));
    assert(p->wall_image[16]==16u); /* missing notice falls back to panel */
    assert(!avp_world_scene_complete(p));
    avp_world_scene_destroy(p);
    free(p);free(maze);free(s);
    puts("R323 Vulkan scene packet: 1920 columns, 74 panels + 18 notices, 49-hit bounds, exact texture bits PASS");
    return 0;
}
