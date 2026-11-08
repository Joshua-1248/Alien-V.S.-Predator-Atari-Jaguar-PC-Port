/* Production PLAYER.S move path: unlimited PC mouse and enhanced-stick stop. */
#include "player.h"
#include "maze.h"
#include "joypad.h"
#include "avp_runtime.h"
#include "mazescrn.h"
#include "doors.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>
volatile u32 joy_cur=0xffffffffu,joy_edge=0xffffffffu,joy2_cur=0xffffffffu,joy2_edge=0xffffffffu;
u32 x_pos=0,y_pos=0,centre_angle=0;
s32 sin_ang=0,cos_ang=0;
s16 hugkill=0;
u8 lift_key=0;
u8 SafePos(u8 height,s32 xpos,s32 ypos,u16 width)
{(void)height;(void)xpos;(void)ypos;(void)width;return 0;}
int AMPCollisions(s32 xpos,s32 ypos){(void)xpos;(void)ypos;return 0;}
static const AvpRuntimeOps empty_ops={0};
const AvpRuntimeOps *avp_runtime_ops(void){return &empty_ops;}
int main(void)
{
    player_type=PT_HUMAN;
    ang_vel=0;ang_acc=1536;max_avel=1536;
    avp_player_host_analog_motion(0,0,0);
    centre_angle=0x05000000u;
    avp_player_host_analog_aim(1,0,1,1);
    avp_player_host_aim_options(0,0);
    MovePlayer();
    assert((centre_angle>>16)==0); /* retail snap */
    centre_angle=0x05000000u;
    avp_player_host_aim_options(1,0);
    MovePlayer();
    assert((centre_angle>>16)==0x0500); /* enhanced stick neutral preserves stop */
    centre_angle=0;
    avp_player_host_analog_aim(1,10.0f,1,1);
    avp_player_host_aim_options(0,1);
    MovePlayer();
    assert((centre_angle>>16)==(u16)-15360);
    assert(ang_vel==0); /* mouse impulse is not a held joystick velocity */
    centre_angle=0;
    avp_player_host_analog_aim(1,-10.0f,1,1);
    avp_player_host_aim_options(0,1);
    MovePlayer();
    assert((centre_angle>>16)==15360);
    /* With enhanced analog active, a 1.5-angle-unit stick movement may stop
     * between the Jaguar's whole 16-bit heading values. */
    joy_cur=0xffffffffu;
    centre_angle=0;
    avp_player_host_aim_options(1,0);
    avp_player_host_analog_aim(1,-1.0f/1024.0f,1,1);
    MovePlayer();
    assert(centre_angle==0x00018000u);
    avp_player_host_analog_aim(1,0,1,1);
    for(int i=0;i<12;i++)MovePlayer();
    assert(centre_angle==0x00018000u); /* stop anywhere without snapping */
    /* A turn smaller than a single whole unit must move too, not vanish. */
    centre_angle=0;
    avp_player_host_analog_aim(1,-1.0f/16384.0f,1,1);
    MovePlayer();
    assert((centre_angle>>16)==0 && (centre_angle&65535u)!=0);
    const u32 tiny=centre_angle;
    avp_player_host_analog_aim(1,0,1,1);
    MovePlayer();
    assert(centre_angle==tiny);
    /* Crossing full-circle wrap works without a discontinuity. */
    centre_angle=0xffff0000u;
    avp_player_host_analog_aim(1,-1.0f/1024.0f,1,1);
    MovePlayer();
    assert(centre_angle==0x00008000u);
    /* External angle edits always win over the host accumulator. */
    centre_angle=0x12340000u;
    avp_player_host_analog_aim(1,0,1,1);
    MovePlayer();
    assert(centre_angle==0x12340000u);
    /* Disabled enhancement retains the original quantized path. */
    centre_angle=0x05000000u;
    avp_player_host_aim_options(0,0);
    MovePlayer();
    assert(centre_angle>>16==0);
    puts("R261 continuous enhanced turn and legacy parity PASS");
    return 0;
}
