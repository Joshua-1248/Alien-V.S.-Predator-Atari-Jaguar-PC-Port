#ifndef AVP_PC_ANALOG_BUFFER_H
#define AVP_PC_ANALOG_BUFFER_H
/* PC-only enhancement: preserve deadzone-filtered stick pulses sampled
 * between retail 10 Hz ticks.  Does not change the Jaguar simulation rate.
 * Current live input takes priority; only a released axis uses its short
 * pulse.  Each pending component is consumed once, and invalid contexts
 * clear all pending values.  No mouse input or button presses are stored. */
typedef struct AvpPcAnalogBuffer {
    float forward,strafe,turn;
} AvpPcAnalogBuffer;
static inline void avp_pc_analog_buffer_reset(AvpPcAnalogBuffer *b)
{
    b->forward=b->strafe=b->turn=0.0f;
}
static inline void avp_pc_analog_buffer_poll(AvpPcAnalogBuffer *b,
    float forward,float strafe,float turn,int enabled)
{
    if(!enabled){avp_pc_analog_buffer_reset(b);return;}
    if(forward!=0.0f&&b->forward==0.0f)b->forward=forward;
    if(strafe!=0.0f&&b->strafe==0.0f)b->strafe=strafe;
    if(turn!=0.0f&&b->turn==0.0f)b->turn=turn;
}
static inline void avp_pc_analog_buffer_take(AvpPcAnalogBuffer *b,
    float *forward,float *strafe,float *turn,int enabled)
{
    if(enabled){
        if(forward&&*forward==0.0f)*forward=b->forward;
        if(strafe&&*strafe==0.0f)*strafe=b->strafe;
        if(turn&&*turn==0.0f)*turn=b->turn;
    }
    avp_pc_analog_buffer_reset(b);
}
#endif
