#ifndef AVP_PC_ANALOG_AIM_H
#define AVP_PC_ANALOG_AIM_H
#include <stdint.h>
#include <math.h>
/* Host aim is a proportional deflection-to-angular-step conversion, not a
 * simulated turn-velocity accumulator. Neutral input returns zero immediately.
 * The retail digital turn path remains in MovePlayer and is unaffected. */
static inline int16_t avp_pc_analog_aim_step(float axis,float sensitivity,
                                            float speed,int maximum)
{
    float scaled,limit,step;
    if(!(axis>=-1.0f&&axis<=1.0f)||!(sensitivity>0.0f)||!(speed>0.0f)||maximum<=0)return 0;
    scaled=axis*sensitivity;
    if(scaled>1.0f)scaled=1.0f;
    if(scaled<-1.0f)scaled=-1.0f;
    limit=(float)maximum*speed;
    if(limit>32767.0f)limit=32767.0f;
    step=-scaled*limit;
    return (int16_t)(step>=0.0f?step+0.5f:step-0.5f);
}

/* Enhanced PC analog look is deliberately NOT an integer stepper.  A host
 * double keeps the continuous heading across simulation ticks; the original
 * 68000 angle is merely a 32-bit transport value for legacy consumers.  The
 * renderer consumes all 32 bits rather than discarding the fractional word.
 * The original 16-bit Jaguar stepping code is unchanged when disabled. */
typedef struct {
    double heading;                  /* [0, 65536) angle units, fractional */
    uint32_t published;              /* last 16.16 angle presented to CPU */
    int active;
} AvpPcContinuousHeading;
static inline void avp_pc_continuous_heading_reset(AvpPcContinuousHeading *h)
{
    if(h)h->active=0;
}
static inline uint32_t avp_pc_continuous_heading_tick(
    AvpPcContinuousHeading *h,uint32_t current,float axis,
    float sensitivity,float speed,int maximum)
{
    double delta,heading,word;
    if(!h)return current;
    /* An external angle change (teleport, load, script, digital turn, mouse)
     * always wins.  Do not restore a stale PC heading after it. */
    if(!h->active||h->published!=current){
        h->heading=(double)current/65536.0;
        h->published=current;
        h->active=1;
    }
    if(!isfinite(axis)||!isfinite(sensitivity)||!isfinite(speed)||
       axis < -1.0f||axis>1.0f||sensitivity<=0.0f||speed<=0.0f||maximum<=0)
        return current;
    delta=-(double)axis*(double)sensitivity*(double)speed*(double)maximum;
    heading=fmod(h->heading+delta,65536.0);
    if(heading<0.0)heading+=65536.0;
    h->heading=heading;
    /* 32-bit 16.16 angle precision (about 8.4e-8 degrees).  No whole-angle
     * step or cardinal snap occurs in this enhanced input path. */
    word=heading*65536.0;
    h->published=(uint32_t)(uint64_t)floor(word+0.5);
    return h->published;
}

/* Enhanced PC camera: distribute a tick's complete, signed stick rotation
 * across the 60-Hz display frames, even with general Smooth Presentation off.
 * The actual 10-Hz game result is untouched.  Use the INPUT delta rather than
 * the shortest circular distance: speeds above 180 degrees per tick must
 * rotate in the commanded direction, not backwards across the wrap. */
static inline int avp_pc_continuous_heading_frame(
    uint32_t before,uint32_t after,double tick_delta,double phase,
    uint32_t *angle)
{
    double end_heading,frame_heading;
    uint32_t expected;
    if(!angle||!isfinite(tick_delta)||!isfinite(phase) ||
       phase<0.0||phase>1.0)return 0;
    if(fabs(tick_delta)>65536.0)return 0;
    /* Match the game's unsigned 16.16 circular rounding, including negative
     * half-LSB turns; round(negative) and round(wrapped negative) differ. */
    end_heading=fmod((double)before/65536.0+tick_delta,65536.0);
    if(end_heading<0.0)end_heading+=65536.0;
    expected=(uint32_t)(uint64_t)floor(end_heading*65536.0+0.5);
    /* Allow one lowest fractional bit for the double accumulator's retained
     * sub-32-bit precision; larger disagreement indicates a scripted change. */
    if((int32_t)(expected-after)>1 || (int32_t)(expected-after)<-1)return 0;
    frame_heading=fmod((double)before/65536.0+tick_delta*phase,65536.0);
    if(frame_heading<0.0)frame_heading+=65536.0;
    *angle=(uint32_t)(uint64_t)floor(frame_heading*65536.0+0.5);
    return 1;
}

#endif
