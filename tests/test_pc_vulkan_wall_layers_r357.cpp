/* R357: exact MAZE2 source-coordinate layers across windows, sliding doors,
 * vertical doors and multiple portal hits. No ROM/texture artwork required. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "pc/pc_vulkan_wall_layers.h"
#include "jaguar/avp_gpu/maze2_column.h"
#include "jaguar/avp_gpu/hires_wall_sample.h"
#include "jaguar/avp_gpu/wall_raster_dda.h"
}
extern "C" int avp_gpu_retail_wall_intensity_delta(double distance)
{
    (void)distance;return -12;
}
static uint32_t slot=0,xtex=0,ytex=0;
static int32_t shade=0;
static bool lookup(const AvpVkWallLayers &s,unsigned x,unsigned y)
{
    return avp_vk_wall_layers_lookup(&s,x,y,&slot,&xtex,&ytex,&shade)!=0;
}
int main()
{
    AvpWorldScenePacket p={};
    assert(avp_world_scene_init(&p,8u,240u));
    p.map_width=p.map_height=8u;
    p.maze=(uint8_t*)calloc(64u,8u);assert(p.maze);
    p.maze_capacity=512u;
    for(auto &item:p.wall_image)item=15u;
    p.image[15]={128u,128u,AVP_WORLD_IMAGE_CRY16_ALPHA8};
    p.image[16]={384u,720u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    p.wall_image[3u<<3u]=16u;
    p.wall_image[(13u<<3u)|0x80u]=16u;
    p.window_pair_loaded[0]=1u;
    p.wall_window_pair[3u<<3u]=0u;
    p.wall_window_pair[4u<<3u]=0u;
    p.wall_window_pair[(13u<<3u)|0x80u]=0u;
    for(unsigned i=0;i<128u;i++){
        p.window_pairs[0][i*2u]=(uint8_t)-18;
        p.window_pairs[0][i*2u+1u]=(uint8_t)18;
    }
    assert(avp_world_scene_hit(&p,0u,2.0,.25,1u,0u,0u,1u,0,240));
    assert(avp_world_scene_hit(&p,1u,2.0,.25,7u<<3u,64u,0u,1u,0,240));
    assert(avp_world_scene_hit(&p,2u,2.0,.25,8u<<3u,32u,0u,1u,0,240));
    assert(avp_world_scene_hit(&p,3u,2.0,.25,3u<<3u,0u,0u,1u,0,240));
    assert(avp_world_scene_hit(&p,4u,2.0,.25,4u<<3u,32u,0u,1u,0,240));
    assert(avp_world_scene_hit(&p,5u,2.0,.75,(13u<<3u)|0x80u,0u,0u,1u,0,240));
    assert(avp_world_scene_hit(&p,6u,1.0,.25,3u<<3u,0u,0u,0u,0,240));
    assert(avp_world_scene_hit(&p,6u,2.0,.25,1u,0u,0u,1u,50,190));
    /* Original ray can leave the draw-distance limit past its last window:
     * such a column has NO final solid wall. */
    assert(avp_world_scene_hit(&p,7u,2.0,.25,3u<<3u,0u,0u,0u,0,240));
    for(unsigned i=0;i<8u;i++)avp_world_scene_end_column(&p,i,2.0);
    assert(avp_world_scene_complete(&p));
    AvpVkWallLayers s={};
    assert(avp_vk_wall_layers_build(&p,&s));
    assert(avp_vk_wall_layers_validate(&s,&p));
    fprintf(stderr,"R357 spans=%u rows=%u doors=%u windows=%u portals=%u\n",s.span_count,s.row_count,s.doorway_columns,s.window_columns,s.portal_columns);
    assert(s.span_count>=10u&&s.row_count>400u);
    assert(s.doorway_columns>=3u&&s.window_columns>=3u);
    assert(s.portal_columns==2u);
    assert(!avp_vk_wall_layers_build(&p,&s));
    assert(lookup(s,0u,120u)&&slot==15u&&shade==-12);
    /* MAZE2 vertical-up door: only the upper half remains when open=64. */
    assert(lookup(s,1u,100u)&&slot==15u);
    assert(!lookup(s,1u,140u));
    /* MAZE2 split-up door: the moving halves leave an uncovered centre. */
    assert(lookup(s,2u,92u));
    assert(!lookup(s,2u,120u));
    assert(lookup(s,2u,148u));
    /* A high-resolution source is retained at full width/height. */
    assert(lookup(s,3u,90u)&&slot==16u&&xtex<384u&&ytex<720u);
    assert(!lookup(s,3u,120u));
    assert(lookup(s,3u,145u)&&slot==16u);
    /* Sliding centred door with window frames and moving middle. */
    assert(lookup(s,4u,92u)&&slot==15u);
    /* Far wall remains visible INSIDE the nearest portal opening; the
     * window framing paints on top outside of its opening. */
    assert(lookup(s,6u,120u)&&slot==15u);
    assert(lookup(s,6u,90u)&&slot==16u);
    assert(!lookup(s,7u,120u));
    assert(lookup(s,7u,92u)&&slot==16u);
    /* Check exact Software high-resolution double stepping, not merely the
     * mathematically equivalent per-pixel quotient near tie boundaries. */
    for(unsigned i=s.column_start[3u];i<s.column_start[4u];i++){
        const auto &span=s.spans[i];
        assert(span.source_slot==16u);
        const int projected=avp_gpu_maze2_project_wall_height(2.0,p.height);
        int top=0,bottom=0;
        avp_gpu_maze2_centered_wall_bounds(p.height,projected,&top,&bottom);
        const double step=720.0/double(bottom-top);
        double sample=(double(int(span.top)-top)+.5)*step;
        for(unsigned y=span.top;y<span.bottom;y++,sample+=step){
            unsigned expected=sample<0?0u:sample>=720.0?719u:unsigned(sample);
            assert(s.rows[span.row_offset+y-span.top]==expected);
        }
    }
    auto old=s.spans[0].row_offset;
    s.spans[0].row_offset=s.row_count;
    assert(!avp_vk_wall_layers_validate(&s,&p));
    s.spans[0].row_offset=old;
    assert(avp_vk_wall_layers_validate(&s,&p));
    avp_vk_wall_layers_dispose(&s);
    assert(s.spans==nullptr && s.rows==nullptr && s.column_start==nullptr);
    /* Remaining MAZE2 classes: authored bulkhead doors, horizontal
     * shutter, singles window, and ordinary final wall. */
    memset(p.columns,0,p.width*sizeof(*p.columns));
    p.wall_window_pair[9u<<3u]=0u;
    p.wall_window_pair[10u<<3u]=0u;
    p.wall_window_pair[11u<<3u]=0u;
    p.wall_window_pair[6u<<3u]=0u;
    p.wall_window_pair[14u<<3u]=0u;
    p.wall_window_pair[15u<<3u]=0u;
    const uint32_t codes[8]={9u<<3u,10u<<3u,11u<<3u,5u<<3u,
                             6u<<3u,14u<<3u,15u<<3u,12u<<3u};
    for(unsigned i=0;i<8u;i++){
        assert(avp_world_scene_hit(&p,i,2.0,.25,codes[i],
            (i==3u?40u:i==4u?0u:12u),0u,1u,0,240));
        avp_world_scene_end_column(&p,i,2.0);
    }
    assert(avp_vk_wall_layers_build(&p,&s));
    assert(avp_vk_wall_layers_validate(&s,&p));
    for(unsigned i=0;i<3u;i++){
        assert(lookup(s,i,92u));
        assert(lookup(s,i,145u));
    }
    assert(!lookup(s,3u,120u)); /* fully retracted sliding shutter */
    assert(lookup(s,4u,92u) && !lookup(s,4u,120u));
    assert(lookup(s,5u,92u) && !lookup(s,5u,120u));
    assert(lookup(s,6u,145u) && !lookup(s,6u,120u));
    assert(lookup(s,7u,120u));
    avp_vk_wall_layers_dispose(&s);
    p.columns[0].overflow=1u;
    assert(!avp_vk_wall_layers_build(&p,&s));
    assert(!s.spans&&!s.column_start&&!s.rows);
    p.columns[0].overflow=0u;
    p.filter_mask=1u<<3u;p.filter_mode=1u;
    assert(!avp_vk_wall_layers_build(&p,&s));
    avp_world_scene_destroy(&p);
    puts("R357 layered Jaguar wall/door/window/portal source geometry PASS");
    return 0;
}
