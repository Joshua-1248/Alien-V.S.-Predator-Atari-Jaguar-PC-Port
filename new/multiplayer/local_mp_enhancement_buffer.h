#ifndef AVP_LOCAL_MP_ENHANCEMENT_BUFFER_H
#define AVP_LOCAL_MP_ENHANCEMENT_BUFFER_H
/* Host-frame sampling for the EXISTING opt-in short-input enhancements.
 * Does not alter the original 10 Hz simulation or input when disabled.
 * P1 retains its old host buffer; only P2-P4 digital/analog pulses are
 * latched. Because high-rate sampling drains mouse deltas, preserve those
 * impulses for every player until the next actual simulation tick. */
#include "local_mp_commands.h"
#include "pc/pc_analog_buffer.h"
#include <stdint.h>
#include <limits.h>

typedef struct AvpMpEnhancementBuffer {
    uint32_t digital[AVP_LOCAL_MP_MAX_PLAYERS];
    AvpPcAnalogBuffer axes[AVP_LOCAL_MP_MAX_PLAYERS];
    int32_t mouse_dx[AVP_LOCAL_MP_MAX_PLAYERS];
    int32_t mouse_dy[AVP_LOCAL_MP_MAX_PLAYERS];
} AvpMpEnhancementBuffer;
static inline void avp_mp_enhancement_buffer_reset(AvpMpEnhancementBuffer *b)
{
    if(b){
        for(unsigned p=0;p<AVP_LOCAL_MP_MAX_PLAYERS;p++){
            b->digital[p]=0;
            avp_pc_analog_buffer_reset(&b->axes[p]);
            b->mouse_dx[p]=b->mouse_dy[p]=0;
        }
    }
}
static inline int32_t avp_mp_buffer_add_clamped(int32_t old,int32_t delta)
{
    int64_t n=(int64_t)old+(int64_t)delta;
    return n>INT32_MAX?INT32_MAX:n<INT32_MIN?INT32_MIN:(int32_t)n;
}
static inline void avp_mp_enhancement_buffer_poll(AvpMpEnhancementBuffer *b,
    const AvpMpPlayerCommand sample[AVP_LOCAL_MP_MAX_PLAYERS],unsigned players,
    int short_buttons,int short_analog)
{
    if(!b||!sample||players<1||players>AVP_LOCAL_MP_MAX_PLAYERS)return;
    for(unsigned p=0;p<players;p++){
        b->mouse_dx[p]=avp_mp_buffer_add_clamped(b->mouse_dx[p],sample[p].mouse_dx);
        b->mouse_dy[p]=avp_mp_buffer_add_clamped(b->mouse_dy[p],sample[p].mouse_dy);
        if(p==0)continue; /* P1 buffering is already in pc_video_x11.c. */
        if(short_buttons)b->digital[p]|=sample[p].digital_actions;
        avp_pc_analog_buffer_poll(&b->axes[p],sample[p].analog_forward,
            sample[p].analog_strafe,sample[p].analog_turn,
            short_analog&&sample[p].analog_enabled);
    }
}
static inline void avp_mp_enhancement_buffer_take(AvpMpEnhancementBuffer *b,
    AvpMpPlayerCommand tick[AVP_LOCAL_MP_MAX_PLAYERS],unsigned players,
    int short_buttons,int short_analog)
{
    if(!b||!tick||players<1||players>AVP_LOCAL_MP_MAX_PLAYERS)return;
    for(unsigned p=0;p<players;p++){
        tick[p].mouse_dx=avp_mp_buffer_add_clamped(b->mouse_dx[p],tick[p].mouse_dx);
        tick[p].mouse_dy=avp_mp_buffer_add_clamped(b->mouse_dy[p],tick[p].mouse_dy);
        if(p>0){
            if(short_buttons){
                tick[p].actions|=b->digital[p];
                tick[p].digital_actions|=b->digital[p];
            }
            avp_pc_analog_buffer_take(&b->axes[p],&tick[p].analog_forward,
                &tick[p].analog_strafe,&tick[p].analog_turn,
                short_analog&&tick[p].analog_enabled);
        }
    }
    avp_mp_enhancement_buffer_reset(b);
}
#endif
