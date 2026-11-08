#include "jaguar/avp_gpu/pred_scope_raster.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    u8 hud[320u*200u]={0},trace[AVP_PRED_SCOPE_W*AVP_PRED_SCOPE_H]={0};
    const size_t last=(AVP_PRED_SCOPE_H-1u)*AVP_PRED_SCOPE_W;
    hud[(AVP_PRED_SCOPE_Y+3u)*320u+AVP_PRED_SCOPE_X+3u]=0xe9;
    avp_pred_scope_initial(trace,hud);
    assert(trace[3u*AVP_PRED_SCOPE_W+3u]==0xe9);
    avp_pred_scope_step(trace,0,5,10);
    assert(trace[AVP_PRED_SCOPE_W+3u]==0xe9);  /* Authored wave moved up two rows. */
    assert(trace[3u*AVP_PRED_SCOPE_W+3u]==0);
    assert(trace[last+AVP_PRED_SCOPE_W/2+10u]==0xe9);
    assert(trace[last+AVP_PRED_SCOPE_W/2+11u]==0);
    assert(trace[(AVP_PRED_SCOPE_H-2u)*AVP_PRED_SCOPE_W+AVP_PRED_SCOPE_W/2u+5u]==0xe9);
    avp_pred_scope_step(trace,10,-100,100);
    assert(trace[last+AVP_PRED_SCOPE_W/2u+10u]==0xe9);
    assert(trace[last+AVP_PRED_SCOPE_W-1u]==0xe9);
    assert(trace[(AVP_PRED_SCOPE_H-3u)*AVP_PRED_SCOPE_W+AVP_PRED_SCOPE_W/2u+10u]==0xe9);
    /* Original HP2 second stroke starts at the interpolated midpoint;
     * R275 erroneously started it from old_meter (10). A short triangular
     * sequence makes the old raster visibly too wide on its second row. */
    avp_pred_scope_step(trace,0,5,-7);
    assert(trace[last+AVP_PRED_SCOPE_W/2u-7u]==AVP_PRED_SCOPE_INK);
    assert(trace[last+AVP_PRED_SCOPE_W/2u+5u]==AVP_PRED_SCOPE_INK);
    assert(trace[last+AVP_PRED_SCOPE_W/2u+6u]==0u);
    assert(trace[(AVP_PRED_SCOPE_H-2u)*AVP_PRED_SCOPE_W+AVP_PRED_SCOPE_W/2u+5u]==AVP_PRED_SCOPE_INK);
    /* Clear on next update to verify both consecutive segments. */
    puts("Predator retail HUD sound trace retained and updated at HP2 cadence");
    return 0;
}
