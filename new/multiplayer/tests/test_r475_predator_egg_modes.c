/* Native MovePlayer integration with injected optical contact and synthetic
 * elbow geometry. Collision is stubbed by the inherited fixture; assert
 * the resulting positions independently against the intended corridor. */
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
#define CELL(x,y) (8*((y)*12+(x)))
static void floor_at(uint8_t *cells,int x,int y){cells[CELL(x,y)+5]=1;}
static unsigned moves(void){return (1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);}
int main(void){
 unsigned cases=0,frames=0,firing=0;
 const int vx[]={1,0,-1,0},vy[]={0,1,0,-1};
 for(unsigned dm=0;dm<2;dm++)for(unsigned rot=0;rot<4;rot++)for(unsigned b=0;b<3;b++)for(unsigned w=1;w<=4;w+=3)for(unsigned h=0;h<16;h++){
   uint8_t cells[8*12*12]={0},flags[256]={0};
   int ex=5,ey=5,dx=vx[rot],dy=vy[rot];
   floor_at(cells,ex,ey);floor_at(cells,ex-dx,ey-dy);floor_at(cells,ex-2*dx,ey-2*dy);floor_at(cells,ex-dy,ey+dx);
   init(8,h*4096,0);x_pos=(ex-dx)*65536+32768;y_pos=(ey-dy)*65536+32768;
   avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
   AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c;
   v.map=(AvpMpBotMap){cells,12,12,flags,NULL,0,NULL,NULL,0};
   v.player_species=2;v.deathmatch=dm;v.behavior=b;v.player_energy=v.max_energy=1000;v.current_weapon=w;v.predator_owned=1u<<w;
   v.observed_world_level=14;v.has_target=v.hostile_target=v.target_visible=v.target_is_egg=v.target_energy_positive=1;
   v.goal=AVP_MP_BOT_HOSTILE;v.target_x=ex*65536+32768;v.target_y=ey*65536+32768;
   v.prop_count=1;v.props[0]=(AvpMpBotProp){v.target_x,v.target_y,(40u<<9)*85/100};
   unsigned fire=0;
   for(unsigned t=0;t<60;t++){
      update_view(&v);avp_mp_bot_step_precise(&s,&v,max_avel,&c);
      if(c.actions&(1u<<AVP_MP_FIRE)){
         assert(!(c.actions&moves()));assert(fabs(c.analog_forward)<1e-6&&fabs(c.analog_strafe)<1e-6);fire++;
      }
      apply(&c,1);frames++;
      double x=x_pos/65536.0,y=y_pos/65536.0;
      double ax=(double)ex+0.5,ay=(double)ey+0.5;
      if(!(fabs(dx?(y-ay):(x-ax))<0.188 && hypot(x-ax,y-ay)>0.30)){
         fprintf(stderr,"clearance fail rot=%u b=%u w=%u h=%u t=%u xy=%f,%f\n",rot,b,w,h,t,x,y);return 1;
      }
   }
   if(fire<10){fprintf(stderr,"fire fail rot=%u b=%u w=%u h=%u fire=%u xy=%f,%f\n",rot,b,w,h,fire,x_pos/65536.0,y_pos/65536.0);return 1;}
   cases++;firing+=fire;
 }
 /* Ranged fallback, no fictitious attack before decloaking, safety override. */
 uint8_t cells[8*12*12]={0},flags[256]={0};floor_at(cells,4,5);floor_at(cells,5,5);
 AvpMpBotObservation v={0};AvpMpBotState s={0};AvpMpPlayerCommand c;
 v.map=(AvpMpBotMap){cells,12,12,flags,NULL,0,NULL,NULL,0};v.player_species=2;v.player_energy=v.max_energy=1000;v.current_weapon=4;v.predator_owned=(1u<<4)|(1u<<3);v.tick=10;
 v.x=(4u<<16)|32768;v.y=(5u<<16)|32768;v.target_x=(5u<<16)|32768;v.target_y=v.y;
 v.has_target=v.hostile_target=v.target_visible=v.target_is_egg=1;v.goal=AVP_MP_BOT_HOSTILE;
 v.prop_count=2;v.props[0]=(AvpMpBotProp){v.target_x,v.target_y,17408};v.props[1]=(AvpMpBotProp){(5u<<16),v.y,32768};
 avp_mp_bot_step_precise(&s,&v,0,&c);assert(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|(1u<<AVP_MP_PREV_WEAPON)));assert(!(c.actions&moves()));assert(!(c.actions&(1u<<AVP_MP_FIRE)));
 v.current_weapon=3;v.invisflag=1;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!(c.actions&(1u<<AVP_MP_FIRE)));
 v.invisflag=0;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(c.actions&(1u<<AVP_MP_FIRE));
 /* A wall margin intrusion should recover sideways toward the current
  * tile centre instead of repeating an invalid forward command. */
 memset(&s,0,sizeof(s));v.current_weapon=4;v.predator_owned=1u<<4;
 v.prop_count=1;v.y=(5u<<16)+18022;v.heading=0;v.tick=1;
 avp_mp_bot_step_precise(&s,&v,0,&c);
 assert((c.analog_axis_mask&3)==3 && c.analog_strafe>0.0f);
 /* Slow jitter must not reset the watchdog. Real damage and approach do. */
 memset(&s,0,sizeof(s));v.y=(5u<<16)|32768;v.x=(4u<<16)|32768;
 v.target_health_valid=1;v.target_health=80;v.target_slot=7;v.observed_world_level=14;
 const u32 base=v.x;
 for(unsigned t=1;t<=41;t++){v.tick=t;v.x=base+(t&1u)*200u;avp_mp_bot_egg_watch(&s,&v,11);}
 assert(avp_mp_bot_egg_avoided(&s,14,7,11,41));
 assert(!avp_mp_bot_egg_avoided(&s,14,7,12,41));
 assert(!avp_mp_bot_egg_avoided(&s,15,7,11,41));
 assert(!avp_mp_bot_egg_avoided(&s,14,8,11,41));
 assert(!avp_mp_bot_egg_avoided(&s,14,7,11,86));
 memset(&s,0,sizeof(s));v.deathmatch=1;
 for(unsigned t=1;t<=41;t++){v.tick=t;v.x=base+(t&1u)*200u;avp_mp_bot_egg_watch(&s,&v,11);}
 assert(avp_mp_bot_egg_avoided(&s,14,7,11,41));v.deathmatch=0;
 memset(&s,0,sizeof(s));v.x=base;
 for(unsigned t=1;t<=100;t++){v.tick=t;if(t%20==0)v.target_health--;avp_mp_bot_egg_watch(&s,&v,11);assert(!avp_mp_bot_egg_avoided(&s,14,7,11,t));}
 memset(&s,0,sizeof(s));v.x=base;v.target_health=80;
 for(unsigned t=1;t<=60;t++){v.tick=t;v.x=base+(t/20)*9000u;avp_mp_bot_egg_watch(&s,&v,11);assert(!avp_mp_bot_egg_avoided(&s,14,7,11,t));}
 /* A real nearby peer behind the bot is not an obstacle to the egg.
  * A peer between it and the egg falls back to ordinary safety policy. */
 memset(&s,0,sizeof(s));v.x=base+30000;v.y=(5u<<16)|32768;v.heading=0;
 v.current_weapon=4;v.predator_owned=1u<<4;v.tick=200;
 v.nearby_peer_valid=1;v.nearby_peer_x=base-20000;v.nearby_peer_y=v.y;
 v.prop_count=2;v.props[1]=(AvpMpBotProp){v.nearby_peer_x,v.nearby_peer_y,17408};
 avp_mp_bot_step_precise(&s,&v,0,&c);assert((c.analog_axis_mask&3)==3 && (c.actions&(1u<<AVP_MP_FIRE)) && !(c.actions&moves()));
 v.nearby_peer_x=base+45000;v.props[1].x=v.nearby_peer_x;
 v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert((c.analog_axis_mask&3)==0);
 puts("PASS clear co-op attack with nearby peer behind; peer in front retains ordinary safety policy");
 puts("PASS wall margin recovery, slow-jitter watchdog, generation/level/slot isolation, real approach and damage progress");
 printf("PASS %u Predator elbow cases, %u native movement ticks, %u stationary fire commands; owned ranged fallback and cloak gate\n",cases,frames,firing);return 0;
}
