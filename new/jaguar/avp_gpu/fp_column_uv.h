#ifndef AVP_GPU_FP_COLUMN_UV_H
#define AVP_GPU_FP_COLUMN_UV_H

#include <stddef.h>
#include <stdint.h>

/* Host-only first-person replacement texture sampling. For a single sprite
 * blit, horizontal UV depends on its output X column but not on Y. Preserve
 * exactly the old double-precision expression and evaluation order, storing
 * its result once per column before drawing the (possibly tall) sprite.
 * Callers must fall back to the original expression for oversized spans. */
enum { AVP_FP_COLUMN_UV_CACHE_MAX=4096u };
static inline int avp_gpu_fp_column_uv_fill(double *columns,size_t capacity,
                                            int x0,int x1,double left,
                                            double scale_x,int nx,int sw)
{
    const int64_t span=(int64_t)x1-(int64_t)x0;
    if(!columns||span<0||(uint64_t)span>(uint64_t)capacity||
       scale_x==0.0||sw==0)return 0;
    for(int x=x0;x<x1;x++)
        columns[x-x0]=(((double)x+0.5-left)/scale_x-nx)/sw;
    return 1;
}

#endif
