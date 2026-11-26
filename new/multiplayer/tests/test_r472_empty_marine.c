/* Real projected MovePlayer in a collision-free fixture, driven by the
 * actual precise bot planner. Original collision is separately exercised
 * by the existing retail graph and interlock fixtures. */
#define main r464_fixture_main
#include "../../pc_video/tests/test_analog_velocity_r464.c"
#undef main
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include "doors.h"
#include <string.h>
#include <math.h>
extern void avp_player_host_analog_aim(int,float,float,float);
extern void avp_player_host_aim_options(int,int);
extern void avp_player_host_live_heading(int,u32);
extern s16 max_avel;
#define PI 3.14159265358979323846
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
static const unsigned moves=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
    (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
int main(void){
 unsigned cases=0,ticks=0,rearmed=0;
 for(unsigned dm=0;dm<2;dm++)for(unsigned behavior=0;behavior<3;behavior++)
 for(unsigned side=0;side<4;side++)for(unsigned h=0;h<32;h++){
    init(0,h*2048,0);x_pos=y_pos=(12u<<16)|0x8000;
    avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c;
    uint8_t cells[8*24*24]={0},flags[256]={0};for(unsigned i=0;i<24*24;i++)cells[i*8+5]=1;
    v.map=(AvpMpBotMap){cells,24,24,flags,NULL,0,NULL,NULL,0};
    v.player_energy=v.max_energy=1000;v.deathmatch=dm;v.behavior=behavior;
    v.marine_inventory_valid=1;v.marine_owned=1u<<2;v.current_weapon=2;
    v.marine_unarmed=v.marine_low_ammo=1;v.has_target=1;
    v.goal=AVP_MP_BOT_HOSTILE;v.hostile_target=1;v.target_visible=1;
    v.target_energy_positive=1;v.target_slot=1;
    v.target_x=x_pos+(side==0?2*65536:side==1?-2*65536:0);
    v.target_y=y_pos+(side==2?2*65536:side==3?-2*65536:0);
    for(unsigned i=0;i<8;i++){
       update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
       assert(!(c.actions&(1u<<AVP_MP_FIRE)));assert(c.actions&moves);
       assert((c.analog_axis_mask&3)==3);
       assert(fabs(hypot(c.analog_forward,c.analog_strafe)-1.0)<0.00001);
       const double old=hypot((double)x_pos-v.target_x,(double)y_pos-v.target_y);
       apply(&c,1);assert(hypot((double)x_pos-v.target_x,(double)y_pos-v.target_y)>old);
       ticks++;
    }
    assert(!s.travel_valid);cases++;
    /* An available loaded weapon releases the empty-inventory guard. */
    v.marine_unarmed=v.marine_low_ammo=0;v.marine_ammo[1]=100;
    v.target_x=x_pos+2*65536;v.target_y=y_pos;centre_angle=0;
    avp_player_host_live_heading(0,0);memset(&s,0,sizeof s);
    update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));rearmed++;
 }
 /* A supply objective is not overwritten by ordinary squad regrouping. */
 unsigned supplies=0;
 for(unsigned h=0;h<32;h++){
    init(0,h*2048,0);x_pos=y_pos=(10u<<16)|0x8000;
    avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c;
    uint8_t cells[8*24*24]={0},flags[256]={0};for(unsigned i=0;i<24*24;i++)cells[i*8+5]=1;
    v.map=(AvpMpBotMap){cells,24,24,flags,NULL,0,NULL,NULL,0};
    v.player_energy=v.max_energy=1000;v.marine_inventory_valid=1;v.marine_owned=1u<<2;
    v.current_weapon=2;v.marine_unarmed=v.marine_low_ammo=1;
    v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;v.target_x=x_pos+6*65536;v.target_y=y_pos;
    assert(!avp_mp_bot_team_regroup_priority(&v,UINT64_MAX,UINT64_MAX,0));
    for(unsigned i=0;i<12;i++){
        update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));assert(c.actions&moves);
        assert(fabs(hypot(c.analog_forward,c.analog_strafe)-1.0)<0.00001);
        u32 old=x_pos;apply(&c,1);assert(x_pos>old);
    }
    supplies++;
 }
 printf("PASS %u empty combat retreats, %u movement ticks, %u rearmed fire commands, %u supply heading cases\n",cases,ticks,rearmed,supplies);
 return 0;
}
