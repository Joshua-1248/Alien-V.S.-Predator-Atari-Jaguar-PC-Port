#ifndef AVP_JAGUAR_AVP_GPU_HIRES_WALL_SAMPLE_H
#define AVP_JAGUAR_AVP_GPU_HIRES_WALL_SAMPLE_H

#include <stdint.h>
#include <math.h>

/* The retail clipping/door/window tests use a 128-column source index.
 * Only the PC image sampler recovers the sub-column fraction, so external
 * high-resolution artwork is not quantized back to 128 virtual texels. */
static inline int avp_gpu_hires_wall_x(int source_col,double ray_fraction,
                                       int reflected,int unreflect,unsigned width)
{
    double p,sub;
    int x;
    if(width==0u)return 0;
    if(source_col<0)source_col=0;
    if(source_col>127)source_col=127;
    if(ray_fraction<0.0)ray_fraction=0.0;
    if(ray_fraction>1.0)ray_fraction=1.0;
    p=ray_fraction*128.0;
    sub=ray_fraction>=1.0?0.999999999999: p-floor(p);
    if(reflected)sub=1.0-sub;
    if(unreflect)sub=1.0-sub;
    x=(int)(((double)source_col+sub)*(double)width/128.0);
    if(x<0)x=0;
    if(x>=(int)width)x=(int)width-1;
    return x;
}

/* Fixed-point host-row phase is continuous through the 128-source-row
 * boundaries. A row in a 1254-high replacement must not sample only one
 * out of every roughly ten available texture rows. */
static inline int avp_gpu_hires_wall_y(int screen_row,int wall_h,
                                       int source_y_shift,unsigned height)
{
    int64_t numerator,denominator,shifted;
    int y;
    if(height==0u||wall_h<=0)return 0;
    numerator=((int64_t)screen_row*2+1)*(int64_t)height;
    denominator=(int64_t)wall_h*2;
    shifted=(numerator*128+(int64_t)source_y_shift*(int64_t)height*denominator)/
            (denominator*128);
    if(shifted<0)return 0;
    if(shifted>=(int64_t)height)return (int)height-1;
    y=(int)shifted;
    return y;
}
#endif
