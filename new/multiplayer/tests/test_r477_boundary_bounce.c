/* Goal-independent A/B boundary oscillation, with actual observations.
 * No synthetic collision or teleport is added to the game. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CELL(x,y) (8*((y)*9+(x)))
static uint8_t cells[9*9*8],flags[256];
static AvpMpBotObservation view(unsigned dm,unsigned sp,unsigned b){
 AvpMpBotObservation v={0};memset(cells,0,sizeof cells);
 for(unsigned i=1;i<8;i++){cells[CELL(i,4)+5]=1;cells[CELL(4,i)+5]=1;}
 v.map=(AvpMpBotMap){cells,9,9,flags,NULL,0,NULL,NULL,0};v.player_species=sp;v.deathmatch=dm;v.behavior=b;v.player_energy=v.max_energy=1000;v.player_collision_disabled=1;v.has_target=1;v.goal=AVP_MP_BOT_PATROL;v.observed_world_level=4;return v;
}
int main(void){unsigned n=0;for(unsigned dm=0;dm<2;dm++)for(unsigned sp=0;sp<3;sp++)for(unsigned b=0;b<3;b++){
 AvpMpBotObservation v=view(dm,sp,b);AvpMpBotState s={0};AvpMpPlayerCommand c;
 /* Crossing the same border repeatedly with changing goals must not buy unlimited time. */
 for(unsigned t=1;t<=8;t++){v.tick=t;v.x=(t&1)?(4u<<16)-64:(4u<<16)+64;v.y=(4u<<16)|32768;v.target_x=((t&1)?7u:1u)<<16|32768;v.target_y=v.y;avp_mp_bot_step_precise(&s,&v,0,&c);}
 assert(s.nav_bad_until>v.tick && s.nav_retarget_patrol);
 /* One legitimate reversal cannot cause an avoidance. */
 memset(&s,0,sizeof s);v=view(dm,sp,b);
 for(unsigned t=1;t<=7;t++){v.tick=t;v.x=((t<=4?t:8-t)<<16)|32768;v.y=(4u<<16)|32768;v.target_x=(t<=4?7u:1u)<<16|32768;v.target_y=v.y;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.nav_bad_until);}
 /* Different level / inactive gap must discard stale boundary history. */
 memset(&s,0,sizeof s);v=view(dm,sp,b);
 for(unsigned t=1;t<=12;t++){v.tick=t;v.observed_world_level=(t<6?4:5);if(t>=9)v.tick+=20;v.x=(t&1)?(4u<<16)-64:(4u<<16)+64;v.y=(4u<<16)|32768;v.target_x=((t&1)?7u:1u)<<16|32768;v.target_y=v.y;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.nav_bad_until);}
 n++;}printf("PASS %u mode/species/profile groups, 3 boundary bounce scenarios each\n",n);return 0;}
