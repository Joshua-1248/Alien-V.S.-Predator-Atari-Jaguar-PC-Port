#ifndef AVP_PC_LIVE_HEADING_H
#define AVP_PC_LIVE_HEADING_H
#include <math.h>
#include <stdint.h>

/* Opt-in host-clock joystick heading. The authoritative Jaguar game stays at
 * 10 Hz, but the PC enhancement samples turns as often as host input is polled.
 * Sample the PREVIOUS axis over elapsed time, so releases stop immediately at
 * their actual poll time. No per-tick angular quantization is introduced. */
typedef struct AvpPcLiveHeading {
    double angle;
    uint64_t last_ns;
    float previous_axis, previous_sensitivity, previous_speed;
    int previous_maximum;
    unsigned previous_sim_hz;
    int active;
} AvpPcLiveHeading;
static inline void avp_pc_live_heading_reset(AvpPcLiveHeading *h)
{
    if(h)h->active=0;
}
static inline uint32_t avp_pc_live_heading_word(const AvpPcLiveHeading *h)
{
    if(!h||!h->active)return 0;
    return (uint32_t)(uint64_t)floor(h->angle*65536.0+0.5);
}
static inline uint32_t avp_pc_live_heading_poll(
    AvpPcLiveHeading *h,uint32_t game_angle,uint64_t now,
    float axis,float sensitivity,float speed,int maximum,unsigned sim_hz)
{
    double seconds,delta;
    if(!h)return game_angle;
    if(!isfinite(axis)||!isfinite(sensitivity)||!isfinite(speed)||
       axis < -1.0f||axis >1.0f||sensitivity<=0.0f||speed<=0.0f||
       maximum<=0||sim_hz==0){h->active=0;return game_angle;}
    if(!h->active || now<h->last_ns){
        h->angle=(double)game_angle/65536.0;
        h->active=1;
    } else if(now>h->last_ns){
        seconds=(double)(now-h->last_ns)/1000000000.0;
        /* A pause/suspend must never turn a held stick for stale wall time. */
        if(seconds<=0.25){
            delta=-(double)h->previous_axis*
                (double)h->previous_sensitivity*(double)h->previous_speed*
                (double)h->previous_maximum*(double)h->previous_sim_hz*seconds;
            h->angle=fmod(h->angle+delta,65536.0);
            if(h->angle<0.0)h->angle+=65536.0;
        }
    }
    h->last_ns=now;
    h->previous_axis=axis;
    h->previous_sensitivity=sensitivity;
    h->previous_speed=speed;
    h->previous_maximum=maximum;
    h->previous_sim_hz=sim_hz;
    return avp_pc_live_heading_word(h);
}
#endif
