/* R260: mouse precision is preserved on the idle tick after a small swipe.
 * Original Jaguar-pad and digital turning still use the retail snap rule. */
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
s16 hugkill=0;u8 lift_key=0;
u8 SafePos(u8 height,s32 xpos,s32 ypos,u16 width)
{(void)height;(void)xpos;(void)ypos;(void)width;return 0;}
int AMPCollisions(s32 xpos,s32 ypos)
{(void)xpos;(void)ypos;return 0;}
static const AvpRuntimeOps empty_ops={0};
const AvpRuntimeOps *avp_runtime_ops(void){return &empty_ops;}
int main(void)
{
    player_type=PT_HUMAN;ang_vel=0;ang_acc=1536;max_avel=1536;
    avp_player_host_analog_motion(0,0,0);
    centre_angle=0;
    avp_player_host_analog_aim(1,-0.1f,1,1);
    avp_player_host_aim_options(0,1);
    MovePlayer();
    assert((centre_angle>>16)==154u); /* Small mouse impulse. */
    avp_player_host_analog_aim(1,0.0f,1,1);
    avp_player_host_aim_options(0,1); /* Mouse grabbed, no current delta. */
    for(int i=0;i<5;i++)MovePlayer();
    assert((centre_angle>>16)==154u); /* No delayed cardinal snap. */

    /* Disable the mouse: return to untouched retail stopping semantics. */
    avp_player_host_aim_options(0,0);
    MovePlayer();
    assert((centre_angle>>16)==0u);

    /* An active digital turn must still follow retail snapping semantics. */
    centre_angle=2000u<<16;
    joy_cur=0xffffffffu&~(1u<<FIRE_C)&~(1u<<JOY_RIGHT);
    avp_player_host_aim_options(0,1);
    MovePlayer();
    assert((centre_angle>>16)==0u); /* 2000 - 1536 = 464, which retains the retail snap. */
    puts("R260 mouse idle precision and Jaguar mode PASS");
    return 0;
}
