#define main avp_r261_player_checks
#include "test_pc_continuous_aim_r261.c"
#undef main
#include "pc/pc_live_heading.h"
int main(void)
{
    assert(avp_r261_player_checks()==0);
    AvpPcLiveHeading h={0};
    const uint64_t t=9000000000ull;
    centre_angle=0;
    joy_cur=0xffffffffu;
    ang_vel=0;ang_acc=1536;max_avel=1536;
    avp_pc_live_heading_poll(&h,centre_angle,t,-0.125f,1,1,max_avel,10);
    uint32_t expected=avp_pc_live_heading_poll(&h,centre_angle,t+26000000ull,0,1,1,max_avel,10);
    assert(expected&& (expected&0xffffu));
    avp_player_host_analog_aim(1,0,1,1);
    avp_player_host_aim_options(1,0);
    avp_player_host_live_heading(1,expected);
    MovePlayer();
    assert(centre_angle==expected);
    avp_player_host_live_heading(0,0);
    /* Existing R261 10-Hz path is still usable without live gamepad input. */
    u32 before=centre_angle;
    avp_player_host_analog_aim(1,0,1,1);
    MovePlayer();assert(centre_angle==before);
    puts("R262 host angle applied once to the original player move PASS");
    return 0;
}
