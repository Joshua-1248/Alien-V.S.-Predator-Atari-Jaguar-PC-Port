#include "pc/pc_analog_aim.h"
#include <assert.h>
int main(void){
    assert(avp_pc_analog_aim_step(0,1,1,1536)==0);
    assert(avp_pc_analog_aim_step(0.5f,1,1,1536)==-768);
    assert(avp_pc_analog_aim_step(-0.5f,1,1,1536)==768);
    assert(avp_pc_analog_aim_step(1,1,1,1536)==-1536);
    assert(avp_pc_analog_aim_step(0.5f,2,1,1536)==-1536);
    assert(avp_pc_analog_aim_step(0.5f,1,2,1536)==-1536);
    assert(avp_pc_analog_aim_step(0,3,3,1536)==0);
    assert(avp_pc_analog_aim_step(1,3,3,1536)==-4608);
    assert(avp_pc_analog_aim_step(-1,3,3,1536)==4608);
    return 0;
}
