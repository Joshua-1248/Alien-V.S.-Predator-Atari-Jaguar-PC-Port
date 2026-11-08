#include "pc/pc_mouse_look.h"
#include <assert.h>
int main(void){
    assert(avp_pc_mouse_turn(0,1,0)==0);
    assert(avp_pc_mouse_turn(120,1,0)==1);
    assert(avp_pc_mouse_turn(-120,1,0)==-1);
    assert(avp_pc_mouse_turn(120,2,0)==2);
    assert(avp_pc_mouse_turn(120,1,1)==2);
    assert(avp_pc_mouse_turn(120,1,2)==3);
    assert(avp_pc_mouse_turn(120,0.5f,0)==0.5f);
    assert(avp_pc_mouse_turn(0,3,2)==0);
    assert(avp_pc_mouse_axis(0,1,0)==0);
    assert(avp_pc_mouse_axis(120,1,0)==1);
    assert(avp_pc_mouse_axis(500,1,0)==1); /* no dead look on fast swipes */
    assert(avp_pc_mouse_axis(-500,1,0)==-1);
    assert(avp_pc_mouse_axis(80,1,0)>0.6f&&avp_pc_mouse_axis(80,1,0)<0.7f);
    return 0;
}
