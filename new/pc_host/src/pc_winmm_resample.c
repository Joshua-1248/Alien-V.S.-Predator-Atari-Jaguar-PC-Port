#include "pc/pc_winmm_resample.h"
#include <string.h>

void avp_pc_device_frame_clock_init(AvpPcDeviceFrameClock *clock,
                                    unsigned output_rate)
{
    clock->output_rate=output_rate;
    clock->remainder=0;
}

unsigned avp_pc_device_frame_clock_next(AvpPcDeviceFrameClock *clock)
{
    uint64_t frames=(uint64_t)AVP_WINMM_SOURCE_BLOCK*clock->output_rate+
                    clock->remainder;
    unsigned whole=(unsigned)(frames/AVP_SYNTH_RATE);
    clock->remainder=frames%AVP_SYNTH_RATE;
    return whole;
}

static void next_source(AvpPcRateBridge *s,int16_t out[2],AvpPcSourceMix mix)
{
    if(s->read_index>=AVP_WINMM_SOURCE_BLOCK){
        mix(s->input,AVP_WINMM_SOURCE_BLOCK);
        s->read_index=0;
    }
    out[0]=s->input[s->read_index*2u];
    out[1]=s->input[s->read_index*2u+1u];
    s->read_index++;
}

void avp_pc_rate_bridge_init(AvpPcRateBridge *s,unsigned output_rate)
{
    memset(s,0,sizeof(*s));
    s->read_index=AVP_WINMM_SOURCE_BLOCK;
    s->step=((uint64_t)AVP_SYNTH_RATE<<32)/output_rate;
}

void avp_pc_rate_bridge_render(AvpPcRateBridge *s,int16_t *out,
                               unsigned frames,AvpPcSourceMix mix)
{
    const uint64_t one=(uint64_t)1<<32;
    unsigned i;
    if(!s->initialized){
        next_source(s,s->previous,mix);
        next_source(s,s->next,mix);
        s->initialized=1;
    }
    for(i=0;i<frames;i++){
        unsigned channel;
        for(channel=0;channel<2;channel++){
            int64_t difference=(int32_t)s->next[channel]-(int32_t)s->previous[channel];
            int64_t scaled=(difference*(int64_t)s->phase)/(int64_t)one;
            out[i*2u+channel]=(int16_t)((int64_t)s->previous[channel]+scaled);
        }
        s->phase+=s->step;
        while(s->phase>=one){
            s->phase-=one;
            s->previous[0]=s->next[0];
            s->previous[1]=s->next[1];
            next_source(s,s->next,mix);
        }
    }
}
