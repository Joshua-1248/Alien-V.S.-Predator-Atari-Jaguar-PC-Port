/* Real production PLAYER.C: interpolated host yaw is exact at gameplay ticks,
 * preserves a neutral 180-degree endpoint and restores Jaguar digital control. */
#include "player.h"
#include "maze.h"
#include "joypad.h"
#include "avp_runtime.h"
#include "mazescrn.h"
#include "doors.h"
#include "collision.h"
#include "pc/pc_quick_turn.h"
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
    AvpPcQuickTurn t={0};
    const uint32_t source=UINT32_C(0x03001234);
    const uint64_t begin=UINT64_C(1000000000);
    player_type=PT_HUMAN;ang_vel=0;ang_acc=1536;max_avel=1536;
    avp_player_host_analog_motion(0,0,0);
    avp_player_host_analog_aim(0,0,1,1);
    avp_player_host_aim_options(0,0);
    avp_player_host_quick_turn_reset();
    centre_angle=source;
    avp_pc_quick_turn_enqueue(&t,centre_angle,begin,1);
    avp_player_host_quick_turn_heading(1,avp_pc_quick_turn_angle(&t,begin));
    MovePlayer();
    assert(centre_angle==source); /* NOT an instant snap. */
    avp_player_host_quick_turn_heading(1,avp_pc_quick_turn_angle(&t,begin+100000000u));
    MovePlayer();
    assert(centre_angle==source-UINT32_C(0x40000000));
    avp_player_host_quick_turn_heading(1,avp_pc_quick_turn_angle(&t,begin+200000000u));
    MovePlayer();
    assert(centre_angle==source-UINT32_C(0x80000000));
    avp_player_host_quick_turn_heading(0,0);
    for(int i=0;i<4;i++){
        MovePlayer();
        assert(centre_angle==source-UINT32_C(0x80000000));
    }
    /* A genuine digital command still follows the existing retail snap. */
    joy_cur=0xffffffffu&~(1u<<FIRE_C)&~(1u<<JOY_RIGHT);
    MovePlayer();
    assert(centre_angle!=source-UINT32_C(0x80000000));
    avp_player_host_quick_turn_reset();
    puts("R267 production MovePlayer interpolated 180-degree yaw, exact endpoint, neutral stability, digital fallback PASS");
    return 0;
}
