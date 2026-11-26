#ifndef AVP_JAGUAR_AVP_GPU_HIRES_HUD_DETAILS_H
#define AVP_JAGUAR_AVP_GPU_HIRES_HUD_DETAILS_H
#include <math.h>
/* R304 PC presentation helpers. Inputs are actual Jaguar HUD-space
 * contacts/samples; no new source objects or audio signals are generated. */
static inline void avp_gpu_hires_tracker_xy(double lateral,double forward,
                                            double *x,double *y)
{
    const double scale=(double)(((79-8)/2)*256/15)/256.0;
    if(x)*x=64.0+lateral*3.0*scale;
    if(y)*y=154.0-forward*3.0*scale;
}
/* Interpolate a single original HP2 row's real endpoint samples; 2/4/8
 * quantization levels provide more horizontal display positions, not new
 * audio measurements. detail 0 retains the native Jaguar raster elsewhere. */
static inline double avp_gpu_awa_detail_position(int start,int end,
                                                 double progress,unsigned detail)
{
    if(progress<0.0)progress=0.0;
    if(progress>1.0)progress=1.0;
    if(detail>3u)detail=3u;
    const double levels=(double)(1u<<detail);
    const double value=(double)start+((double)end-(double)start)*progress;
    return round(value*levels)/levels;
}
#endif
