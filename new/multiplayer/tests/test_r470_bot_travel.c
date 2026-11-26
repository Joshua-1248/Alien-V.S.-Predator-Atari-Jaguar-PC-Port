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
/* Every initial facing including 180 degrees away: movement and rotation
 * must be simultaneous, and translation must remain along the path.
 * Projected MovePlayer is real; SafePos and AMP collision are fixture stubs. */
int main(void){
 unsigned cases=0,ticks_checked=0;
 for(int sp=0;sp<=8;sp+=4)for(unsigned mapped=0;mapped<2;mapped++)
 for(unsigned h=0;h<64;h++){
    init(sp,h*1024,0);x_pos=(10u<<16)|0x8000;y_pos=(mapped?0u:10u)<<16|0x8000;
    avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    AvpMpBotState s={0};AvpMpBotObservation v={0};
    uint8_t cells[8*16]={0},flags[256]={0};for(unsigned i=0;i<16;i++)cells[i*8+5]=1;
    if(mapped)v.map=(AvpMpBotMap){cells,16,1,flags,NULL,0,NULL,NULL,0};
    v.player_species=(unsigned)sp>>2;v.player_energy=v.max_energy=1000;v.has_target=1;
    v.goal=AVP_MP_BOT_PATROL;v.target_x=x_pos+4*65536;v.target_y=y_pos;
    unsigned simultaneous=0;const u32 start_x=x_pos,start_y=y_pos;
    for(unsigned t=0;t<12;t++){
       AvpMpPlayerCommand c;update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
       assert((c.analog_axis_mask&3)==3 && (fabs(c.analog_forward)+fabs(c.analog_strafe)>0));
       const u32 old_x=x_pos,old_heading=centre_angle;
       apply(&c,1);
       assert(x_pos>old_x);assert(fabs((double)((int64_t)y_pos-start_y))<0.015*65536);
       if(fabs(c.analog_turn)>0.001 && centre_angle!=old_heading)simultaneous++;
       ticks_checked++;
    }
    assert(x_pos>start_x);if(h!=0)assert(simultaneous>0);cases++;
 }
 /* Original stop-for-interaction and scripted Queen wait are intentional. */
 for(unsigned sp=0;sp<3;sp++){
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c;
    uint8_t cells[8*16]={0},flags[256]={0};for(unsigned i=0;i<16;i++)cells[i*8+5]=1;
    v.map=(AvpMpBotMap){cells,16,1,flags,NULL,0,NULL,NULL,0};
    v.x=v.target_x=(10u<<16)|0x8000;v.y=v.target_y=0x8000;
    v.heading=0x80000000;v.player_species=sp;v.player_energy=v.max_energy=1000;v.has_target=1;
    v.goal=AVP_MP_BOT_AIRLOCK_CLOSE;v.target_side=2;v.airlock_door_ready=1;v.tick=1;
    avp_mp_bot_step_precise(&s,&v,0,&c);assert(c.analog_turn!=0 && !(c.analog_axis_mask&3));
    assert(!(c.actions&(1u<<AVP_MP_USE)));
 }
 printf("PASS %u all-heading native travel cases, %u moving ticks, 3 panel-facing holds\n",cases,ticks_checked);
 return 0;
}
