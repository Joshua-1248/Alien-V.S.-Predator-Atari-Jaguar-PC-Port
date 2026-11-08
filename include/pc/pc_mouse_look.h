#ifndef AVP_PC_MOUSE_LOOK_H
#define AVP_PC_MOUSE_LOOK_H
/* Pure host-side horizontal mouse look. Returns a normalized tick-scale
   aiming impulse; it never changes analog forward/strafe movement. */
#include <math.h>
#include <stdint.h>
static inline float avp_pc_mouse_turn(float dx,float sensitivity,float acceleration)
{
    if(!isfinite(dx)||!isfinite(sensitivity)||!isfinite(acceleration))return 0.0f;
    if(sensitivity<0.0f)sensitivity=0.0f;
    if(acceleration<0.0f)acceleration=0.0f;
    return (dx/120.0f)*sensitivity*(1.0f+acceleration*fabsf(dx)/120.0f);
}
/* Turn any finite mouse impulse directly into a Jaguar 16-bit angle delta.
 * Modulo reduction is intentional for a full circle, not a joystick-speed
 * clamp. Positive X follows the existing right-turn (negative angle) sign. */
static inline uint16_t avp_pc_mouse_angle_step(float motion,int maximum)
{
    double step,reduced,rounded;
    if(!isfinite(motion)||maximum<=0)return 0u;
    step=-(double)motion*(double)maximum;
    if(!isfinite(step))return 0u;
    reduced=fmod(step,65536.0);
    rounded=reduced>=0.0?floor(reduced+0.5):ceil(reduced-0.5);
    if(rounded<0.0)rounded+=65536.0;
    if(rounded>=65536.0)rounded-=65536.0;
    return (uint16_t)rounded;
}
/* The analog aim step accepts only [-1,1]. Fast mouse swipes previously
   exceeded that range and were discarded entirely instead of turning. */
static inline float avp_pc_mouse_axis(float dx,float sensitivity,float acceleration)
{
    float turn=avp_pc_mouse_turn(dx,sensitivity,acceleration);
    return turn>1.0f?1.0f:(turn< -1.0f?-1.0f:turn);
}
#endif
