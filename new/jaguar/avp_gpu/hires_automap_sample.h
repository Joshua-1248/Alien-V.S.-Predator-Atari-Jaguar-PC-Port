#ifndef AVP_JAGUAR_AVP_GPU_HIRES_AUTOMAP_SAMPLE_H
#define AVP_JAGUAR_AVP_GPU_HIRES_AUTOMAP_SAMPLE_H

#include <stdint.h>
#include <math.h>

/* Inverse of the unchanged floor(pixel*native/host) HUD scale. */
static inline unsigned avp_gpu_hires_automap_edge(unsigned native_edge,
                                                  unsigned host,unsigned native)
{
    return (unsigned)(((uint64_t)native_edge*host+native-1u)/native);
}

/* Host-only fractional form of MAZE5.GAS's signed 16.16 blitter transform.
 * Integer sample_x/y give the same source address as the original Jaguar
 * avp_gpu_automap_sample(); intermediate pixel centres are now available
 * at 2x/4x/8x resolutions without quantizing to the native 320x228 HUD.
 * Read-only: exploration, mask bits, and camera state remain original. */
static inline void avp_gpu_hires_automap_sample(uint32_t xpos,uint32_t ypos,
    int32_t sine,int32_t cosine,unsigned width,unsigned height,
    double sample_x,double sample_y,int *source_x,int *source_y)
{
    double xs=(double)(int32_t)xpos*8.0+(double)height*cosine-(double)width*sine+
              sample_x*2.0*(double)sine-sample_y*2.0*(double)cosine;
    double ys=(double)(int32_t)ypos*8.0-(double)height*sine-(double)width*cosine+
              sample_x*2.0*(double)cosine+sample_y*2.0*(double)sine;
    if(source_x)*source_x=(int)floor(xs/65536.0);
    if(source_y)*source_y=(int)floor(ys/65536.0);
}


/* Q32.32 scanline increments avoid two trigonometric coordinate products
 * and two libm floor operations per display pixel. Only the first pixel of
 * each map row needs floating-point projection; the row itself is strictly
 * integer, allowing the automap at high host refresh rates. */
typedef struct AvpGpuHiresAutomapRow {
    int64_t x,y,step_x,step_y;
} AvpGpuHiresAutomapRow;

static inline void avp_gpu_hires_automap_row_begin(AvpGpuHiresAutomapRow *r,
    uint32_t xpos,uint32_t ypos,int32_t sine,int32_t cosine,
    unsigned width,unsigned height,double pixel_x,double pixel_y,double step)
{
    const double x=(double)(int32_t)xpos*8.0+(double)height*cosine-(double)width*sine+
                   pixel_x*2.0*(double)sine-pixel_y*2.0*(double)cosine;
    const double y=(double)(int32_t)ypos*8.0-(double)height*sine-(double)width*cosine+
                   pixel_x*2.0*(double)cosine+pixel_y*2.0*(double)sine;
    r->x=(int64_t)llround(x*65536.0);
    r->y=(int64_t)llround(y*65536.0);
    r->step_x=(int64_t)llround(step*2.0*(double)sine*65536.0);
    r->step_y=(int64_t)llround(step*2.0*(double)cosine*65536.0);
}
static inline int avp_gpu_hires_automap_floor32(int64_t n)
{
    const int64_t unit=INT64_C(4294967296);
    return (int)(n>=0?n/unit:-1-((-n-1)/unit));
}
static inline void avp_gpu_hires_automap_row_next(AvpGpuHiresAutomapRow *r,
                                                    int *src_x,int *src_y)
{
    if(src_x)*src_x=avp_gpu_hires_automap_floor32(r->x);
    if(src_y)*src_y=avp_gpu_hires_automap_floor32(r->y);
    r->x+=r->step_x;
    r->y+=r->step_y;
}

#endif
