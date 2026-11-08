#ifndef AVP_PC_TURN_PRESENTATION_H
#define AVP_PC_TURN_PRESENTATION_H
#include <stdint.h>

/* Presentation-only prediction of the NEXT 10-Hz keyboard turn sample.
 * The Jaguar MovePlayer calculation remains authoritative; this uses its
 * signed acceleration, reversal and cardinal snap rules to fill the host
 * display frames after the last completed tick.  Never writes game state. */
static inline int avp_pc_digital_turn_display(uint32_t heading,int16_t velocity,
    int16_t acceleration,int16_t maximum,int direction,double phase,
    uint32_t *display_heading)
{
    int requested,v,hi,delta;
    if(!display_heading||!direction||acceleration<=0||maximum<=0||
       phase<0.0||phase>1.0)return 0;
    requested=direction>0?-(int)acceleration:(int)acceleration;
    v=(int)velocity;
    if(v && ((v<0)!=(requested<0)))v=0;
    v+=requested;
    if(v>maximum)v=maximum;
    if(v<-(int)maximum)v=-(int)maximum;
    hi=(int)(uint16_t)((heading>>16)+(uint16_t)v);
    if((hi&0x3fff)<acceleration)hi&=0xc000;
    delta=(int)(int16_t)((uint16_t)hi-(uint16_t)(heading>>16));
    *display_heading=heading+(uint32_t)(int32_t)((double)delta*65536.0*phase);
    return 1;
}

/* Host-only visual paths must never bypass a disabled Smooth Presentation.
 * Quick Turn is a separately enabled, explicitly animated action. */
static inline int avp_pc_turn_visual_override_allowed(int smooth,int active)
{
    return smooth && active;
}
#endif
