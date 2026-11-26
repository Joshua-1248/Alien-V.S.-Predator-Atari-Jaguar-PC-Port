/* R319 F12 2x player movement: exercise the real native PLAYER.S movement
 * code under a controlled collision-free 10Hz tick, not a copied algorithm.
 * The always-on Jaguar path must remain bit-exact with the cheat disabled. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "player.h"
#include "joypad.h"
#include "maze.h"
#include "collision.h"
#include "avp_runtime.h"
#include "pc/pc_double_movement.h"

volatile u32 joy_cur=UINT32_MAX;
s32 sin_ang,cos_ang;
u32 x_pos,y_pos,centre_angle;
s16 hugkill,use_cocoon,lift_key;
static int safe(void *u,s32 x,s32 y,s16 h,u16 w)
{ (void)u;(void)x;(void)y;(void)h;(void)w;return AVP_COLL_SAFE; }
static int amp(void *u,s32 x,s32 y)
{ (void)u;(void)x;(void)y;return 1; }
static int noclip(void *u){(void)u;return 0;}
static AvpRuntimeOps ops={.safe_pos=safe,.amp_collision=amp,.player_noclip=noclip};
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}
u8 SafePos(u8 h,s32 x,s32 y,u16 w)
{ (void)h;(void)x;(void)y;(void)w;return AVP_COLL_SAFE; }
int AMPCollisions(s32 x,s32 y){(void)x;(void)y;return 0;}

static s32 move_once(int species,int fast)
{
    player_type=(s16)species;
    InitMove(); ResetMove();
    x_pos=0x00200000u;y_pos=0x00200000u;centre_angle=0;
    sin_ang=0; cos_ang=65536;
    joy_cur=UINT32_MAX ^ (1u<<JOY_UP);
    avp_player_host_double_movement(fast);
    avp_player_host_analog_motion(0,0.0f,0.0f);
    avp_player_host_analog_aim(0,0.0f,1.0f,1.0f);
    avp_player_host_keyboard_strafe_during_turn(0);
    MovePlayer();
    assert(y_pos==0x00200000u);
    return x_vel;
}
static s32 travel_n_ticks(int species,int fast,unsigned ticks)
{
    player_type=(s16)species;
    InitMove(); ResetMove();
    x_pos=0x00200000u;y_pos=0x00200000u;centre_angle=0;
    sin_ang=0; cos_ang=65536;
    joy_cur=UINT32_MAX ^ (1u<<JOY_UP);
    avp_player_host_double_movement(fast);
    avp_player_host_analog_motion(0,0.0f,0.0f);
    avp_player_host_analog_aim(0,0.0f,1.0f,1.0f);
    avp_player_host_keyboard_strafe_during_turn(0);
    for(unsigned i=0;i<ticks;i++)MovePlayer();
    assert(y_pos==0x00200000u);
    return (s32)(x_pos-0x00200000u);
}
int main(void)
{
    const int species[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR};
    for(unsigned i=0;i<3u;i++){
        const s32 normal=move_once(species[i],0);
        const s32 doubled=move_once(species[i],1);
        assert(normal>0 && doubled==normal*2);
        assert(move_once(species[i],0)==normal); /* instant OFF */
        {const s32 distance=travel_n_ticks(species[i],0,50u);
         const s32 fast_distance=travel_n_ticks(species[i],1,50u);
         /* Same original 10Hz ticks and friction; the doubled movement
          * stays approximately 2x even at sustained terminal speed. */
         assert(distance>0 && fast_distance>distance*19/10);
         assert(fast_distance<distance*21/10);
        }
    }
    assert(avp_pc_double_move_shift(4,0)==4);
    assert(avp_pc_double_move_shift(4,1)==3);
    assert(avp_pc_double_move_shift(3,1)==2); /* Alien */
    assert(avp_pc_double_move_speed(11900u,0)==11900u);
    assert(avp_pc_double_move_speed(11900u,1)==23800u);
    assert(avp_pc_double_move_speed(17000u,1)==34000u);
    assert(avp_pc_double_move_speed(UINT32_MAX,1)==UINT32_MAX);
    avp_player_host_double_movement(0);
    puts("R319 native first-tick movement + Jaguar OFF restoration: PASS (3 species)");
    return 0;
}
