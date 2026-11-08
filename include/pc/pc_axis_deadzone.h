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
#endif
