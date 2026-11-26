/* Real projected MovePlayer in a collision-free fixture, driven by the
 * actual precise bot planner. Original collision is separately exercised
 * by the existing retail graph and interlock fixtures. */
#define main r464_fixture_main
#include "../../pc_video/tests/test_analog_velocity_r464.c"
#undef main
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <string.h>
#include <math.h>
extern void avp_player_host_analog_aim(int,float,float,float);
extern void avp_player_host_aim_options(int,int);
extern void avp_player_host_live_heading(int,u32);
extern s16 max_avel;
#define PI 3.14159265358979323846
static double diffdeg(u32 a,u32 b){return (double)(int32_t)(a-b)*360.0/4294967296.0;}
static void update_view(AvpMpBotObservation *v){v->x=x_pos;v->y=y_pos;v->heading=centre_angle;v->tick++;}
static void apply(AvpMpPlayerCommand *c,int movement){
    assert(c->analog_enabled && c->enhanced_analog_enabled);
    assert(!(c->digital_actions&((1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT))));
    joy_cur=UINT32_MAX;
    avp_player_host_aim_options(1,0);
    avp_player_host_analog_aim(1,c->analog_turn,c->analog_turn_sensitivity,c->analog_turn_speed);
    avp_player_host_analog_motion(movement && (c->analog_axis_mask&3u),c->analog_forward,c->analog_strafe);
    sin_ang=sine((u16)(centre_angle>>16));cos_ang=sine((u16)((centre_angle>>16)+0x4000));
    MovePlayer();
}
int main(void){
 unsigned checks=0;
 const double target_degs[]={31.25,90.75,179.875,359.875};
 for(int sp=0;sp<=8;sp+=4)for(unsigned a=0;a<4;a++)for(unsigned h=0;h<16;h++){
    init(sp,h*4096,0);x_pos=y_pos=10u<<16;
    avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    AvpMpBotObservation v={0};v.player_species=(unsigned)sp>>2;v.player_energy=v.max_energy=1000;
    v.has_target=1;v.goal=AVP_MP_BOT_PATROL;
    v.target_x=(u32)((int64_t)x_pos+llround(5*65536*cos(target_degs[a]*PI/180)));
    v.target_y=(u32)((int64_t)y_pos-llround(5*65536*sin(target_degs[a]*PI/180)));
    const u32 target=(u32)(uint64_t)llround(target_degs[a]*4294967296.0/360);
    for(unsigned t=0;t<45;t++){
       AvpMpBotState s={0};AvpMpPlayerCommand c;update_view(&v);
       avp_mp_bot_step_precise(&s,&v,max_avel,&c);apply(&c,0);
    }
    assert(fabs(diffdeg(centre_angle,target))<0.001);
    u32 stop=centre_angle;
    for(unsigned t=0;t<100;t++){
       AvpMpBotState s={0};AvpMpPlayerCommand c;update_view(&v);
       avp_mp_bot_step_precise(&s,&v,max_avel,&c);apply(&c,0);
    }
    assert(fabs(diffdeg(centre_angle,stop))<0.00001);checks++;
 }
 for(int sp=0;sp<=8;sp+=4)for(unsigned direction=0;direction<8;direction++){
    init(sp,direction*8192,0);x_pos=y_pos=(10u<<16)|0x8000;
    avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    AvpMpBotState s={0};AvpMpBotObservation v={0};
    uint8_t cells[8*16*16]={0},flags[256]={0};for(unsigned i=0;i<16*16;i++)cells[8*i+5]=1;
    v.map=(AvpMpBotMap){cells,16,16,flags,NULL,0,NULL,NULL,0};
    v.player_species=(unsigned)sp>>2;v.player_energy=v.max_energy=1000;v.has_target=1;
    v.goal=AVP_MP_BOT_AIRLOCK_SEAT;v.airlock_face_side=2;
    v.target_x=x_pos+20000;v.target_y=y_pos-12000;
    double initial=hypot((double)((int64_t)v.target_x-x_pos),(double)((int64_t)v.target_y-y_pos));
    for(unsigned t=0;t<50;t++){
       AvpMpPlayerCommand c;update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);apply(&c,1);
    }
    double remaining=hypot((double)((int64_t)v.target_x-x_pos),(double)((int64_t)v.target_y-y_pos));
    assert(remaining<initial && remaining<0.16*65536);
    assert((x_pos>>16)==10 && (y_pos>>16)==10);
    assert(!x_vel&&!y_vel);checks++;
 }
 printf("PASS %u actual projected MovePlayer bot-heading and seating scenarios (free-space fixture)\n",checks);
 return 0;
}
