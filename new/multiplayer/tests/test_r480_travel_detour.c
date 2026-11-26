#include "local_mp_bot.h"
#include "local_mp_input.h"
#include "doors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CELL(x,y) (8*((y)*7+(x)))
static uint8_t cells[7*7*8],flags[256];
int main(void){unsigned count=0;
 for(unsigned dm=0;dm<2;dm++)for(unsigned lift=0;lift<2;lift++)for(unsigned alternate=0;alternate<2;alternate++){
  memset(cells,0,sizeof cells);memset(flags,0,sizeof flags);
  for(unsigned x=2;x<=5;x++){cells[CELL(x,3)+5]=1;if(alternate)cells[CELL(x,1)+5]=1;}
  if(alternate)cells[CELL(2,2)+5]=cells[CELL(5,2)+5]=1;
  flags[5]=AVP_WB_DOOR;cells[CELL(2,3)+2]=5;cells[CELL(3,3)+0]=5;
  AvpMpBotObservation v={0};AvpMpBotState s={0};AvpMpPlayerCommand c;
  v.map=(AvpMpBotMap){cells,7,7,flags,NULL,99,NULL,NULL,0};
  v.x=0x28000;v.y=0x38000;v.target_x=0x58000;v.target_y=v.y;
  v.has_target=1;v.goal=lift?AVP_MP_BOT_LIFT:AVP_MP_BOT_EXIT;v.deathmatch=dm;
  v.player_energy=v.max_energy=1000;v.observed_world_level=4;v.target_side=2;
  for(unsigned i=1;i<=61;i++){v.tick=i;avp_mp_bot_step_precise(&s,&v,0,&c);}
  assert(s.nav_bad_until>v.tick && s.nav_bad_next_plus_one==3+3*64+1);
  v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);
  if(alternate)assert(s.nav_anchor_next==2+2*64+1 && !s.door_wait_valid && !(c.actions&(1u<<AVP_MP_USE)));
  else assert(s.door_wait_valid && s.door_next==3+3*64);
  /* At the actual panel the bad-cell hint must not veto legitimate Use. */
  s.nav_bad_next_plus_one=5+3*64+1;v.x=v.target_x;v.tick++;v.heading=0;
  avp_mp_bot_step_precise(&s,&v,0,&c);
  assert(c.actions & (1u<<AVP_MP_USE));count++;
 }
 printf("PASS %u lift/exit travel cases: stalled door, alternate route, sole-path fallback and original panel Use\n",count);return 0;
}
