#include "pc/pc_mouse_look.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    assert(avp_pc_mouse_angle_step(0.0f,1536)==0);
    assert(avp_pc_mouse_angle_step(1.0f,1536)==(uint16_t)-1536);
    assert(avp_pc_mouse_angle_step(10.0f,1536)==(uint16_t)-15360);
    assert(avp_pc_mouse_angle_step(-10.0f,1536)==15360);
    assert(avp_pc_mouse_angle_step(100.0f,1536)==(uint16_t)-153600);
    assert(avp_pc_mouse_angle_step(1.0f,1536)!=avp_pc_mouse_angle_step(10.0f,1536));
    puts("R259 unlimited mouse delta PASS");return 0;
}
