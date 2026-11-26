/* Native MovePlayer feedback with a conservative wall-margin fixture, not a live match. */
#define SafePos fixture_unused_safe_pos
#define main r464_fixture_main
#include "../../pc_video/tests/test_analog_velocity_r464.c"
#undef main
#undef SafePos
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <math.h>
#include <string.h>
extern void avp_player_host_analog_aim(int,float,float,float);
extern void avp_player_host_aim_options(int,int);
extern void avp_player_host_live_heading(int,u32);
extern s16 max_avel;
#define W 9
static uint8_t cells[W*W*8],flags[256],risk[4096];
static int floor_at(int x,int y){return x>=0&&y>=0&&x<W&&y<W&&cells[8*(y*W+x)+5];}
u8 SafePos(u8 h,s32 px,s32 py,u16 w){(void)h;(void)w;double x=px/65536.0,y=py/65536.0;int cx=(int)floor(x),cy=(int)floor(y);double a=x-cx,b=y-cy;return floor_at(cx,cy)&&(a>=.3125||floor_at(cx-1,cy))&&(a<=.6875||floor_at(cx+1,cy))&&(b>=.3125||floor_at(cx,cy-1))&&(b<=.6875||floor_at(cx,cy+1))?AVP_COLL_SAFE:0;}
static void apply_cmd(AvpMpPlayerCommand *c,int sp){
 joy_cur=UINT32_MAX;
 unsigned a=c->digital_actions;
 if(a&(1u<<AVP_MP_FORWARD))joy_cur&=~(1u<<JOY_UP);
 if(a&(1u<<AVP_MP_BACKWARD))joy_cur&=~(1u<<JOY_DOWN);
 if(a&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))){joy_cur&=~(1u<<(sp==4?OPTION:FIRE_C));if(a&(1u<<AVP_MP_STRAFE_LEFT))joy_cur&=~(1u<<JOY_LEFT);if(a&(1u<<AVP_MP_STRAFE_RIGHT))joy_cur&=~(1u<<JOY_RIGHT);}
 avp_player_host_aim_options(1,0);avp_player_host_analog_aim(1,c->analog_turn,c->analog_turn_sensitivity,c->analog_turn_speed);
 avp_player_host_analog_motion((c->analog_axis_mask&3u)!=0,c->analog_forward,c->analog_strafe);
 sin_ang=sine((u16)(centre_angle>>16));cos_ang=sine((u16)((centre_angle>>16)+0x4000));MovePlayer();
}
static int run(int dm,int sp,int profile,int cross,int h,int side,int acid){
 memset(cells,0,sizeof cells);memset(risk,0,sizeof risk);
 for(int i=1;i<=4;i++){cells[8*(4*W+i)+5]=1;cells[8*(i*W+4)+5]=1;}
 if(cross)for(int i=5;i<=7;i++){cells[8*(4*W+i)+5]=1;cells[8*(i*W+4)+5]=1;}
 init(sp,h*8192,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
 x_pos=(u32)(3.55*65536);y_pos=(u32)((4.32+.18*side)*65536);
 AvpMpBotState s={0};AvpMpBotObservation v={0};v.map=(AvpMpBotMap){cells,W,W,flags,NULL,0,risk,NULL,0};v.player_species=sp/4;v.deathmatch=dm;v.behavior=profile;v.player_index=2;v.player_collision_disabled=1;v.player_energy=v.max_energy=1000;v.observed_world_level=6;
 v.target_x=(u32)(4.5*65536);v.target_y=(u32)(1.5*65536);v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;
 if(acid){v.acid_count=1;v.acid[0]=(AvpMpBotAcid){(u32)((4.05+.225*((acid-1)%5))*65536),(u32)((4.05+.225*((acid-1)/5))*65536)};avp_mp_bot_acid_risk_mark(risk,v.acid[0].x,v.acid[0].y);}
 for(unsigned t=1;t<=210;t++){AvpMpPlayerCommand c;v.tick=t;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;avp_mp_bot_step_precise(&s,&v,0,&c);apply_cmd(&c,sp);if(fabs(x_pos/65536.0-4.5)<.55&&fabs(y_pos/65536.0-1.5)<.55)return 1;}
 return 0;
}
int main(void){unsigned ok=0,fail=0;for(int dm=0;dm<2;dm++)for(int sp=0;sp<=8;sp+=8)for(int b=0;b<3;b++)for(int cross=0;cross<2;cross++)for(int h=0;h<8;h++)for(int side=0;side<3;side++)for(int acid=0;acid<=25;acid++){if(run(dm,sp,b,cross,h,side,acid))ok++;else{if(fail<20)fprintf(stderr,"FAIL dm%d sp%d b%d cross%d h%d side%d acid%d\n",dm,sp,b,cross,h,side,acid);fail++;}}printf("native junction: %u passed %u failed\n",ok,fail);return !!fail;}
