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
static int scenario(int dm,int sp,int heading){
    memset(cells,0,sizeof cells);memset(risk,0,sizeof risk);
    for(int i=1;i<=7;i++)cells[8*(4*W+i)+5]=1;
    init(sp,heading*8192,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    x_pos=(u32)(1.5*65536);y_pos=(u32)(4.5*65536);
    AvpMpBotState s={0};AvpMpBotObservation v={0};
    v.map=(AvpMpBotMap){cells,W,W,flags,NULL,0,risk,NULL,0};
    v.player_species=sp/4;v.deathmatch=dm;v.player_index=2;v.player_collision_disabled=1;
    v.player_energy=v.max_energy=1000;v.observed_world_level=4;
    v.target_x=(u32)(6.5*65536);v.target_y=y_pos;v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;
    v.acid_count=2;for(unsigned i=0;i<2;i++){
        v.acid[i]=(AvpMpBotAcid){(u32)((3.5+i)*65536),y_pos};
        avp_mp_bot_acid_risk_mark(risk,v.acid[i].x,v.acid[i].y);
    }
    unsigned crossed=0,returns=0,prior=0,exposure=0;
    for(unsigned t=1;t<=360;t++){
        AvpMpPlayerCommand c;v.tick=t;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;
        unsigned cur=0;for(unsigned j=0;j<2;j++)if(contact(v.x,v.y,v.acid[j]))cur|=1u<<j;
        if(!crossed && v.x>(u32)(5.25*65536)){crossed=t;}
        if(crossed){
            v.target_x=(u32)(((t-crossed)%60<40?1.5:6.5)*65536);
            v.goal=(t&1)?AVP_MP_BOT_SUPPLY:AVP_MP_BOT_PATROL;
            if(cur&~prior)returns++;
            if(cur)exposure++;
        }
        prior=cur;avp_mp_bot_step_precise(&s,&v,0,&c);apply_cmd(&c,sp);
    }
    printf("dm=%d species=%d heading=%d initial_cross=%u reentries=%u exposure_ticks=%u final=%.3f\n",dm,sp/4,heading,crossed,returns,exposure,x_pos/65536.0);
    return crossed && (sp==4 || (!returns && !exposure));
}
int main(void){unsigned pass=0,total=0;for(int dm=0;dm<2;dm++)for(int sp=0;sp<=8;sp+=4)for(int h=0;h<8;h++){
    total++;pass+=scenario(dm,sp,h);
}printf("%u/%u scenarios passed\n",pass,total);return pass!=total;}
