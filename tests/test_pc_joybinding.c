#include "pc/pc_joybinding.h"
#include <assert.h>
#include <limits.h>
int main(void)
{
    int16_t axes[3]={0,-30000,30000},prev[3]={0,0,30000};
    unsigned char buttons[4]={0,1,0,0},older[4]={0,0,0,0};
    assert(avp_pc_binding_down(1,axes,3,buttons,4));
    assert(avp_pc_binding_edge(1,axes,prev,3,buttons,older,4));
    assert(!avp_pc_binding_down(-1,axes,3,buttons,4));
    assert(!avp_pc_binding_down(INT_MIN,axes,3,buttons,4));
    assert(!avp_pc_binding_down(INT_MIN+1,axes,3,buttons,4));
    assert(!avp_pc_binding_edge(INT_MIN,axes,prev,3,buttons,older,4));
    assert(!avp_pc_binding_down(INT_MAX,axes,3,buttons,4));
    assert(avp_pc_binding_down(avp_pc_axis_button_token(1,0),axes,3,buttons,4));
    assert(!avp_pc_binding_down(avp_pc_axis_button_token(1,1),axes,3,buttons,4));
    assert(avp_pc_binding_edge(avp_pc_axis_button_token(1,0),axes,prev,3,buttons,older,4));
    assert(avp_pc_binding_down(avp_pc_axis_button_token(2,1),axes,3,buttons,4));
    assert(!avp_pc_binding_edge(avp_pc_axis_button_token(2,1),axes,prev,3,buttons,older,4));
    assert(!avp_pc_binding_down(avp_pc_axis_button_token(10,1),axes,3,buttons,4));
    axes[1]=7000;
    assert(!avp_pc_binding_down(avp_pc_axis_button_token(1,0),axes,3,buttons,4));
    return 0;
}
