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
  /* Two changing targets share the same physical first hop. */
  for(unsigned t=1;t<=46;t++){
   v.tick=t;v.target_x=((t&1u?5u:6u)<<16)|32768;
   avp_mp_bot_step_precise(&s,&v,0,&c);
  }
  assert(s.nav_bad_until>v.tick && s.nav_retarget_patrol);
  /* Switch between two orthogonal hops without moving. */
  memset(&s,0,sizeof s);v=setup(dm,sp,b);
  for(unsigned t=1;t<=46;t++){
   v.tick=t;v.target_x=t&1u?((5u<<16)|32768):v.x;
   v.target_y=t&1u?v.y:((6u<<16)|32768);
   avp_mp_bot_step_precise(&s,&v,0,&c);
  }
  assert(s.nav_bad_until>v.tick && s.nav_retarget_patrol);
  /* Real forward progress should still earn time despite retargeting. */
  memset(&s,0,sizeof s);v=setup(dm,sp,b);
  for(unsigned t=1;t<=100;t++){
   v.tick=t;v.target_x=((t&1u?5u:6u)<<16)|32768;v.x=((2u<<16)|32768)+t*250u;
   avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.nav_bad_until);
  }
  /* Pausing the route planner for a long interaction must not spend the budget. */
  memset(&s,0,sizeof s);v=setup(dm,sp,b);v.tick=1;
  avp_mp_bot_step_precise(&s,&v,0,&c);v.tick=100;
  avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.nav_bad_until);
  /* A disconnected patrol island must request an immediate replacement. */
  memset(&s,0,sizeof s);v=setup(dm,sp,b);memset(cells,0,sizeof cells);
  cells[CELL(2,3)+5]=cells[CELL(5,3)+5]=1;v.tick=1;
  avp_mp_bot_step_precise(&s,&v,0,&c);assert(s.nav_retarget_patrol && !s.nav_bad_until);
  /* A disconnected authored transition is not an ordinary patrol. */
  memset(&s,0,sizeof s);v.goal=AVP_MP_BOT_EXIT;v.tick++;
  avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.nav_retarget_patrol);
  n++;
 }
 printf("PASS %u mode/species/profile groups, six cases each: goal/hop churn, real progress, inactive interval, disconnected patrol, protected exit\n",n);
 return 0;
}
