#include "jaguar/avp_gpu/maze2_occlusion.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>

int main(void)
{
    AvpMaze2Occlusion o={0};
    int x,y;
    assert(avp_gpu_maze2_occlusion_ensure(&o,4u,6u));
    assert(o.width==4u&&o.height==6u&&o.depth);
    for(y=0;y<6;y++)for(x=0;x<4;x++)assert(avp_gpu_maze2_occlusion_depth(&o,x,y)==FLT_MAX);

    /* A foreground frame occupies only rows [1,3); the opening stays clear. */
    avp_gpu_maze2_occlusion_mark_span(&o,2u,1,3,5.0f);
    assert(avp_gpu_maze2_occlusion_depth(&o,2,0)==FLT_MAX);
    assert(avp_gpu_maze2_occlusion_depth(&o,2,1)==5.0f);
    assert(avp_gpu_maze2_occlusion_depth(&o,2,2)==5.0f);
    assert(avp_gpu_maze2_occlusion_depth(&o,2,3)==FLT_MAX);
    assert(avp_gpu_maze2_occlusion_blocks(&o,2,1,5.0f));
    assert(avp_gpu_maze2_occlusion_blocks(&o,2,1,8.0f));
    assert(!avp_gpu_maze2_occlusion_blocks(&o,2,1,4.0f));
    assert(!avp_gpu_maze2_occlusion_blocks(&o,2,3,100.0f));

    /* Nearest authored span wins, regardless of draw order. */
    avp_gpu_maze2_occlusion_mark_span(&o,2u,1,3,9.0f);
    assert(avp_gpu_maze2_occlusion_depth(&o,2,1)==5.0f);
    avp_gpu_maze2_occlusion_mark_span(&o,2u,1,3,2.0f);
    assert(avp_gpu_maze2_occlusion_depth(&o,2,1)==2.0f);

    /* Bounds clipping must be safe and still mark the visible rows. */
    avp_gpu_maze2_occlusion_mark_span(&o,1u,-4,10,7.0f);
    for(y=0;y<6;y++)assert(avp_gpu_maze2_occlusion_depth(&o,1,y)==7.0f);
    avp_gpu_maze2_occlusion_mark_span(&o,99u,0,6,1.0f);
    assert(avp_gpu_maze2_occlusion_depth(&o,-1,0)==FLT_MAX);
    assert(avp_gpu_maze2_occlusion_depth(&o,0,-1)==FLT_MAX);
    assert(avp_gpu_maze2_occlusion_depth(&o,4,0)==FLT_MAX);
    assert(avp_gpu_maze2_occlusion_depth(&o,0,6)==FLT_MAX);

    avp_gpu_maze2_occlusion_clear(&o);
    for(y=0;y<6;y++)for(x=0;x<4;x++)assert(avp_gpu_maze2_occlusion_depth(&o,x,y)==FLT_MAX);

    /* Same-size ensure preserves allocation but resets nothing by itself. */
    avp_gpu_maze2_occlusion_mark_span(&o,0u,0,1,3.0f);
    assert(avp_gpu_maze2_occlusion_ensure(&o,4u,6u));
    assert(avp_gpu_maze2_occlusion_depth(&o,0,0)==3.0f);

    /* Resize yields a fresh clear mask. */
    assert(avp_gpu_maze2_occlusion_ensure(&o,2u,2u));
    assert(o.width==2u&&o.height==2u);
    assert(avp_gpu_maze2_occlusion_depth(&o,0,0)==FLT_MAX);

    avp_gpu_maze2_occlusion_destroy(&o);
    assert(!o.depth&&o.width==0u&&o.height==0u);
    puts("maze2 occlusion tests passed");
    return 0;
}
