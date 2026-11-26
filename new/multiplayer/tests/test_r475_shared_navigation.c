/* Both mode observations through production planner; controlled coordinates,
 * not a live game or a replacement for native collision. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include "doors.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CELL(x,y) (8*((y)*7+(x)))
static uint8_t cells[7*7*8],flags[256];
static AvpMpBotObservation setup(unsigned dm,unsigned species,unsigned behavior){
 AvpMpBotObservation v={0};memset(cells,0,sizeof cells);memset(flags,0,sizeof flags);
 for(unsigned x=0;x<7;x++)for(unsigned y=0;y<7;y++)cells[CELL(x,y)+5]=1;
 v.map=(AvpMpBotMap){cells,7,7,flags,NULL,99,NULL,NULL,0};
 v.x=(2u<<16)|32768;v.y=(3u<<16)|32768;
 v.target_x=(5u<<16)|32768;v.target_y=v.y;
 v.has_target=1;v.goal=AVP_MP_BOT_PATROL;v.deathmatch=dm;
 v.player_species=species;v.behavior=behavior;v.player_energy=v.max_energy=1000;
 v.observed_world_level=14;return v;
}
int main(void){unsigned n=0;
 for(unsigned dm=0;dm<2;dm++)for(unsigned sp=0;sp<3;sp++)for(unsigned b=0;b<3;b++){
  AvpMpBotObservation v=setup(dm,sp,b);AvpMpBotState s={0};AvpMpPlayerCommand c;
  const uint32_t base=v.y; /* >0.25 tile sideways oscillation, never closer by 1/8 */
  for(unsigned t=1;t<=46;t++){v.tick=t;v.y=base+(t&1u?10000:-10000);avp_mp_bot_step_precise(&s,&v,0,&c);}
  assert(s.nav_bad_level==14 && s.nav_bad_next_plus_one==3+3*64+1 && s.nav_bad_until>v.tick && s.nav_retarget_patrol);
  memset(&s,0,sizeof s);v=setup(dm,sp,b);
  for(unsigned t=1;t<=100;t++){v.tick=t;v.x=(2u<<16)|32768;v.x+=t*250;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.nav_bad_until);}
  /* Door on the only available route: never fabricate a detour. */
  memset(&s,0,sizeof s);v=setup(dm,sp,b);
  /* single row for first edge: the only available path must stay legal */
  memset(cells,0,sizeof cells);for(unsigned x=1;x<=5;x++)cells[CELL(x,3)+5]=1;
  flags[5]=AVP_WB_DOOR;cells[CELL(2,3)+2]=5;cells[CELL(3,3)+0]=5;
  const unsigned off=CELL(3,3)+6;
  for(unsigned t=1;t<=61;t++){v.tick=t;avp_mp_bot_step_precise(&s,&v,0,&c);}
  assert(s.nav_bad_level==14 && s.nav_bad_next_plus_one==3+3*64+1 && s.nav_bad_until>v.tick && s.nav_retarget_patrol);
  /* A different map at identical grid coords must not inherit avoidance. */
  v.observed_world_level=15;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(s.door_wait_level==15 && s.door_wait_tick==v.tick);
  memset(&s,0,sizeof s);v.observed_world_level=14;
  for(unsigned t=1;t<=100;t++){v.tick=t;cells[off]=(uint8_t)(t/10);avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.nav_bad_until);}
  /* Original animation reaches open threshold; no repeat USE. */
  cells[off]=48;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.door_wait_valid && !(c.actions&(1u<<AVP_MP_USE)));
  n++;
 }
 for(unsigned dm=0;dm<2;dm++)for(unsigned sp=0;sp<=2;sp+=2)for(unsigned b=0;b<3;b++){
  AvpMpBotObservation v=setup(dm,sp,b);AvpMpBotState s={0};AvpMpPlayerCommand c;
  v.current_weapon=2;v.marine_inventory_valid=sp==0;v.marine_owned=1u<<2;v.marine_ammo[1]=100;v.predator_owned=1u<<2;
  v.tick=1;avp_mp_bot_step_precise(&s,&v,0,&c);assert(s.travel_valid);
  v.goal=AVP_MP_BOT_HOSTILE;v.hostile_target=v.target_visible=v.target_energy_positive=1;
  v.target_x=v.x;v.target_y=v.y-2*65536;v.heading=0x40000000;
  const uint32_t base=v.y;unsigned fire=0;
  for(unsigned t=2;t<=47;t++){v.tick=t;v.y=base+(t&1u?500:-500);avp_mp_bot_step_precise(&s,&v,0,&c);if(c.actions&(1u<<AVP_MP_FIRE))fire++;}
  assert(!s.travel_valid && s.nav_bad_level==14 && s.nav_bad_until>v.tick);assert(fire);
 }
 for(unsigned dm=0;dm<2;dm++){
  AvpMpBotObservation v=setup(dm,0,0);AvpMpBotState s={0};AvpMpPlayerCommand c;AvpMpBotRoute route={0};
  memset(cells,0,sizeof cells);for(unsigned x=2;x<=5;x++){cells[CELL(x,3)+5]=1;cells[CELL(x,1)+5]=1;}
  cells[CELL(2,2)+5]=cells[CELL(5,2)+5]=1;
  flags[5]=AVP_WB_DOOR;cells[CELL(2,3)+2]=5;cells[CELL(3,3)+0]=5;
  assert(avp_mp_bot_route(&v.map,2,3,5,3,&route) && route.requires_door_use);
  for(unsigned t=1;t<=61;t++){v.tick=t;avp_mp_bot_step_precise(&s,&v,0,&c);}
  assert(s.nav_bad_until>v.tick);v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);
  assert(s.nav_anchor_next==2+2*64+1 && !(c.actions&(1u<<AVP_MP_USE)));
  v.observed_world_level=15;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);
  assert(s.door_wait_valid && s.door_wait_level==15);
 }
 puts("PASS actual alternate-route selection after blocked door, both modes, no cross-level blacklist");
 puts("PASS 12 co-op/Deathmatch ranged combat-travel stalls: stale route retired, native firing retained");
 printf("PASS %u mode/species/profile groups: lateral jitter, real approach, stalled door, level isolation, progressing/open door\n",n);return 0;}
