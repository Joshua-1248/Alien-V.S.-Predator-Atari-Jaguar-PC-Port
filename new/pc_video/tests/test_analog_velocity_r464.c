/* Actual projected MovePlayer with a collision-free fixture and native sine table. */
#include "player.h"
#include "maze.h"
#include "joypad.h"
#include "avp_runtime.h"
#include "collision.h"
#include "doors.h"
#include "pc/pc_axis_deadzone.h"
#include "pc/pc_input_routing.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
volatile u32 joy_cur=0xffffffffu;
u32 x_pos,y_pos,centre_angle;
s32 sin_ang,cos_ang;
s16 hugkill;
u8 lift_key;
static const AvpRuntimeOps ops={0};
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}
u8 SafePos(u8 h,s32 x,s32 y,u16 w){(void)h;(void)x;(void)y;(void)w;return AVP_COLL_SAFE;}
int AMPCollisions(s32 x,s32 y){(void)x;(void)y;return 0;}
extern void avp_player_host_analog_motion(int,float,float);
extern void avp_player_host_double_movement(int);
extern void avp_player_host_keyboard_strafe_during_turn(int);
extern u32 max_speed;
extern s32 x_vel,y_vel;
static const u16 sine_table[513]={
#include "../../../src/game/sintab.inc"
};
static s32 sine(u16 a){u16 b=a&0x7fff;if(b&0x4000)b=0x8000-b;return (a&0x8000)?-(s32)(sine_table[b>>5]>>1):(s32)(sine_table[b>>5]>>1);}
static int metric(s32 x,s32 y){x=abs(x);y=abs(y);return 3*(x>y?x:y)+(x>y?y:x);}
static void init(int sp,unsigned heading,int fast){player_type=sp;InitMove();ResetMove();centre_angle=heading<<16;sin_ang=sine(heading);cos_ang=sine((u16)(heading+0x4000));x_pos=y_pos=0x40000000;joy_cur=UINT32_MAX;lift_key=0;avp_player_host_analog_motion(0,0,0);avp_player_host_keyboard_strafe_during_turn(0);avp_player_host_double_movement(fast);}
static u32 pad(int sp,int dir){if(dir==0)return UINT32_MAX&~(1u<<JOY_UP);if(dir==1)return UINT32_MAX&~(1u<<JOY_DOWN);return UINT32_MAX&~((1u<<(sp==PT_ALIEN?OPTION:FIRE_C))|(1u<<(dir==2?JOY_LEFT:JOY_RIGHT)));}
static void ticks(int n){while(n--)MovePlayer();}
static void analog(int dir,float value){avp_player_host_analog_motion(1,dir<2?(dir==0?value:-value):0,dir>=2?(dir==2?-value:value):0);}
int main(void){unsigned cases=0;for(int sp=0;sp<=8;sp+=4)for(int fast=0;fast<2;fast++)for(unsigned h=0;h<65536;h+=2048)for(int dir=0;dir<4;dir++){
    init(sp,h,fast);joy_cur=pad(sp,dir);ticks(80);s32 dx=x_vel,dy=y_vel;
    init(sp,h,fast);joy_cur=pad(sp,dir);avp_player_host_analog_motion(1,0,0);ticks(80);assert(x_vel==dx&&y_vel==dy);
    init(sp,h,fast);analog(dir,1);ticks(80);s32 fullx=x_vel,fully=y_vel;int cap=3*(int)max_speed*(fast?2:1);assert(metric(fullx,fully)<=cap && metric(fullx,fully)>=cap-4);
    if(h==0&&fast==0)printf("species %d direction %d digital %d analog %d cap %d\n",sp,dir,metric(dx,dy)/3,metric(fullx,fully)/3,cap/3);
    init(sp,h,fast);joy_cur=pad(sp,dir);analog(dir,1);ticks(80);assert(x_vel==fullx&&y_vel==fully);
    init(sp,h,fast);analog(dir,0.5f);ticks(80);s32 halfx=x_vel,halfy=y_vel;assert(abs(metric(halfx,halfy)-cap/2)<=4);
    init(sp,h,fast);joy_cur=pad(sp,dir);analog(dir,0.5f);ticks(80);assert(x_vel==halfx&&y_vel==halfy);
    avp_player_host_analog_motion(1,0,0);joy_cur=UINT32_MAX;ticks(80);assert(x_vel==0&&y_vel==0);cases++;
}
for(int sp=0;sp<=8;sp+=4){init(sp,0,0);lift_key=1;analog(0,1);ticks(80);assert(!x_vel&&!y_vel);analog(3,1);ticks(80);assert(y_vel==(s32)max_speed);init(sp,0,0);avp_player_host_analog_motion(1,1,1);ticks(80);assert(x_vel>0&&y_vel>0&&metric(x_vel,y_vel)<=3*(int)max_speed);}
for(int invert=0;invert<=1;invert++)for(int d=0;d<=90;d+=5){float dz=d/100.0f;assert(avp_pc_axis_motion_full_travel(avp_pc_axis_deadzone(32767,dz,invert))==(invert?-1.0f:1.0f));assert(avp_pc_axis_motion_full_travel(avp_pc_axis_deadzone(-32768,dz,invert))==(invert?1.0f:-1.0f));}
printf("PASS %u native movement scenarios: all species, 32 headings, 4 directions, normal/double speed, half/full, duplicate digital, neutral and lift\n",cases);return 0;}
