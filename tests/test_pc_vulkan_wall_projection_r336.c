/* RE46 R336: source-projection parity and fail-closed wall-band tests. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_vulkan_wall_projection.h"
#include "jaguar/avp_gpu/maze2_column.h"
#include "jaguar/avp_gpu/hires_wall_sample.h"

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0]|(uint32_t)p[1]<<8u|
           (uint32_t)p[2]<<16u|(uint32_t)p[3]<<24u;
}
static void setup(AvpWorldScenePacket *p,uint32_t w,uint32_t h,uint32_t tw,uint32_t th)
{
    memset(p,0,sizeof(*p));
    p->width=w;p->height=h;
    p->hit_capacity_per_column=1u;
    p->image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    p->columns=calloc(w,sizeof(*p->columns));
    p->hits=calloc(w,sizeof(*p->hits));
    p->maze=calloc(64u*64u,8u);
    p->map_width=p->map_height=64u;
    assert(p->maze && p->hits && p->columns);
    p->wall_image[1u]=p->wall_image[129u]=5u;
    p->image[5]=(AvpWorldImageMeta){tw,th,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    for(uint32_t x=0;x<w;x++){
        double d=(x%4u==0u)?0.06:(x%4u==1u)?1.0:(x%4u==2u)?3.75:35.0;
        p->columns[x]=(AvpWorldRayColumn){d,1u,1u,0u};
        p->hits[x]=(AvpWorldRayHit){d,(double)(x%128u)/128.0,
                     x%2u?129u:1u,0u,1u,1u,0,(int32_t)h};
    }
}
static void cleanup(AvpWorldScenePacket *p)
{
    free(p->columns);free(p->hits);free(p->maze);
}
static void basic(uint32_t w,uint32_t h,uint32_t tw,uint32_t th)
{
    AvpWorldScenePacket p;
    AvpVkWallProjectionStream s={0};
    uint32_t visible=0;
    setup(&p,w,h,tw,th);
    assert(avp_vk_wall_projection_pack(&s,&p));
    assert(s.bytes==AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                    (size_t)w*AVP_VK_WALL_PROJECTION_COLUMN_BYTES);
    assert(avp_vk_wall_projection_validate(s.data,s.bytes));
    for(uint32_t x=0;x<w;x++){
        const uint8_t *r=s.data+AVP_VK_WALL_PROJECTION_HEADER_BYTES+
                         (size_t)x*AVP_VK_WALL_PROJECTION_COLUMN_BYTES;
        const AvpWorldRayHit *hit=p.hits+x;
        int wallh=avp_gpu_maze2_project_wall_height(hit->distance,h), y0=0,y1=0;
        assert(avp_gpu_maze2_centered_wall_bounds(h,wallh,&y0,&y1));
        wallh=y1-y0;
        int top=y0<0?0:y0,bottom=y1>(int)h?(int)h:y1;
        int col=(int)(hit->texture_fraction*128.0);
        if(hit->wall&128u)col=127-col;
        int tx=tw>128u?avp_gpu_hires_wall_x(col,hit->texture_fraction,
                                    (hit->wall&128u)!=0,0,tw):col*(int)tw/128;
        uint32_t active=(top<bottom)?1u:0u;
        assert(read_u32(r)==active);
        if(active){
            ++visible;
            assert(read_u32(r+4)==5u && read_u32(r+8)==hit->wall);
            assert((int)read_u32(r+12)==top && (int)read_u32(r+16)==bottom);
            assert((int32_t)read_u32(r+20)-(int32_t)AVP_VK_WALL_PROJECTION_BASE_BIAS==y0 &&
                   (int)read_u32(r+24)==wallh);
            assert((int)read_u32(r+28)==tx);
            assert(read_u32(r+32)==tw && read_u32(r+36)==th);
            for(int y=top;y<bottom;y+=(bottom-top)/11+1){
                int ty=avp_vk_wall_projection_sample_y(y,y0,wallh,(int)th);
                int ref=th>128u ?
                    avp_gpu_hires_wall_y(y-y0,wallh,0,th) :
                    avp_gpu_maze2_host_source_row(y-y0,wallh,0)*(int)th/128;
                assert(ty==ref && ty>=0 && ty<(int)th);
            }
        }
    }
    assert(read_u32(s.data+24u)==visible);
    assert(visible>0u);
    /* Failing new frame cannot expose previously valid output. */
    p.hits[w/2u].wall=0x18u; /* window type 3, unsupported */
    assert(!avp_vk_wall_projection_pack(&s,&p));
    assert(s.bytes==0u);
    p.hits[w/2u].wall=(w/2u)%2u?129u:1u;
    assert(avp_vk_wall_projection_pack(&s,&p));
    /* Fuzz byte-level gate: a swapped hit width, reserved word, or
       impossible texture source coordinate MUST be rejected. */
    s.data[16u]^=1u;
    assert(!avp_vk_wall_projection_validate(s.data,s.bytes));
    s.data[16u]^=1u;
    assert(avp_vk_wall_projection_validate(s.data,s.bytes));
    s.data[28u]=1u;
    assert(!avp_vk_wall_projection_validate(s.data,s.bytes));
    s.data[28u]=0u;
    assert(avp_vk_wall_projection_validate(s.data,s.bytes));
    /* A portal hit must not silently become a solid wall. */
    p.columns[0].count=2u;
    assert(!avp_vk_wall_projection_pack(&s,&p));
    assert(s.bytes==0u);
    p.columns[0].count=1u;
    p.columns[0].ready=0u;
    assert(!avp_vk_wall_projection_pack(&s,&p));
    p.columns[0].ready=1u;
    p.hits[0].distance=0.001; /* near-field spans too tall for 32-bit shader arithmetic */
    assert(!avp_vk_wall_projection_pack(&s,&p));
    p.hits[0].distance=p.columns[0].wall_depth;
    p.hits[0].texture_fraction=NAN;
    assert(!avp_vk_wall_projection_pack(&s,&p));
    assert(s.bytes==0u);
    avp_vk_wall_projection_destroy(&s);
    cleanup(&p);
}
static void test_gpu_reference(uint32_t w,uint32_t h,uint32_t format)
{
    const size_t bytes=(size_t)w*h*4u;
    uint8_t *pixels=malloc(bytes);
    uint32_t differences=UINT32_MAX;
    int wallh=avp_gpu_maze2_project_wall_height(1.0,h),y0=0,y1=0;
    assert(pixels && avp_gpu_maze2_centered_wall_bounds(h,wallh,&y0,&y1));
    wallh=y1-y0;
    for(uint32_t y=0u;y<h;++y)for(uint32_t x=0u;x<w;++x){
        uint32_t rgb=0u;
        if((int)y>=y0 && (int)y<y1){
            const int tx=(int)(x%16u);
            const int ty=avp_gpu_maze2_host_source_row((int)y-y0,wallh,0)*16/128;
            rgb=((tx^ty)&1)?0x00336699u:0x00bb8855u;
        }
        size_t off=((size_t)y*w+x)*4u;
        pixels[off]=(uint8_t)(rgb>>(format==44u?0u:16u));
        pixels[off+1u]=(uint8_t)(rgb>>8u);
        pixels[off+2u]=(uint8_t)(rgb>>(format==44u?16u:0u));
        pixels[off+3u]=255u;
    }
    assert(avp_vk_wall_projection_check_readback(pixels,bytes,w,h,format,&differences));
    assert(differences==0u);
    pixels[((size_t)(h/2u)*w+w/2u)*4u]^=1u;
    assert(!avp_vk_wall_projection_check_readback(pixels,bytes,w,h,format,&differences));
    assert(differences==1u);
    assert(!avp_vk_wall_projection_check_readback(pixels,bytes-1u,w,h,format,&differences));
    free(pixels);
}

int main(void)
{
    basic(320u,228u,128u,128u);
    basic(853u,480u,1254u,1254u);
    basic(1920u,1080u,128u,128u);
    basic(1920u,1080u,1254u,1254u);
    test_gpu_reference(319u,257u,37u);
    test_gpu_reference(319u,257u,44u);
    test_gpu_reference(640u,480u,37u);
    test_gpu_reference(640u,480u,44u);
    assert(avp_vk_wall_projection_sample_y(0,0,0,128)==-1);
    puts("R336 projected solid wall stream: original bounds, source columns, native/high-res rows, 4 CPU GPU-readback oracles, rejection and wire parity PASS");
    return 0;
}
