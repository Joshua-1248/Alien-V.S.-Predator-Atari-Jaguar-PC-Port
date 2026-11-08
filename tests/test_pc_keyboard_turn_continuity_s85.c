/* S85: original 10 Hz keyboard turns must render without a direction-change
 * discontinuity.  This tests the existing authoritative-pose interpolation;
 * the live-key lookahead is deliberately not part of the render path. */
#include <stdint.h>
#include <stdio.h>
#include "jaguar/avp_gpu/presentation_interp.h"
#define CHECK(x) do { if(!(x))return __LINE__; } while(0)
static uint32_t rendered(uint32_t before,uint32_t after,double alpha)
{
    AvpPresentationPose p={0x10000u,0x10000u,before};
    AvpPresentationPose c={0x10000u,0x10000u,after};
    AvpPresentationPose r={0};
    if(!avp_presentation_interpolate_pose(&p,&c,alpha,&r))return 0xffffffffu;
    return r.angle;
}
int main(void)
{
    const uint32_t a=0x20000000u,b=0x1d000000u,c=0x17000000u;
    uint32_t prev=a;
    /* Phase 0 at the next tick must equal phase 1 from the prior tick. */
    CHECK(rendered(a,b,1.0)==b);
    CHECK(rendered(b,c,0.0)==b);
    /* Press/release transitions mid-interval do not alter rendered yaw: the
     * world sim samples the held keyboard state only on the next 10 Hz tick. */
    for(int i=0;i<=6;i++){
        double phase=(double)i/6.0;
        uint32_t cur=rendered(a,b,phase);
        CHECK(cur<=prev); /* monotonic right turning with no backward jump */
        prev=cur;
    }
    CHECK(rendered(a,b,0.5)==0x1e800000u);
    CHECK(rendered(b,c,1.0)==c);
    CHECK(rendered(c,c,0.5)==c); /* held key released: no retrograde yaw */
    puts("S85 keyboard 10 Hz authoritative yaw presentation continuity PASS");
    return 0;
}
