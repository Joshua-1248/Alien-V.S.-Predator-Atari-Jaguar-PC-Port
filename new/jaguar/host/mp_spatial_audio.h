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
    unsigned subject_plus_one; /* 0=legacy viewport owner, otherwise observed slot+1 */
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
        const unsigned subject=listeners[i].subject_plus_one ?
                               listeners[i].subject_plus_one-1u:i;
        const unsigned direct=owner==subject && level==listeners[i].level;
        const unsigned gain=direct?256u:avp_mp_waveform_gain(owner,subject,(int)level,
            (int)listeners[i].level,x,y,listeners[i].x,listeners[i].y);
        /* A spy target is the center of THAT viewport's sound, even when
         * subject is a headless bot. A matching source beats a distant
         * observer, but ordinary split-screen listeners remain in the mix. */
        const unsigned previous=out.listener<4u &&
            (listeners[out.listener].subject_plus_one ?
             listeners[out.listener].subject_plus_one-1u:out.listener)==owner;
        if(gain && (out.listener==4u || (direct && !previous) ||
                    (!previous && gain>best))){
            best=gain;out.listener=i;
        }
    }
    if(out.listener==4u)return out;
    if((listeners[out.listener].subject_plus_one ?
        listeners[out.listener].subject_plus_one-1u:out.listener)==owner){
        out.left=out.right=256u;return out;
    }
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
/* Enemy and world-object SFX have their OWN original coordinates, not a
 * player identity. Never give them a self-owner unity gain just because a
 * player happened to be the enemy AI target. Match the nearest presented
 * same-level viewpoint and pan from that viewpoint's original heading. */
static inline AvpMpSpatialGain avp_mp_spatial_world_gain(unsigned level,
    int32_t x,int32_t y,const AvpMpSpatialListener listeners[4])
{
    AvpMpSpatialGain out={0u,0u,4u};
    unsigned best=0u;
    if(!listeners || level<1u || level>15u)return out;
    for(unsigned i=0u;i<4u;i++)if(listeners[i].active && listeners[i].level==level){
        const int64_t dx=(int64_t)x-listeners[i].x;
        const int64_t dy=(int64_t)y-listeners[i].y;
        const uint64_t ax=(uint64_t)(dx<0?-dx:dx);
        const uint64_t ay=(uint64_t)(dy<0?-dy:dy);
        const uint64_t radius=12ull<<16;
        const uint64_t d=ax>ay?ax:ay;
        const unsigned gain=d>=radius?0u:(unsigned)((radius-d)*256ull/radius);
        if(gain && (out.listener==4u || gain>best)){
            best=gain;
            out.listener=i;
        }
    }
    if(out.listener<4u){
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
/* A player stepping on acid is first-person damage feedback, NOT a
 * world effect that can bleed into another level or unrelated viewport.
 * Match the presented subject (including spycam) rather than input owner.
 * The shared host stereo output still contains a single combined signal. */
static inline AvpMpSpatialGain avp_mp_spatial_personal_gain(unsigned owner,
    unsigned level,const AvpMpSpatialListener listeners[4])
{
    AvpMpSpatialGain out={0u,0u,4u};
    if(!listeners || owner>=AVP_LOCAL_MP_MAX_ACTORS || level<1u || level>15u)
        return out;
    for(unsigned i=0;i<4u;i++)if(listeners[i].active && listeners[i].level==level){
        const unsigned subject=listeners[i].subject_plus_one ?
                               listeners[i].subject_plus_one-1u:i;
        if(subject==owner){
            out.left=out.right=256u;
            out.listener=i;
            return out;
        }
    }
    return out;
}
#endif
