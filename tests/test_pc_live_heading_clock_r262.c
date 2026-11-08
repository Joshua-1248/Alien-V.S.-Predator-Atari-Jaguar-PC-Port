#include "pc/pc_live_heading.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
int main(void)
{
    AvpPcLiveHeading h={0};
    const uint64_t t0=1000000000ull;
    uint32_t start=avp_pc_live_heading_poll(&h,0,t0,-1.0f,1,1,1536,10);
    assert(start==0);
    /* Physical poll every 2ms: the camera moves before the next 100ms tick. */
    uint32_t first=avp_pc_live_heading_poll(&h,0,t0+2000000ull,-1,1,1,1536,10);
    uint32_t second=avp_pc_live_heading_poll(&h,0,t0+4000000ull,-1,1,1,1536,10);
    assert(first>0&&second>first);
    assert(fabs(h.angle-61.44)<0.000001);
    /* Stick release: integrate only until release, then stop exactly there. */
    const uint32_t released=avp_pc_live_heading_poll(&h,0,t0+49000000ull,0,1,1,1536,10);
    assert(released>second);
    assert(avp_pc_live_heading_poll(&h,0,t0+96000000ull,0,1,1,1536,10)==released);
    assert(avp_pc_live_heading_poll(&h,0,t0+100000000ull,0,1,1,1536,10)==released);
    /* Native host clock is independent of the old integer-step counter. */
    assert((released&0xffffu)!=0);
    avp_pc_live_heading_reset(&h);
    /* Speed 5 x sensitivity 5 can move >180deg in one original tick.
     * Frequent host polls preserve the full signed turn direction. */
    avp_pc_live_heading_poll(&h,0,5000000000ull,-1,5,5,1536,10);
    uint32_t last=0;
    for(unsigned i=1;i<=10;i++){
        uint32_t a=avp_pc_live_heading_poll(&h,0,5000000000ull+10000000ull*i,-1,5,5,1536,10);
        assert(a!=last);last=a;
    }
    assert(fabs(h.angle-38400.0)<0.000001);
    /* Pause/suspend gap does not replay hundreds of milliseconds of input. */
    uint32_t before=avp_pc_live_heading_word(&h);
    assert(avp_pc_live_heading_poll(&h,0,6100000000ull,-1,5,5,1536,10)==before);
    avp_pc_live_heading_reset(&h);
    assert(avp_pc_live_heading_poll(&h,0xabcd0000u,7000000000ull,0,1,1,1536,10)==0xabcd0000u);
    puts("R262 host-rate continuous analog heading and instant release PASS");
    return 0;
}
