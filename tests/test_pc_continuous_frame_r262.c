#include "pc/pc_analog_aim.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
    uint32_t out=0, before=0x10000000u;
    AvpPcContinuousHeading heading={0};
    const float axis=-1.0f/1024.0f;
    uint32_t after=avp_pc_continuous_heading_tick(&heading,before,axis,1,1,1536);
    assert(after==before+0x00018000u);
    /* 60Hz view samples across a 10Hz simulation frame (smooth OFF). */
    uint32_t last=before;
    for(int i=0;i<=6;i++){
        assert(avp_pc_continuous_heading_frame(before,after,1.5,(double)i/6,&out));
        assert(out==before+(uint32_t)(i*0x00004000u));
        assert(out>=last);last=out;
    }
    assert(avp_pc_continuous_heading_frame(after,after,0.0,0.75,&out));
    assert(out==after); /* neutral: exact stop, no angle snap */
    /* Past 180 degrees: retain direction and magnitude, not shortest arc. */
    before=0xF0000000u;
    after=(uint32_t)((uint64_t)before+(uint64_t)40000u*65536u);
    assert(avp_pc_continuous_heading_frame(before,after,40000.0,0.5,&out));
    assert(out==before+20000u*65536u);
    /* Reverse and crossing wrap with fractional positioning. */
    before=0x00008000u;
    after=0xffff0000u;
    assert(avp_pc_continuous_heading_frame(before,after,-1.5,0.5,&out));
    assert(out==0xffffc000u);
    assert(!avp_pc_continuous_heading_frame(0,123u,0,0.5,&out));
    assert(!avp_pc_continuous_heading_frame(0,0,0,-0.1,&out));
    puts("R262 60Hz enhanced yaw and external snap guard PASS");
    return 0;
}
