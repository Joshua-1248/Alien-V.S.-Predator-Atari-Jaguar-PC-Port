/* Actual native MovePlayer feedback, with the existing conservative fixture
 * SafePos. Deliberately reverse a goal after two unavoidable puddles. */
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
static int contact(uint32_t x,uint32_t y,AvpMpBotAcid p){
    return llabs((long long)x-p.x)<=(30u<<9) && llabs((long long)y-p.y)<=(30u<<9);
}
static int corner(int dm,int sp,int h,int reverse){
 memset(cells,0,sizeof cells);memset(risk,0,sizeof risk);for(int i=1;i<=4;i++){cells[8*(4*W+i)+5]=1;cells[8*(i*W+4)+5]=1;}
 init(sp,h*8192,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
 const uint32_t a=(u32)(1.5*65536),b=(u32)(4.5*65536);
 uint32_t sx=reverse?b:a,sy=reverse?a:b,gx=reverse?a:b,gy=reverse?b:a;
 x_pos=sx;y_pos=sy;
 AvpMpBotState s={0};AvpMpBotObservation v={0};v.map=(AvpMpBotMap){cells,W,W,flags,NULL,0,risk,NULL,0};v.player_species=sp/4;v.deathmatch=dm;v.player_energy=v.max_energy=1000;v.observed_world_level=10;v.player_index=2;v.player_collision_disabled=1;
 v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;v.target_x=gx;v.target_y=gy;v.acid_count=1;v.acid[0]=(AvpMpBotAcid){b,b};
 unsigned crossed=0,returned=0,entries=0,exposure=0,prior=0,flip=0;
 for(unsigned t=1;t<=500;t++){
  AvpMpPlayerCommand c;v.tick=t;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;
  memset(risk,0,sizeof risk);avp_mp_bot_acid_risk_mark(risk,b,b);avp_mp_bot_acid_route_memory(&s,&v,risk);
  if(!crossed && hypot((double)((int64_t)x_pos-gx)/65536.0,(double)((int64_t)y_pos-gy)/65536.0)<.4)crossed=t;
  if(crossed){int other=t-crossed<60&&(t&1);v.target_x=other?gx:sx;v.target_y=other?gy:sy;v.goal=other?AVP_MP_BOT_PATROL:AVP_MP_BOT_SUPPLY;
   if(t-crossed>=60 && hypot((double)((int64_t)x_pos-sx)/65536.0,(double)((int64_t)y_pos-sy)/65536.0)<.4){returned=t;break;}}
  unsigned cur=contact(v.x,v.y,v.acid[0]);entries+=cur&&!prior;prior=cur;exposure+=cur;if(crossed&&t-crossed<60)flip+=cur;
  avp_mp_bot_step_precise(&s,&v,0,&c);apply_cmd(&c,sp);
 }
 int ok=crossed && returned && entries==2 && !flip;
 if(!ok)printf("FAIL dm%d sp%d h%d rev%d crossed=%u returned=%u entries=%u exposure=%u flip=%u xy=%g,%g\n",dm,sp/4,h,reverse,crossed,returned,entries,exposure,flip,x_pos/65536.0,y_pos/65536.0);
 return ok;
}
int main(void){unsigned pass=0,total=0;for(int dm=0;dm<2;dm++)for(int sp=0;sp<=8;sp+=8)for(int h=0;h<8;h++)for(int r=0;r<2;r++){total++;pass+=corner(dm,sp,h,r);}printf("%u/%u corner return scenarios passed\n",pass,total);return pass!=total;}
