#include "jaguar/avp_gpu/maze2_occlusion.h"
#include <float.h>
#include <stdint.h>
#include <stdlib.h>

int avp_gpu_maze2_occlusion_ensure(AvpMaze2Occlusion *o,unsigned width,unsigned height)
{
    size_t count;
    float *p;
    if(!o||!width||!height)return 0;
    if((size_t)width>SIZE_MAX/(size_t)height)return 0;
    count=(size_t)width*(size_t)height;
    if(count>SIZE_MAX/sizeof(*p))return 0;
    if(o->depth&&o->width==width&&o->height==height)return 1;
    p=(float*)malloc(count*sizeof(*p));
    if(!p)return 0;
    free(o->depth);
    o->depth=p;o->width=width;o->height=height;
    avp_gpu_maze2_occlusion_clear(o);
    return 1;
}

void avp_gpu_maze2_occlusion_destroy(AvpMaze2Occlusion *o)
{
    if(!o)return;
    free(o->depth);o->depth=NULL;o->width=o->height=0u;
}

void avp_gpu_maze2_occlusion_clear(AvpMaze2Occlusion *o)
{
    size_t i,count;
    if(!o||!o->depth)return;
    count=(size_t)o->width*(size_t)o->height;
    for(i=0;i<count;i++)o->depth[i]=FLT_MAX;
}

void avp_gpu_maze2_occlusion_mark_span(AvpMaze2Occlusion *o,unsigned x,int y0,int y1,float depth)
{
    int y;
    if(!o||!o->depth||x>=o->width||y1<=y0)return;
    if(y0<0)y0=0;
    if(y1>(int)o->height)y1=(int)o->height;
    if(y1<=y0)return;
    for(y=y0;y<y1;y++){
        float *slot=&o->depth[(size_t)y*(size_t)o->width+x];
        if(depth<*slot)*slot=depth;
    }
}

float avp_gpu_maze2_occlusion_depth(const AvpMaze2Occlusion *o,int x,int y)
{
    if(!o||!o->depth||x<0||y<0||(unsigned)x>=o->width||(unsigned)y>=o->height)return FLT_MAX;
    return o->depth[(size_t)y*(size_t)o->width+(unsigned)x];
}

int avp_gpu_maze2_occlusion_blocks(const AvpMaze2Occlusion *o,int x,int y,float object_depth)
{
    float d=avp_gpu_maze2_occlusion_depth(o,x,y);
    return d!=FLT_MAX&&object_depth>=d;
}
