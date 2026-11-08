#include "pc/pc_analog_buffer.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    AvpPcAnalogBuffer b={0};float f=0,s=0,t=0;
    avp_pc_analog_buffer_poll(&b,0.25f,-0.5f,0.75f,1);
    avp_pc_analog_buffer_poll(&b,0,0,0,1);
    avp_pc_analog_buffer_take(&b,&f,&s,&t,1);
    assert(f==0.25f&&s==-0.5f&&t==0.75f);
    f=s=t=0;avp_pc_analog_buffer_take(&b,&f,&s,&t,1);
    assert(f==0&&s==0&&t==0); /* no stale replay */
    avp_pc_analog_buffer_poll(&b,-0.6f,0.2f,0.3f,1);
    f=0.8f;s=-0.4f;t=-0.9f;
    avp_pc_analog_buffer_take(&b,&f,&s,&t,1);
    assert(f==0.8f&&s==-0.4f&&t==-0.9f); /* live wins */
    avp_pc_analog_buffer_poll(&b,0.2f,0,0,1);
    avp_pc_analog_buffer_poll(&b,-0.8f,0,0,1);
    f=0;avp_pc_analog_buffer_take(&b,&f,&s,&t,1);
    assert(f==0.2f); /* first short pulse wins */
    avp_pc_analog_buffer_poll(&b,0.5f,0,0,1);
    avp_pc_analog_buffer_poll(&b,0,0,0,0);
    f=0;avp_pc_analog_buffer_take(&b,&f,&s,&t,1);assert(f==0);
    avp_pc_analog_buffer_poll(&b,0,0.9f,0,1);
    f=s=t=0;avp_pc_analog_buffer_take(&b,&f,&s,&t,0);
    assert(f==0&&s==0&&t==0);
    puts("R259 analog pulse buffer PASS");return 0;
}
