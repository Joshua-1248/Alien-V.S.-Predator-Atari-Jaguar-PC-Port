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
/* The host normally supplies a new hostile goal after a visible contact.
 * Simulate that exact sequence with the real planner and projected MovePlayer.
 * Visibility is injected; native firing and collision are not simulated here. */
int main(void){
 unsigned cases=0,frames=0,firing=0;
 for(unsigned sp=0;sp<=2;sp+=2)for(unsigned behavior=0;behavior<3;behavior++)
 for(unsigned side=0;side<2;side++)for(unsigned h=0;h<16;h++){
    init(sp*4,h*4096,0);x_pos=(10u<<16)|0x8000;y_pos=(10u<<16)|0x8000;
    avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c;
    uint8_t cells[8*24*24]={0},flags[256]={0},risk[4096]={0};for(unsigned i=0;i<24*24;i++)cells[i*8+5]=1;
    v.map=(AvpMpBotMap){cells,24,24,flags,NULL,0,risk,NULL,0};
    v.player_species=sp;v.behavior=behavior;v.player_energy=v.max_energy=1000;
    v.current_weapon=2;v.marine_inventory_valid=sp==0;
    v.marine_owned=1u<<2;v.marine_ammo[1]=100;v.predator_owned=1u<<2;
    v.observed_world_level=1;v.has_target=1;v.goal=AVP_MP_BOT_PATROL;
    v.target_x=x_pos+8*65536;v.target_y=y_pos;
    update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
    assert(s.travel_valid && s.travel_x==v.target_x && s.travel_y==v.target_y);
    const u32 start_y=y_pos;v.goal=AVP_MP_BOT_HOSTILE;v.hostile_target=v.target_visible=1;
    v.target_x=x_pos+2*65536;v.target_y=side?y_pos+3*65536:y_pos-3*65536;
    v.target_energy_positive=1;v.target_slot=1;
    if(h&1u){ /* A real level can contain acid on a distant route. */
        v.acid_count=1;v.acid[0]=(AvpMpBotAcid){2u<<16,2u<<16};
        risk[2*64+2]=2;
    }
    unsigned fire=0;
    for(unsigned t=0;t<30;t++){
       update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
       if(!(s.travel_valid && (c.analog_axis_mask&3)==3)){fprintf(stderr,"FAIL sp=%u b=%u side=%u h=%u t=%u x=%x y=%x a=%x mask=%u travel=%u ev=%u last=%u imm=%u\n",sp,behavior,side,h,t,x_pos,y_pos,c.actions,c.analog_axis_mask,s.travel_valid,s.evade_ticks,s.last_motion,s.immobile_ticks);abort();}
       const u32 old_x=x_pos;
       if(c.actions&(1u<<AVP_MP_FIRE))fire++;
       apply(&c,1);
       assert(x_pos>old_x);
       assert(fabs((double)((int64_t)y_pos-start_y))<0.025*65536);
       frames++;
    }
    assert(fire>0);firing+=fire;cases++;
    /* A change of native level invalidates the former route. */
    v.observed_world_level=2;update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
    assert(!s.travel_valid);
 }
 /* Melee can also attack on a clear route when the native range/aim
  * gate permits it. Out-of-reach melee still uses its pursuit tactics. */
 unsigned melee=0;
 for(unsigned sp=1;sp<=2;sp++)for(unsigned b=0;b<3;b++){
    init(sp*4,0x4000,0);x_pos=(10u<<16)|0x8000;y_pos=(10u<<16)|0x8000;
    avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c;
    uint8_t cells[8*24*24]={0},flags[256]={0};for(unsigned i=0;i<24*24;i++)cells[i*8+5]=1;
    v.map=(AvpMpBotMap){cells,24,24,flags,NULL,0,NULL,NULL,0};
    v.player_species=sp;v.behavior=b;v.player_energy=v.max_energy=1000;
    v.current_weapon=1;v.predator_owned=1u<<1;
    v.has_target=1;v.goal=AVP_MP_BOT_PATROL;v.target_x=x_pos+8*65536;v.target_y=y_pos;
    update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
    v.goal=AVP_MP_BOT_HOSTILE;v.hostile_target=v.target_visible=v.target_energy_positive=1;
    v.target_x=x_pos;v.target_y=y_pos-31457;v.target_slot=1;
    update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
    assert(s.travel_tick==v.tick && (c.analog_axis_mask&3)==3);
    const unsigned attacks=sp==1?((1u<<AVP_MP_ALIEN_CLAW)|(1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)):(1u<<AVP_MP_FIRE);
    assert(c.actions&attacks);
    u32 old_x=x_pos;apply(&c,1);assert(x_pos>old_x);melee++;
 }
 /* Safety exceptions compare a cached route to the same ordinary combat
  * planner with no cache. Its attack/movement decisions must be identical. */
 unsigned guards=0;
 for(unsigned mode=0;mode<9;mode++){
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c,a;
    uint8_t cells[8*24*24]={0},flags[256]={0},risk[4096]={0};for(unsigned i=0;i<24*24;i++)cells[i*8+5]=1;
    v.map=(AvpMpBotMap){cells,24,24,flags,NULL,0,risk,NULL,0};
    v.player_energy=v.max_energy=1000;v.current_weapon=2;v.marine_inventory_valid=1;
    v.marine_owned=1u<<2;v.marine_ammo[1]=100;v.observed_world_level=1;
    v.x=v.y=(10u<<16)|0x8000;v.has_target=1;v.goal=AVP_MP_BOT_PATROL;
    v.target_x=v.x+8*65536;v.target_y=v.y;v.tick=1;
    avp_mp_bot_step_precise(&s,&v,0,&c);assert(s.travel_valid);
    v.goal=AVP_MP_BOT_HOSTILE;v.hostile_target=v.target_visible=1;
    v.target_x=v.x;v.target_y=v.y-3*65536;v.target_energy_positive=1;v.tick=2;
    switch(mode){
    case 0:v.target_visible=0;v.tracker_only_target=1;break;
    case 1:v.health_low=1;break;
    case 2:v.facehugged=1;break;
    case 3:v.observed_world_level=2;break;
    case 4:v.tick=123;break;
    case 5: /* Seal the remembered route behind a real floor/wall boundary. */
        flags[1]=AVP_WB_SOLID;
        for(unsigned y=0;y<24;y++){cells[(y*24+10)*8+2]=1;cells[(y*24+11)*8]=1;}break;
    case 6:v.acid_count=1;v.acid[0]=(AvpMpBotAcid){v.x+32768,v.y};risk[10*64+10]=2;break;
    case 7:v.nearby_peer_valid=1;v.nearby_peer_x=v.x+32768;v.nearby_peer_y=v.y;break;
    case 8:v.cover_flank_valid=1;v.cover_flank_x=9;v.cover_flank_y=9;break;
    }
    AvpMpBotState ordinary=s;ordinary.travel_valid=0;
    avp_mp_bot_step_precise(&ordinary,&v,0,&a);avp_mp_bot_step_precise(&s,&v,0,&c);
    assert(c.actions==a.actions && c.digital_actions==a.digital_actions && c.analog_axis_mask==a.analog_axis_mask &&
           c.analog_turn==a.analog_turn && c.analog_forward==a.analog_forward && c.analog_strafe==a.analog_strafe);
    if(mode==0)assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    if(mode>=2 && mode<=4)assert(!s.travel_valid);
    guards++;
 }
 printf("PASS %u ranged combat travel cases, %u movement ticks, %u native fire commands, %u melee attacks on route, %u safety guards, level invalidation\n",cases,frames,firing,melee,guards);
 return 0;
}
