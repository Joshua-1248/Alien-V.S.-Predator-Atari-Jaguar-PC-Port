#include "pc/pc_hybrid_viewport.h"
#include <string.h>
int avp_hybrid_viewport_make(uint32_t surface_w,uint32_t surface_h,
    uint32_t present_w,uint32_t present_h,
    uint32_t source_w,uint32_t source_h,AvpHybridViewport *out)
{
    AvpHybridViewport p={0};
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!surface_w||!surface_h||!present_w||!present_h||
       !source_w||!source_h||surface_w>8192u||surface_h>8192u||
       source_w>8192u||source_h>8192u||
       present_w>surface_w||present_h>surface_h)return 0;
    p.x=(surface_w-present_w)/2u;
    p.y=(surface_h-present_h)/2u;
    p.width=present_w;p.height=present_h;
    p.source_width=source_w;p.source_height=source_h;
    *out=p;
    return 1;
}
int avp_hybrid_viewport_source_pixel(const AvpHybridViewport *v,
    uint32_t x,uint32_t y,uint32_t *sx,uint32_t *sy)
{
    if(!v||!sx||!sy||!v->width||!v->height||
       !v->source_width||!v->source_height||
       x<v->x||y<v->y||x-v->x>=v->width||y-v->y>=v->height)return 0;
    *sx=(uint32_t)(((uint64_t)(x-v->x)*v->source_width)/v->width);
    *sy=(uint32_t)(((uint64_t)(y-v->y)*v->source_height)/v->height);
    return *sx<v->source_width&&*sy<v->source_height;
}
