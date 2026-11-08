#include "jaguar/avp_gpu/heading_radians.h"
#include "pc/pc_analog_aim.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    AvpPcContinuousHeading heading={0};
    const uint32_t a=avp_pc_continuous_heading_tick(&heading,0,-1.0f/1024.0f,1,1,1536);
    assert(a==0x00018000u);
    /* Two locations in the old 16-bit heading bucket render as different
     * camera rotations ONLY in the PC enhancement. */
    assert(avp_gpu_heading_radians(0x00010000u,0)==
           avp_gpu_heading_radians(a,0));
    assert(avp_gpu_heading_radians(0x00010000u,1)!=
           avp_gpu_heading_radians(a,1));
    assert(fabs(avp_gpu_heading_radians(a,1)+1.5*(2.0*3.14159265358979323846/65536.0))<1e-14);
    /* Arbitrary sub-integer turn: enhanced renderer reacts immediately. */
    avp_pc_continuous_heading_reset(&heading);
    const uint32_t tiny=avp_pc_continuous_heading_tick(&heading,0,-1.0f/16384.0f,1,1,1536);
    assert(tiny>0 && tiny<65536u);
    assert(avp_gpu_heading_radians(tiny,1)!=0.0);
    assert(avp_gpu_heading_radians(tiny,0)==0.0);
    puts("R261 enhanced renderer uses high-precision continuous heading PASS");
    return 0;
}
