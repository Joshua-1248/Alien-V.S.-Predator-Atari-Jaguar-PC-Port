#include "pc/pc_input_buffer.h"
#include "pc/pc_axis_deadzone.h"
#include "joypad.h"
#include <assert.h>
int main(void)
{
    uint32_t prev=0xffffffffu,pending=0,pad=0xffffffffu;
    /* A short button tap between two 10 Hz ticks reaches the next tick once. */
    pad&=~(1u<<FIRE_A);
    avp_pc_buffer_poll(&prev,&pending,pad,1);
    pad|=(1u<<FIRE_A);
    avp_pc_buffer_poll(&prev,&pending,pad,1);
    assert((avp_pc_buffer_take(&pending,pad,1)&(1u<<FIRE_A))==0u);
    assert((avp_pc_buffer_take(&pending,pad,1)&(1u<<FIRE_A))!=0u);
    /* Held keys are still held; no repeating synthetic edge when released. */
    pad&=~(1u<<FIRE_B);
    avp_pc_buffer_poll(&prev,&pending,pad,1);
    assert((avp_pc_buffer_take(&pending,pad,1)&(1u<<FIRE_B))==0u);
    assert((avp_pc_buffer_take(&pending,pad,1)&(1u<<FIRE_B))==0u);
    pad=0xffffffffu;
    avp_pc_buffer_poll(&prev,&pending,pad,1);
    assert(avp_pc_buffer_take(&pending,pad,1)==pad);
    /* Disabling enhancement must not retain an old tap. */
    pad&=~(1u<<FIRE_A);
    avp_pc_buffer_poll(&prev,&pending,pad,1);
    pad=0xffffffffu;
    avp_pc_buffer_poll(&prev,&pending,pad,0);
    assert(avp_pc_buffer_take(&pending,pad,0)==pad);
    assert(avp_pc_axis_deadzone(8192,0.30f,0)==0.0f);
    assert(avp_pc_axis_deadzone(8192,0.20f,0)>0.24f);
    assert(avp_pc_axis_deadzone(-16384,0.20f,1)>0.49f);
    assert(avp_pc_axis_deadzone(-32768,0.95f,0)==-1.0f);
    return 0;
}
