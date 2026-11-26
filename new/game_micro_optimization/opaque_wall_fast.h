#ifndef AVP_MICRO_OPAQUE_WALL_FAST_H
#define AVP_MICRO_OPAQUE_WALL_FAST_H
#include <stddef.h>
#include <stdint.h>
#include "jaguar/avp_gpu/wall_raster_dda.h"
/* Opaque retail CRY walls only; coordinates and buffer extent are clipped
 * by the caller. No alpha, filter or replacement-texture assumptions leak
 * into the generic path. Depth comparisons keep the original <= rule. */
static inline void avp_micro_opaque_wall128(uint32_t *pixels,float *owners,
    unsigned stride,unsigned x,int y0,int y1,int base_y0,int wall_h,
    const uint16_t *texture,unsigned tx,int shift,int shade,
    const uint32_t rgb[65536],float depth)
{
    AvpWallRowDda rows=avp_wall_row_dda_begin(y0-base_y0,wall_h);
    size_t at=(size_t)y0*stride+x;
    int previous=-1;
    uint32_t color=0;
    for(int y=y0;y<y1;++y,at+=stride,avp_wall_row_dda_advance(&rows)){
        if(owners[at]<=depth)continue;
        owners[at]=depth;
        int ty=avp_wall_row_dda_value(&rows)+shift;
        if(ty<0)ty=0;
        if(ty>127)ty=127;
        if(ty!=previous){
            uint16_t c=texture[(unsigned)ty*128u+tx];
            int v=(int)(c&255u)+shade;
            if(v<0)v=0;
            if(v>255)v=255;
            color=rgb[(c&0xff00u)|(unsigned)v];
            previous=ty;
        }
        pixels[at]=color;
    }
}
/* Only for a single solid-wall span in a known-cleared column. */
static inline void avp_micro_opaque_wall128_fresh(uint32_t *pixels,float *owners,
    unsigned stride,unsigned x,int y0,int y1,int base_y0,int wall_h,
    const uint16_t *texture,unsigned tx,int shift,int shade,
    const uint32_t rgb[65536],float depth)
{
    AvpWallRowDda rows=avp_wall_row_dda_begin(y0-base_y0,wall_h);
    size_t at=(size_t)y0*stride+x;
    int previous=-1;
    uint32_t color=0;
    for(int y=y0;y<y1;++y,at+=stride,avp_wall_row_dda_advance(&rows)){
        owners[at]=depth;
        int ty=avp_wall_row_dda_value(&rows)+shift;
        if(ty<0)ty=0;
        if(ty>127)ty=127;
        if(ty!=previous){
            uint16_t c=texture[(unsigned)ty*128u+tx];
            int v=(int)(c&255u)+shade;
            if(v<0)v=0;
            if(v>255)v=255;
            color=rgb[(c&0xff00u)|(unsigned)v];
            previous=ty;
        }
        pixels[at]=color;
    }
}
#endif
