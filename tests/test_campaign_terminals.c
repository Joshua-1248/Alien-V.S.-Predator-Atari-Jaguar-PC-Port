#include "computer.h"
#include "player.h"
#include "doors.h"
#include "avp_runtime.h"
#include <assert.h>

s16 player_energy,destruct_flag,launch_flag;
u8 acs_level;
static unsigned sfx_count;
static void sfx(void *user,unsigned id){(void)user;(void)id;sfx_count++;}
int main(void)
{
    AvpRuntimeOps ops={0};
    AvpComputerContent content={0};
    ops.play_sfx=sfx;avp_runtime_bind(&ops);
    content.allows_destruct=1;
    acs_level=9;player_energy=300;
    assert(!avp_computer_can_destruct(&content));
    assert(!avp_computer_launch_pod() && !launch_flag);
    assert(avp_computer_first_aid()==1 && player_energy==750);
    player_energy=300;acs_level=4;
    assert(avp_computer_first_aid()==1 && player_energy==500);
    acs_level=10;
    assert(avp_computer_can_destruct(&content));
    avp_computer_arm_destruct();
    assert(destruct_flag==1 && sfx_count==1);
    assert(!avp_computer_can_destruct(&content));
    assert(avp_computer_launch_pod() && launch_flag==1);
    avp_runtime_reset_ops();
    return 0;
}
