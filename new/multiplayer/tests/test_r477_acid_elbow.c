/* Controlled feedback replay of a one-cell-wide elbow, both directions,
 * using production commands and a conservative wall-margin acceptance model.
 * Not an emulation of native collision or acid damage. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define W 9
#define CELL(x,y) (8*((y)*W+(x)))
static unsigned char cells[W*W*8],flags[256],risk[4096];
static int floor_at(int x,int y){return x>=0&&y>=0&&x<W&&y<W&&cells[CELL(x,y)+5];}
static int clear(double x,double y){int cx=(int)floor(x),cy=(int)floor(y);double a=x-cx,b=y-cy;return floor_at(cx,cy)&& (a>=.3125||floor_at(cx-1,cy))&&(a<=.6875||floor_at(cx+1,cy))&&(b>=.3125||floor_at(cx,cy-1))&&(b<=.6875||floor_at(cx,cy+1));}
static int run(int dm,int sp,int profile,int reverse,int h,int side,int ax,int ay){
 AvpMpBotObservation v={0};AvpMpBotState s={0};AvpMpPlayerCommand c;
 memset(cells,0,sizeof cells);memset(risk,0,sizeof risk);
 for(int i=1;i<=4;i++){cells[CELL(i,4)+5]=1;cells[CELL(4,i)+5]=1;}
 v.map=(AvpMpBotMap){cells,W,W,flags,NULL,0,risk,NULL,0};v.player_species=sp;v.deathmatch=dm;v.behavior=profile;v.player_index=2;v.player_collision_disabled=1;v.player_energy=v.max_energy=1000;v.observed_world_level=6;
 v.x=(unsigned)((reverse?4.32+.18*side:1.5)*65536);v.y=(unsigned)((reverse?1.5:4.32+.18*side)*65536);v.heading=(unsigned)h*0x20000000u;
 v.target_x=(unsigned)((reverse?1.5:4.5)*65536);v.target_y=(unsigned)((reverse?4.5:1.5)*65536);v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;
 v.acid_count=1;v.acid[0]=(AvpMpBotAcid){(unsigned)((4.05+.225*ax)*65536),(unsigned)((4.05+.225*ay)*65536)};avp_mp_bot_acid_risk_mark(risk,v.acid[0].x,v.acid[0].y);
 for(unsigned t=1;t<=210;t++){
  v.tick=t;avp_mp_bot_step_precise(&s,&v,0,&c);
  double a=v.heading*(2*3.141592653589793/4294967296.0),fx=cos(a),fy=-sin(a),f=0,r=0;
  unsigned m=c.digital_actions;
  if(c.analog_axis_mask&1)f=c.analog_forward;else f=!!(m&(1u<<AVP_MP_FORWARD))-!!(m&(1u<<AVP_MP_BACKWARD));
  if(c.analog_axis_mask&2)r=c.analog_strafe;else r=!!(m&(1u<<AVP_MP_STRAFE_RIGHT))-!!(m&(1u<<AVP_MP_STRAFE_LEFT));
  v.heading-=(unsigned)(int32_t)llround(c.analog_turn*(sp==1?0x900:0x600)*65536.0);
  double x=v.x/65536.0+.07*(fx*f-fy*r),y=v.y/65536.0+.07*(fy*f+fx*r);
  if(clear(x,y)){v.x=(unsigned)(x*65536);v.y=(unsigned)(y*65536);}
  if(fabs((double)v.x/65536-v.target_x/65536.0)<.55&&fabs((double)v.y/65536-v.target_y/65536.0)<.55)return 1;
 }
 return 0;
}
int main(void){unsigned ok=0,fail=0;for(int dm=0;dm<2;dm++)for(int sp=0;sp<3;sp+=2)for(int b=0;b<3;b++)for(int rev=0;rev<2;rev++)for(int h=0;h<8;h++)for(int side=0;side<3;side++)for(int ax=0;ax<5;ax++)for(int ay=0;ay<5;ay++){if(run(dm,sp,b,rev,h,side,ax,ay))ok++;else {if(fail<15)fprintf(stderr,"FAIL dm%d sp%d b%d rev%d h%d side%d ax%d ay%d\n",dm,sp,b,rev,h,side,ax,ay);fail++;}}printf("elbow replay: %u passed %u failed\n",ok,fail);return !!fail;}
