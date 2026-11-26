#ifndef AVP_JAGUAR_FP_WIDE_BLIT_H
#define AVP_JAGUAR_FP_WIDE_BLIT_H

#include "jaguar/avp_gpu/host_widescreen.h"
#include <stddef.h>
#include <math.h>
#include <stdint.h>

/* Host-only extended first-person OP raster.  Jaguar's authored x/y offsets
 * still refer to the middle 320x228 picture.  Unlike a centered 4:3 scratch
 * framebuffer, this blit may paint the original weapon/flash art into the
 * EXTRA widescreen world columns.  Only the real output bounds clip it.
 * The HUD itself is still drawn from its unchanged centered 320-wide brush.
 * For each Jaguar source texel use the same nearest-neighbour inverse-scale
 * boundaries as overlay_scale_composite.h, extended beyond x=0..319. */
static inline int64_t avp_fp_wide_ceil_div(int64_t n,int64_t d)
{
    /* d is positive. C99 negative integer division truncates toward zero,
     * which is exactly ceil() for a negative numerator. */
    return n>=0 ? (n+d-1)/d : n/d;
}

static inline unsigned avp_fp_wide_clip_edge(int64_t e,unsigned limit)
{
    return (unsigned)(e<=0?0:e>=(int64_t)limit?limit:e);
}

static inline void avp_gpu_fp_wide_blit(uint32_t *dst,unsigned dw,unsigned dh,
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
    for(int sy=0;sy<sh;sy++){
        unsigned y0=ye[sy],y1=ye[sy+1];
        if(y0==y1)continue;
        for(int sx=0;sx<sw;sx++){
            uint8_t pi;
            size_t si=(size_t)sy*stride;
            unsigned x0=xe[sx],x1=xe[sx+1];
            if(x0==x1)continue;
            if(four_bit){
                uint8_t b=sprite[si+(unsigned)sx/2u];
                pi=(uint8_t)((sx&1)?(b&15u):(b>>4));
            }else pi=sprite[si+(unsigned)sx];
            if(!pi||pi>=palette_count)continue;
            uint32_t color=((uint32_t)palette[pi][0]<<16)|
                           ((uint32_t)palette[pi][1]<<8)|palette[pi][2];
            for(unsigned y=y0;y<y1;y++){
                uint32_t *row=dst+(size_t)y*dw;
                for(unsigned x=x0;x<x1;x++)row[x]=color;
            }
        }
    }
}

/* Filtering opt-in: interpolate the original indexed OP sprite and its
 * transparency in source space. No screen-space blur or opaque black fringe. */
static inline void avp_gpu_fp_wide_blit_bilinear(uint32_t *dst,unsigned dw,unsigned dh,
    const uint8_t *sprite,size_t sprite_size,int sw,int sh,
    const uint8_t palette[208][3],unsigned palette_count,
    int original_x,int original_y,int four_bit)
{
    if(!dst||!dw||!dh||!sprite||!palette||sw<=0||sh<=0||sw>320||sh>240)return;
    size_t stride=four_bit?((size_t)sw+1u)/2u:(size_t)sw;
    if(stride*(size_t)sh>sprite_size)return;
    unsigned center_w=avp_host_wide_center_width(dh);
    if(!center_w||center_w>dw)return;
    if(palette_count>208u)palette_count=208u;
    double left=((double)dw-(double)center_w)*0.5;
    int x0=(int)floor(left+(double)original_x*center_w/320.0);
    int x1=(int)ceil(left+(double)(original_x+sw)*center_w/320.0);
    int y0=(int)floor((double)original_y*dh/228.0);
    int y1=(int)ceil((double)(original_y+sh)*dh/228.0);
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>(int)dw)x1=(int)dw;
    if(y1>(int)dh)y1=(int)dh;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
        double tx=((double)x+0.5-left)*320.0/center_w-original_x-0.5;
        double ty=((double)y+0.5)*228.0/dh-original_y-0.5;
        int sx=(int)floor(tx),sy=(int)floor(ty);
        double fx=tx-sx,fy=ty-sy;
        double ac=0,rc=0,gc=0,bc=0;
        for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
            int xi=sx+dx,yi=sy+dy;
            if(xi<0||xi>=sw||yi<0||yi>=sh)continue;
            const uint8_t *row=sprite+(size_t)yi*stride;
            uint8_t idx=four_bit?(uint8_t)((xi&1)?(row[xi/2]&15u):(row[xi/2]>>4)):
                             row[xi];
            if(!idx||idx>=palette_count)continue;
            double wt=(dx?fx:1.0-fx)*(dy?fy:1.0-fy);
            ac+=wt;rc+=wt*palette[idx][0];gc+=wt*palette[idx][1];bc+=wt*palette[idx][2];
        }
        if(ac<=0.0)continue;
        size_t at=(size_t)y*dw+(unsigned)x;
        uint32_t bg=dst[at];
        unsigned a=(unsigned)(ac*255.0+0.5);
        unsigned r=(unsigned)(rc/ac+0.5),g=(unsigned)(gc/ac+0.5),b=(unsigned)(bc/ac+0.5);
        dst[at]=((r*a+((bg>>16)&255u)*(255u-a)+127u)/255u<<16u)|
                ((g*a+((bg>>8)&255u)*(255u-a)+127u)/255u<<8u)|
                ((b*a+(bg&255u)*(255u-a)+127u)/255u);
    }
}

#endif
