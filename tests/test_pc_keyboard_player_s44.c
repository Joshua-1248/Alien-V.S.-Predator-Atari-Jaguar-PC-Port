/* Actual original MovePlayer fixed-point acceleration, with host-only
 * simultaneous keyboard strafe bridge; builds with production player.c. */
#define main old_pc_aim_regression
#include "test_pc_continuous_aim_r261.c"
#undef main
#include <assert.h>
#include <stdio.h>
static void check(int species,int turn_right,int strafe_dir)
{
    u32 pad=0xffffffffu;
    player_type=(s16)species;hugkill=0;lift_key=0;
    centre_angle=0;sin_ang=0;cos_ang=1024;
    x_vel=y_vel=0;ang_vel=0;ang_acc=1536;max_avel=1536;
    max_speed=1000000;acc_shift=0;dec_shift=2;
    avp_player_host_analog_motion(0,0,0);
    avp_player_host_analog_aim(0,0,1,1);
    avp_player_host_aim_options(0,0);
    avp_player_host_live_heading(0,0);
    avp_player_host_quick_turn_reset();
    pad&=~(1u<<(turn_right?JOY_RIGHT:JOY_LEFT));
    pad&=~(1u<<(species==PT_ALIEN?OPTION:FIRE_C));
    joy_cur=pad;
    avp_player_host_keyboard_strafe_during_turn(strafe_dir);
    MovePlayer();
    assert(ang_vel==(turn_right?-1536:1536));
    assert(x_vel==0);
    assert(y_vel==512*strafe_dir);
    avp_player_host_keyboard_strafe_during_turn(0);
    x_vel=y_vel=0;ang_vel=0;
    MovePlayer();
    assert(!x_vel&&!y_vel); /* no stuck sidestep after key release */
}
int main(void)
{
    assert(old_pc_aim_regression()==0);
    for(int species=0;species<=8;species+=4){
        check(species,1,-1); /* Q+D */
        check(species,0,+1); /* E+A */
        check(species,0,-1); /* Q+A */
        check(species,1,+1); /* E+D */
        check(species,1,0);  /* D alone */
    }
    puts("S44 player original yaw + simultaneous host keyboard sidestep PASS");
    return 0;
}
