#ifndef AVP_JAGUAR_HOST_MP_SPATIAL_AUDIO_H
#define AVP_JAGUAR_HOST_MP_SPATIAL_AUDIO_H
#include "jaguar/host/mp_waveform_proximity.h"
#include "../../../new/multiplayer/local_mp.h"
#include <math.h>
#include <stdint.h>

/* PC-only local-multiplayer DSP mix policy. Original Jaguar voice synthesis,
 * music, effects and enemy logic remain unchanged. The host has ONE stereo
 * output, so spatial sounds use the nearest audible on-screen viewpoint.
 * An AI-controlled P1-P4 viewport is still an audio listener: its owner must
 * hear its own effects without needing P1 to stand nearby. Headless P5-P16
 * bots remain positional relative to the available on-screen viewpoints. */
typedef struct AvpMpSpatialListener {
    unsigned active,level;
    int32_t x,y;
    uint32_t heading;
} AvpMpSpatialListener;
typedef struct AvpMpSpatialGain {
    unsigned left,right,listener;
} AvpMpSpatialGain;

/* Only presented split-screen viewports have their own audio listener.
 * Control ownership (human/bot) must NOT decide whether the screen hears.
 * Supplying an invalid/absent level leaves that viewport silent. */
static inline unsigned avp_mp_spatial_view_listener(unsigned viewport,
    unsigned presented_views,unsigned level)
{
    return viewport<4u && viewport<presented_views && level>=1u && level<=15u;
}

static inline AvpMpSpatialGain avp_mp_spatial_gain(unsigned owner,unsigned level,
    int32_t x,int32_t y,const AvpMpSpatialListener listeners[4])
{
    AvpMpSpatialGain out={0u,0u,4u};
    unsigned best=0;
    if(!listeners || owner>=AVP_LOCAL_MP_MAX_ACTORS || level<1u || level>15u)return out;
    for(unsigned i=0u;i<4u;i++)if(listeners[i].active){
        unsigned gain=avp_mp_waveform_gain(owner,i,(int)level,
            (int)listeners[i].level,x,y,listeners[i].x,listeners[i].y);
        /* If the bot itself owns a viewport, retain its original direct
         * feedback; otherwise choose the nearest audible human viewport. */
        if(gain && (out.listener==4u || gain>best || (i==owner && out.listener!=owner))){
            best=gain;out.listener=i;
        }
    }
    if(out.listener==4u)return out;
    if(out.listener==owner){out.left=out.right=256u;return out;}
    {
        const AvpMpSpatialListener *l=&listeners[out.listener];
        const double dx=((double)(int64_t)x-(double)(int64_t)l->x)/65536.0;
        const double dy=((double)(int64_t)y-(double)(int64_t)l->y)/65536.0;
        const double angle=-(double)((uint16_t)(l->heading>>16))*(6.28318530717958647692/65536.0);
        const double side=dx*(-sin(angle))+dy*cos(angle);
        double pan=side/8.0;
        if(pan>1.0)pan=1.0;
        if(pan< -1.0)pan= -1.0;
        out.left=(unsigned)((double)best*(pan>0.0?1.0-pan:1.0)+0.5);
        out.right=(unsigned)((double)best*(pan<0.0?1.0+pan:1.0)+0.5);
    }
    return out;
}
#endif
