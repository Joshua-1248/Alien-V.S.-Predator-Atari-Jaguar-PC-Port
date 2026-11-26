#include "jaguar/avp_gpu/hires_hud_details.h"
#include "jaguar/avp_gpu/pred_scope_raster.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
int main(void)
{
    double x,y,prev;
    avp_gpu_hires_tracker_xy(0,0,&x,&y);
    assert(x==64.0&&y==154.0);
    avp_gpu_hires_tracker_xy(1.01,2.02,&x,&y);
    assert(x>64.0&&x<72.0&&y<154.0&&y>139.0);
    assert(fabs(x-round(x))>0.02); /* true fractional position */
    avp_gpu_hires_tracker_xy(-1.01,-2.02,&prev,NULL);
    assert(fabs(prev-(128.0-x))<0.000001);
    /* 43-wide native AWA remains entirely unchanged by new host details. */
    u8 trace[AVP_PRED_SCOPE_W*AVP_PRED_SCOPE_H]={0};
    avp_pred_scope_step(trace,0,5,10);
    assert(trace[(AVP_PRED_SCOPE_H-1u)*AVP_PRED_SCOPE_W+AVP_PRED_SCOPE_W/2u+10u]==AVP_PRED_SCOPE_INK);
    assert(avp_gpu_awa_detail_position(0,5,0.5,1)==2.5);
    assert(avp_gpu_awa_detail_position(0,5,0.525,2)==2.75);
    assert(avp_gpu_awa_detail_position(0,5,0.525,3)==2.625);
    assert(avp_gpu_awa_detail_position(0,5,-1,3)==0.0);
    assert(avp_gpu_awa_detail_position(0,5,2,3)==5.0);
    assert(avp_gpu_awa_detail_position(-21,21,0.5,3)==0.0);
    puts("R304: fractional radar coordinates, AWA 2x/4x/8x display positions, original raster preserved");
    return 0;
}
