/* R334: exercise actual production ray-only capture (no framebuffer pixels).
 * Compare against the exact underlying source raycaster for every column. */
#include "../new/jaguar/avp_gpu/software_renderer.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pc/pc_vulkan_geometry_policy.h"
#include "pc/pc_vulkan_wall_projection.h"
#include "pc/pc_vulkan_direct_rays.h"

int avp_gpu_wall_texture_file(unsigned level,u8 wall,unsigned *file)
{ if (level!=1u || wall>73u) return 0; if(file) *file=wall;return 1; }
int avp_gpu_flat_texture_file(unsigned level,u8 wall,unsigned *file)
{ return avp_gpu_wall_texture_file(level,wall,file); }
int avp_gpu_wall_notice_index(unsigned level,u8 wall,unsigned *idx)
{(void)level;(void)wall;(void)idx;return 0;}
int avp_gpu_wall_window_pair_file(unsigned level,u8 wall,unsigned *file)
{(void)level;(void)wall;(void)file;return 0;}
unsigned avp_gpu_flat_texture_rotation(unsigned level,u8 flat)
{(void)level;(void)flat;return 0;}

int avp_gpu_retail_wall_intensity_delta(double d) {(void)d;return 0;}
static int test(unsigned width,unsigned height,unsigned angle,int smooth)
{
    AvpAvpGpuState *s=calloc(1,sizeof(*s));
    AvpWorldScenePacket p={0};
    AvpWorldGpuWire wire={0};
    AvpVkWallProjectionStream wall_projection={0};
    uint8_t *maze=calloc(64u*64u,8u);
    static uint8_t pixel[3]={99,88,77};
    double px=32.5,py=32.5,a,fx,fy,rx,ry,focal;
    assert(s && maze);
    for(unsigned i=0;i<64u*64u;i++){
        uint8_t *c=maze+i*8u;
        c[0]=1u;c[1]=1u;c[2]=1u;c[3]=1u;
    }
    s->panel_tex[1].rgb=pixel;
    s->panel_tex[1].width=1;
    s->panel_tex[1].height=1;
    s->host_widescreen_fov=1;
    assert(avp_world_scene_init(&p,width,height));
    assert(avp_gpu_trace_world_wire(s,maze,64u,64u,(u32)(px*65536),
         (u32)(py*65536),angle,1u,smooth,width,height,&p,&wire));
    assert(wire.bytes>0u && avp_vk_geometry_wire_valid(wire.data,wire.bytes));
    assert(avp_world_scene_complete(&p));
    /* R382: the GPU-first scene is sourced by the independent original
     * ray tracer. Its exact semantic contract must be stable across calls
     * and reject corrupt/interrupted geometry before any GPU submission. */
    {
        AvpWorldScenePacket other={0};
        assert(avp_world_scene_init(&other,width,height));
        assert(avp_gpu_trace_world_rays(s,maze,64u,64u,
            (u32)(px*65536),(u32)(py*65536),angle,1u,smooth,
            width,height,&other));
        assert(avp_vk_direct_rays_match_capture(&other,&p));
        other.columns[width/2u].wall_depth+=1.0;
        assert(!avp_vk_direct_rays_match_capture(&other,&p));
        other.columns[width/2u].wall_depth-=1.0;
        assert(avp_vk_direct_rays_match_capture(&other,&p));
        other.flat_image[3u]^=1u;
        assert(!avp_vk_direct_rays_match_capture(&other,&p));
        other.flat_image[3u]^=1u;
        assert(avp_vk_direct_rays_match_capture(&other,&p));
        other.camera_x+=0.001;
        assert(!avp_vk_direct_rays_match_capture(&other,&p));
        other.camera_x-=0.001;
        /* Double subtraction can have a rounding tie; restoring explicitly
         * is the intentional original-camera semantic contract. */
        other.camera_x=p.camera_x;
        assert(avp_vk_direct_rays_match_capture(&other,&p));
        other.window_pairs[0u][0u]^=1u;
        assert(!avp_vk_direct_rays_match_capture(&other,&p));
        other.window_pairs[0u][0u]^=1u;
        assert(avp_vk_direct_rays_match_capture(&other,&p));
        other.maze[0u]^=1u;
        assert(!avp_vk_direct_rays_match_capture(&other,&p));
        other.maze[0u]^=1u;
        assert(avp_vk_direct_rays_match_capture(&other,&p));
        avp_world_scene_invalidate(&other);
        assert(!avp_vk_direct_rays_match_capture(&other,&p));
        avp_world_scene_destroy(&other);
    }
    /* R336: real ray-only output, not a synthetic pixel buffer, must produce
     * the same conservative projected-wall records for plain-wall scenes. */
    assert(avp_vk_wall_projection_pack(&wall_projection,&p));
    assert(avp_vk_wall_projection_validate(wall_projection.data,
                                            wall_projection.bytes));
    a=avp_gpu_heading_radians(angle,s->host_continuous_heading);
    fx=cos(a);fy=sin(a);rx=-fy;ry=fx;
    focal=avp_host_has_extra_columns(width,height)?
           avp_host_wide_focal(height):(double)width*0.5;
    assert(p.focal==focal);
    for(unsigned x=0;x<width;x++){
        double tanoff=((double)x+0.5-(double)width*0.5)/focal;
        double dx=fx+rx*tanoff,dy=fy+ry*tanoff;
        PcRayHit portals[AVP_MAZE2_MAX_RAY_PORTALS];
        unsigned count=0;
        PcRayHit last=cast_ray_hit(s,1u,maze,64u,64u,px,py,dx,dy,
                                   portals,&count,AVP_MAZE2_MAX_RAY_PORTALS);
        pc_portal_clip_column(s,1u,portals,count,&last,height);
        const AvpWorldRayColumn *column=&p.columns[x];
        assert(column->ready && !column->overflow);
        assert(column->count==count+(last.hit?1u:0u));
        assert(column->wall_depth==(last.hit?last.d:avp_host_ray_limit(0)));
        if(last.hit){
            const AvpWorldRayHit *h=&p.hits[(size_t)x*p.hit_capacity_per_column+count];
            assert(h->distance==last.d && h->texture_fraction==last.tc);
            assert(h->wall==last.wall && h->open==last.open);
            assert(h->clip_lo==last.clip_lo && h->clip_hi==last.clip_hi);
            assert((int)h->vertical==last.vertical && h->is_final);
        }
    }
    assert(!avp_gpu_trace_world_wire(s,NULL,64u,64u,(u32)(px*65536),
         (u32)(py*65536),angle,1u,smooth,width,height,&p,&wire));
    assert(wire.bytes==0u);
    assert(!avp_world_scene_complete(&p));
    assert(!avp_vk_wall_projection_pack(&wall_projection,&p));
    assert(wall_projection.bytes==0u);
    avp_world_gpu_wire_destroy(&wire);
    avp_vk_wall_projection_destroy(&wall_projection);
    avp_world_scene_destroy(&p);
    free(maze);free(s);
    return 1;
}
int main(void)
{
    assert(test(320u,228u,0u,0));
    assert(test(1920u,1080u,0x4000u,1));
    assert(test(1920u,1080u,0x9777u,0));
    assert(test(853u,480u,0xb300u,1));
    puts("PASS R382 original source ray-only parity and GPU-first semantic equality/rejection at 320, 853 and 1920 columns");
    return 0;
}
