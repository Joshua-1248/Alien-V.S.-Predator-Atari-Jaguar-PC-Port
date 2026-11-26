#ifndef AVP_PC_AXIS_DEADZONE_H
#define AVP_PC_AXIS_DEADZONE_H
#include <math.h>
#include <stdint.h>
static inline float avp_pc_axis_deadzone(int16_t value,float deadzone,int invert)
{
    float f=(float)value/32767.0f;
    if(invert)f=-f;
    if(f>1.0f)f=1.0f;
    if(f< -1.0f)f=-1.0f;
    if(!isfinite(deadzone)||deadzone<0.0f)deadzone=0.0f;
    if(deadzone>0.95f)deadzone=0.95f;
    if(fabsf(f)<deadzone)return 0.0f;
    return f;
}
/* Only proportional gameplay uses this response. Keep the raw deadzone
 * helper for digital bindings, menus and Jaguar pad emulation. The inner
 * deadzone is removed continuously, rather than jumping to its edge value.
 * Blend linear and quadratic response for fine control without a second
 * artificial deadzone. Movement retains the R464 90% end-travel calibration;
 * unusually large deadzones use the remaining travel to the physical endpoint. */
static inline float avp_pc_axis_precision(float raw,float deadzone,int movement)
{
    if(!isfinite(raw))return 0.0f;
    if(!isfinite(deadzone)||deadzone<0.0f)deadzone=0.0f;
    if(deadzone>0.95f)deadzone=0.95f;
    const float end=movement && deadzone<0.90f ? 0.90f : 1.0f;
    const float magnitude=fabsf(raw);
    if(magnitude<=deadzone)return 0.0f;
    if(magnitude>=end)return copysignf(1.0f,raw);
    const float t=(magnitude-deadzone)/(end-deadzone);
    return copysignf(t*(0.25f+0.75f*t),raw);
}
static inline float avp_pc_axis_precision_sample(int16_t value,float deadzone,int invert,int movement)
{
    return avp_pc_axis_precision(avp_pc_axis_deadzone(value,deadzone,invert),deadzone,movement);
}
#endif
