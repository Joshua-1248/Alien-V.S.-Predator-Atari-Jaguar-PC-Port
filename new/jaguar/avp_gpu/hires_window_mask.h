#ifndef AVP_JAGUAR_AVP_GPU_HIRES_WINDOW_MASK_H
#define AVP_JAGUAR_AVP_GPU_HIRES_WINDOW_MASK_H
#include <stdint.h>
#include <math.h>
/* The original authored pair list is 128 columns, with signed Y endpoints.
 * A high-resolution renderer can interpolate between two *already open*
 * neighboring columns. Never invent a new opening over a closed column. */
static inline void avp_gpu_hires_window_pair(const uint8_t pairs[256],
    int column,double subcolumn,int enabled,double *top,double *bottom)
{
    int a,b,next;
    if(column<0)column=0;
    if(column>127)column=127;
    a=(int)(int8_t)pairs[column*2];
    b=(int)(int8_t)pairs[column*2+1];
    *top=(double)a;*bottom=(double)b;
    if(!enabled || b<=a)return;
    if(subcolumn<0.0)subcolumn=0.0;
    if(subcolumn>1.0)subcolumn=1.0;
    /* Original 128-column samples are centred at column + 0.5. */
    if(subcolumn>=0.5){next=column+1;subcolumn-=0.5;}
    else{
        /* R316: from the left boundary toward the source-column centre,
         * influence from the PREVIOUS column must decrease from 1/2 to 0.
         * The old `subcolumn+=0.5` increased it toward 1, then jumped
         * instantly to zero at 0.5, producing a sawtooth (combing) mask. */
        next=column-1;
        subcolumn=0.5-subcolumn;
    }
    if(next<0||next>=128)return;
    {
        int na=(int)(int8_t)pairs[next*2];
        int nb=(int)(int8_t)pairs[next*2+1];
        if(nb<=na)return;
        *top=(double)a+((double)na-a)*subcolumn;
        *bottom=(double)b+((double)nb-b)*subcolumn;
    }
}
/* Existing Jaguar signed-Y edge formula exactly handles integer inputs.
 * Here only *fractional* edges are smoothly mapped into host pixels. */
static inline int avp_gpu_hires_edge_y(int base,int wall_height,double y)
{
    double top=(double)(wall_height/2),bottom=(double)(wall_height-wall_height/2);
    if(y<=-64.0)return base;
    if(y>=64.0)return base+wall_height;
    if(y<0.0)return base+(int)top-(int)ceil(-y*top/64.0);
    return base+(int)top+(int)ceil(y*bottom/64.0);
}
#endif
