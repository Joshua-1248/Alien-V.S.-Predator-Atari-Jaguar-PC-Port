#ifndef AVP_MICRO_FP_WIDE_FAST_H
#define AVP_MICRO_FP_WIDE_FAST_H
#include "jaguar/avp_gpu/fp_wide_blit.h"
#include <string.h>
/* Expand opaque runs once, copy to remaining output rows. Never copy transparent gaps. */
static inline void avp_micro_fp_wide_blit(uint32_t *dst,unsigned dw,unsigned dh,
    const uint8_t *sprite,size_t sprite_size,int sw,int sh,
    const uint8_t palette[208][3],unsigned palette_count,
    int original_x,int original_y,int four_bit)
{
    unsigned center_w,xe[321],ye[241];
    int64_t center_left;
    size_t stride;
    if(!dst||!dw||!dh||!sprite||!palette||sw<=0||sh<=0||
       sw>320||sh>240)return; /* Largest original OP picture: 320x240. */
    stride=four_bit?((size_t)sw+1u)/2u:(size_t)sw;
    if(stride*(size_t)sh>sprite_size)return;
    center_w=avp_host_wide_center_width(dh);
    if(!center_w||center_w>dw)return;
    if(palette_count>208u)palette_count=208u;
    center_left=((int64_t)dw-(int64_t)center_w)/2;
    /* Division is done once per native column/row, never per source pixel.
     * The same 4:3 presentation scale maps every frame's sprite and flash;
     * these arrays contain CLIPPED destination edges, including negative
     * Jaguar coordinates and sprite art beyond x=319. */
    for(int x=0;x<=sw;x++)
        xe[x]=avp_fp_wide_clip_edge(center_left+avp_fp_wide_ceil_div(
            ((int64_t)original_x+x)*(int64_t)center_w,320),dw);
    for(int y=0;y<=sh;y++)
        ye[y]=avp_fp_wide_clip_edge(avp_fp_wide_ceil_div(
            ((int64_t)original_y+y)*(int64_t)dh,228),dh);
    uint32_t colors[208];
    for(unsigned i=1;i<palette_count;i++)colors[i]=((uint32_t)palette[i][0]<<16)|((uint32_t)palette[i][1]<<8)|palette[i][2];
    for(int sy=0;sy<sh;sy++){
        unsigned y0=ye[sy],y1=ye[sy+1];
        if(y0==y1)continue;
        const uint8_t *src=sprite+(size_t)sy*stride;
        uint32_t *first=dst+(size_t)y0*dw;
        unsigned begin=0,end=0;
        for(int sx=0;sx<sw;sx++){
            unsigned x0=xe[sx],x1=xe[sx+1];
            if(x0==x1)continue;
            unsigned pi=four_bit?((sx&1)?src[sx/2]&15u:src[sx/2]>>4):src[sx];
            if(!pi||pi>=palette_count){
                if(end>begin){for(unsigned y=y0+1;y<y1;y++)memcpy(dst+(size_t)y*dw+begin,first+begin,(size_t)(end-begin)*sizeof(uint32_t));begin=end=0;}
                continue;
            }
            if(begin==end)begin=x0;
            end=x1;
            uint32_t c=colors[pi];
            for(unsigned x=x0;x<x1;x++)first[x]=c;
        }
        if(end>begin)for(unsigned y=y0+1;y<y1;y++)memcpy(dst+(size_t)y*dw+begin,first+begin,(size_t)(end-begin)*sizeof(uint32_t));
    }
}
#endif
