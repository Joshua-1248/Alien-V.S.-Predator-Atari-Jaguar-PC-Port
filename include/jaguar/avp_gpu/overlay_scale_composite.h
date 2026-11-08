#ifndef AVP_GPU_OVERLAY_SCALE_COMPOSITE_H
#define AVP_GPU_OVERLAY_SCALE_COMPOSITE_H

#include <stdint.h>
#include <stddef.h>

/* Composite only the native OP pixels changed by the first-person overlay.
 * The outgoing surface uses the existing inverse (floor) nearest-neighbour
 * map: native_x = floor(x * 320 / width), native_y = floor(y * 228 / height).
 * Each native texel therefore covers a disjoint rectangle bounded by ceil
 * of its two scaled source edges. The previous full-screen per-pixel compare
 * has exactly the same result, including at noninteger scales and below the
 * Jaguar's native size; only the host compositor's traversal changes.
 * Neither original OP imagery nor 68000 simulation is touched. */
static inline void avp_gpu_overlay_scaled_composite(uint32_t *pixels,
                                                     unsigned width,unsigned height,
                                                     const uint32_t *before,
                                                     const uint32_t *after)
{
    unsigned xe[321],ye[229];
    unsigned x,y;
    if(!pixels||!before||!after||!width||!height)return;
    for(x=0;x<=320u;x++)
        xe[x]=(unsigned)(((uint64_t)x*width+319u)/320u);
    for(y=0;y<=228u;y++)
        ye[y]=(unsigned)(((uint64_t)y*height+227u)/228u);
    for(y=0;y<228u;y++){
        unsigned sy0=ye[y],sy1=ye[y+1u];
        if(sy0==sy1)continue;
        for(x=0;x<320u;x++){
            size_t i=(size_t)y*320u+x;
            unsigned sx0,sx1,yy,xx;
            uint32_t colour=after[i];
            if(before[i]==colour)continue;
            sx0=xe[x];sx1=xe[x+1u];
            for(yy=sy0;yy<sy1;yy++){
                uint32_t *row=pixels+(size_t)yy*width;
                for(xx=sx0;xx<sx1;xx++)row[xx]=colour;
            }
        }
    }
}

#endif
