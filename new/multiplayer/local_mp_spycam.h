#ifndef AVP_LOCAL_MP_SPYCAM_H
#define AVP_LOCAL_MP_SPYCAM_H
#include <stdint.h>
#include "local_mp.h"
#include "local_mp_input.h"
/* Viewport-owner identity remains separate from the viewed actor. Slots are
 * ordered P1..P4, then P5..P36 bots; the viewer's own slot stays in the ring.
 * Input is a rising edge, not a held key. */
static inline unsigned avp_mp_spycam_cycle(unsigned viewer,unsigned current,
                                           unsigned actors,AvpMpActorMask eligible,int direction)
{
    unsigned slot,step;
    if(viewer>=AVP_LOCAL_MP_MAX_PLAYERS||actors==0u||actors>AVP_LOCAL_MP_MAX_ACTORS||
       viewer>=actors||!direction)return viewer;
    if(current>=actors)current=viewer;
    eligible|=AVP_MP_ACTOR_BIT(viewer); /* Always return to your own character. */
    for(step=1u;step<=actors;step++){
        slot=(current+(direction>0?step:actors-(step%actors)))%actors;
        if(eligible&(AVP_MP_ACTOR_BIT(slot)))return slot;
    }
    return viewer;
}
/* A death does NOT make a subject ineligible: their world/actor remains
 * renderable through the original fade to black and respawn. This optional
 * helper only repairs a missing/disconnected subject; it is not invoked
 * automatically when a player dies. */
static inline unsigned avp_mp_spycam_retarget(unsigned viewer,unsigned current,
                                               unsigned actors,AvpMpActorMask eligible)
{
    if(current==viewer)return viewer;
    if(current<actors && (eligible&(AVP_MP_ACTOR_BIT(current))))return current;
    return avp_mp_spycam_cycle(viewer,current,actors,eligible,1);
}
static inline unsigned avp_mp_spycam_edge(uint32_t actions,uint32_t *held)
{
    const uint32_t mask=(1u<<AVP_MP_SPY_NEXT_PLAYER)|
                        (1u<<AVP_MP_SPY_PREVIOUS_PLAYER);
    uint32_t edge;
    if(!held)return 0u;
    edge=(actions&mask)&~*held;
    *held=actions&mask;
    return edge;
}
#endif
