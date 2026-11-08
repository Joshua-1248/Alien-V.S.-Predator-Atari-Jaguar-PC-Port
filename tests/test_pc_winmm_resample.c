#include "pc/pc_winmm_resample.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int source_position;
static void constant_source(int16_t *output,unsigned count)
{
    unsigned i;
    for(i=0;i<count;i++){
        output[i*2]=12345;
        output[i*2+1]=-23456;
        source_position++;
    }
}
static void ramp_source(int16_t *output,unsigned count)
{
    unsigned i;
    for(i=0;i<count;i++){
        int16_t v=(int16_t)(source_position-12000);
        output[i*2]=v;output[i*2+1]=-v;
        source_position++;
    }
}
static void check_constant(unsigned rate)
{
    AvpPcRateBridge s;
    int16_t output[1184*2];unsigned i;
    source_position=0;
    avp_pc_rate_bridge_init(&s,rate);
    for(i=0;i<20;i++){
        unsigned j;
        avp_pc_rate_bridge_render(&s,output,1184,constant_source);
        for(j=0;j<1184;j++){
            assert(output[2*j]==12345);
            assert(output[2*j+1]==-23456);
        }
    }
}
static void check_stream_continuity(unsigned rate)
{
    AvpPcRateBridge a,b;
    int16_t full[2400*2],split[2400*2];
    avp_pc_rate_bridge_init(&a,rate);
    source_position=0;
    avp_pc_rate_bridge_render(&a,full,2400,ramp_source);
    avp_pc_rate_bridge_init(&b,rate);
    source_position=0;
    avp_pc_rate_bridge_render(&b,split,512,ramp_source);
    avp_pc_rate_bridge_render(&b,split+512*2,1184,ramp_source);
    avp_pc_rate_bridge_render(&b,split+(512+1184)*2,704,ramp_source);
    assert(memcmp(full,split,sizeof(full))==0);
}
static void check_clock(unsigned rate)
{
    AvpPcRateBridge s;
    int16_t b[1200*2];
    unsigned i,j;
    avp_pc_rate_bridge_init(&s,rate);source_position=0;
    for(i=0;i<rate/1200;i++){
        avp_pc_rate_bridge_render(&s,b,1200,ramp_source);
        for(j=0;j<1199;j++)
            assert(b[j*2+2]-b[j*2]>=0 && b[j*2+2]-b[j*2]<=1);
    }
    /* 1 second of device output should advance FullSynth by ~20774 frames,
       independent of which Windows hardware output rate is selected. */
    assert(s.read_index<=AVP_WINMM_SOURCE_BLOCK);
    assert(source_position>=19000 && source_position<22000);
}
static void check_device_frame_clock(unsigned rate)
{
    AvpPcDeviceFrameClock clock;
    uint64_t total=0;
    unsigned i;
    avp_pc_device_frame_clock_init(&clock,rate);
    for(i=1;i<=100000u;i++){
        unsigned count=avp_pc_device_frame_clock_next(&clock);
        uint64_t expected=((uint64_t)i*AVP_WINMM_SOURCE_BLOCK*rate)/AVP_SYNTH_RATE;
        /* No interval may be off by more than a single output frame. */
        assert(count>=((uint64_t)AVP_WINMM_SOURCE_BLOCK*rate)/AVP_SYNTH_RATE);
        assert(count<=((uint64_t)AVP_WINMM_SOURCE_BLOCK*rate)/AVP_SYNTH_RATE+1u);
        total+=count;
        assert(total==expected);
        assert(clock.remainder<AVP_SYNTH_RATE);
    }
}
int main(void)
{
    check_constant(44100);check_constant(48000);
    check_stream_continuity(44100);check_stream_continuity(48000);
    check_clock(44100);check_clock(48000);
    check_device_frame_clock(20774);check_device_frame_clock(44100);check_device_frame_clock(48000);
    puts("PC WinMM source-clock bridge tests passed");return 0;
}
