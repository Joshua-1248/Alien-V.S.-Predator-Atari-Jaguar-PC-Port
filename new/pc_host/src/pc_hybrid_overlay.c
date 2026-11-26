#include "pc/pc_hybrid_overlay.h"
#include <stdlib.h>
#include <string.h>

int avp_hybrid_overlay_ensure(AvpHybridOverlay *f,uint32_t w,uint32_t h)
{
    size_t n;
    uint32_t *world,*foreground;
    if(!f)return 0;
    f->valid=0;
    if(!w||!h||w>AVP_HYBRID_OVERLAY_MAX_PIXELS/h)return 0;
    if(f->world && f->foreground && f->width==w && f->height==h)return 1;
    n=(size_t)w*h;
    world=(uint32_t*)malloc(n*sizeof(uint32_t));
    foreground=(uint32_t*)malloc(n*sizeof(uint32_t));
    if(!world||!foreground){free(world);free(foreground);return 0;}
    free(f->world);free(f->foreground);
    f->world=world;f->foreground=foreground;
    f->width=w;f->height=h;
    f->changed=f->transparent=0u;
    f->min_x=f->max_x=f->min_y=f->max_y=0u;
    return 1;
}
void avp_hybrid_overlay_invalidate(AvpHybridOverlay *f)
{
    if(f)f->valid=0;
}
void avp_hybrid_overlay_destroy(AvpHybridOverlay *f)
{
    if(!f)return;
    free(f->world);free(f->foreground);
    memset(f,0,sizeof(*f));
}
int avp_hybrid_overlay_finish(AvpHybridOverlay *f,
    const uint32_t *final,uint32_t stride,uint32_t cap_percent)
{
    uint64_t n;
    uint32_t changed=0u,minx=0u,miny=0u,maxx=0u,maxy=0u;
    if(!f)return 0;
    f->valid=0;
    f->changed=f->transparent=0u;
    if(!final||!f->world||!f->foreground||!f->width||!f->height||
       f->height>AVP_HYBRID_OVERLAY_MAX_PIXELS/f->width||
       stride<f->width||cap_percent>100u)return 0;
    n=(uint64_t)f->width*f->height;
    /* Reject an unbounded source array address calculation. */
    if((size_t)stride>SIZE_MAX/sizeof(uint32_t)/f->height)return 0;
    for(uint32_t y=0u;y<f->height;y++){
        const uint32_t *row=final+(size_t)y*stride;
        for(uint32_t x=0u;x<f->width;x++){
            size_t index=(size_t)y*f->width+x;
            uint32_t color=row[x]&0x00ffffffu;
            if((f->world[index]&0x00ffffffu)==color){
                f->foreground[index]=0u;
            }else{
                f->foreground[index]=0xff000000u|color;
                if(!changed){minx=x;maxx=x+1u;miny=y;maxy=y+1u;}
                else{
                    if(x<minx)minx=x;
                    if(x+1u>maxx)maxx=x+1u;
                    if(y<miny)miny=y;
                    if(y+1u>maxy)maxy=y+1u;
                }
                changed++;
            }
        }
    }
    f->changed=changed;
    f->transparent=(uint32_t)(n-changed);
    f->min_x=minx;f->max_x=maxx;f->min_y=miny;f->max_y=maxy;
    /* Compare integers in uint64_t: no rounding or overflow at huge sizes.
     * Global CLUT/thermal fades often touch everything: fall back cleanly. */
    if((uint64_t)changed*100u>n*cap_percent)return 0;
    f->valid=1;
    return 1;
}
int avp_hybrid_overlay_reconstruct(const AvpHybridOverlay *f,
    uint32_t *dst,size_t dst_pixels)
{
    size_t n;
    if(!f||!f->valid||!f->world||!f->foreground||!dst||
       !f->width||!f->height||
       f->height>AVP_HYBRID_OVERLAY_MAX_PIXELS/f->width)return 0;
    n=(size_t)f->width*f->height;
    if(dst_pixels<n)return 0;
    for(size_t i=0u;i<n;i++){
        const uint32_t fg=f->foreground[i];
        if(fg>>24u==255u)dst[i]=fg&0x00ffffffu;
        else if(fg>>24u==0u)dst[i]=f->world[i]&0x00ffffffu;
        else return 0; /* partial alpha is intentionally never emitted */
    }
    return 1;
}
