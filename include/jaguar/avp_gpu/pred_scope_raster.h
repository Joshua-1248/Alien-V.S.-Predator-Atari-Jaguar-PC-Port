#ifndef AVP_JAGUAR_AVP_GPU_PRED_SCOPE_RASTER_H
#define AVP_JAGUAR_AVP_GPU_PRED_SCOPE_RASTER_H

#include "avp_types.h"
#include <string.h>

/* The retail Predator HUD's sound trace occupies x=27..69, y=149..193 in
 * the 320x200 FILES.DAT HUD brush (file 187).  HUD.S::HP2 emits TWO new
 * scan rows per 10 Hz simulation update.  Keep the existing rows and shift
 * them upwards instead of reconstructing the original static HUD artwork on
 * every host video frame.  All pixels are the original 8-bit HUD indices. */
enum { AVP_PRED_SCOPE_X=27, AVP_PRED_SCOPE_Y=149,
       AVP_PRED_SCOPE_W=43, AVP_PRED_SCOPE_H=45,
       AVP_PRED_SCOPE_INK=0xe9 };

/* Only Predator HUDs carry the original audio waveform analyzer. */
static inline int avp_mp_scope_visible(unsigned species)
{
    return species==2u;
}
static inline int avp_mp_scope_origin_x(unsigned species)
{
    (void)species;
    return AVP_PRED_SCOPE_X;
}

static inline void avp_pred_scope_initial(u8 *dst,const u8 *retail_hud)
{
    if(!dst||!retail_hud)return;
    for(unsigned y=0;y<AVP_PRED_SCOPE_H;y++)
        memcpy(dst+y*AVP_PRED_SCOPE_W,
               retail_hud+(AVP_PRED_SCOPE_Y+y)*320u+AVP_PRED_SCOPE_X,
               AVP_PRED_SCOPE_W);
}

static inline int avp_pred_scope_clamp(int v)
{
    const int lim=AVP_PRED_SCOPE_W/2;
    return v<-lim?-lim:(v>lim?lim:v);
}

static inline void avp_pred_scope_step(u8 *dst,int previous,int midpoint,int next)
{
    if(!dst)return;
    /* The new row is appended at the bottom, the older 43 rows advance
     * upward two lines.  Ignore host render FPS; advance only on HP2. */
    memmove(dst,dst+2u*AVP_PRED_SCOPE_W,
            (AVP_PRED_SCOPE_H-2u)*AVP_PRED_SCOPE_W);
    memset(dst+(AVP_PRED_SCOPE_H-2u)*AVP_PRED_SCOPE_W,0,
           2u*AVP_PRED_SCOPE_W);
    previous=avp_pred_scope_clamp(previous);
    midpoint=avp_pred_scope_clamp(midpoint);
    next=avp_pred_scope_clamp(next);
    for(unsigned row=0;row<2u;row++){
        /* HUD.S::HP2 copies d5 (the smoothed middle sample) into d4
         * before drawing its second row.  Both spans must not begin at
         * the previous sample: that makes each update needlessly wide. */
        int start=row==0u?previous:midpoint;
        int end=row==0u?midpoint:next;
        int lo=start<end?start:end;
        int hi=start>end?start:end;
        for(int x=lo;x<=hi;x++)
            dst[(AVP_PRED_SCOPE_H-2u+row)*AVP_PRED_SCOPE_W+
                (AVP_PRED_SCOPE_W/2+x)]=AVP_PRED_SCOPE_INK;
    }
}
#endif
