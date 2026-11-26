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
    /* R283: source-authored medical panels exist on Levels 2 and 4.
     * General/armoury/pod panels must never be eligible for bot aid. */
    avp_computer_bind_retail_terminals();
    assert(avp_computer_medical_terminals(1u,NULL,0u)==0u);
    assert(avp_computer_medical_terminals(2u,NULL,0u)==4u);
    assert(avp_computer_medical_terminals(3u,NULL,0u)==0u);
    assert(avp_computer_medical_terminals(4u,NULL,0u)==7u);
    assert(avp_computer_medical_terminals(5u,NULL,0u)==0u);
    assert(avp_computer_is_medical_terminal(2u,4113u));
    assert(avp_computer_is_medical_terminal(4u,12873u));
    assert(!avp_computer_is_medical_terminal(2u,4200u));
    assert(!avp_computer_is_medical_terminal(5u,27275u));
    assert(!avp_computer_is_medical_terminal(1u,4113u));
    assert(avp_computer_first_aid_ceiling(3u)==0);
    assert(avp_computer_first_aid_ceiling(4u)==500);
    assert(avp_computer_first_aid_ceiling(7u)==750);
    assert(avp_computer_first_aid_ceiling(10u)==1000);
    assert(avp_computer_first_aid_ceiling(11u)==0);
    player_energy=550;acs_level=4;
    assert(avp_computer_first_aid()==0 && player_energy==550);
    acs_level=10;
    assert(avp_computer_can_destruct(&content));
    avp_computer_arm_destruct();
    assert(destruct_flag==1 && sfx_count==1);
    assert(!avp_computer_can_destruct(&content));
    assert(avp_computer_launch_pod() && launch_flag==1);
    avp_runtime_reset_ops();
    return 0;
}
