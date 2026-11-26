#include "local_mp_bot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define W 16u
#define P(x) (((x)<<16)|32768u)
static uint8_t cells[W*5*8],flags[256],risk[4096];
static uint16_t visits[4096],labels[4096];
static void floor_at(unsigned x,unsigned y){cells[(y*W+x)*8+5]=1;}
static void fresh(AvpMpBotObservation *v){memset(risk,0,sizeof risk);for(unsigned i=0;i<v->acid_count;i++)avp_mp_bot_acid_risk_mark(risk,v->acid[i].x,v->acid[i].y);}
static void scenario(unsigned sp,unsigned dm,unsigned reverse){
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpBotRoute r;unsigned tx,ty;
    memset(cells,0,sizeof cells);memset(visits,0,sizeof visits);
    for(unsigned x=1;x<=14;x++)floor_at(x,2);
    v.map=(AvpMpBotMap){cells,W,5,flags,NULL,0,risk,NULL,0};
    v.player_species=sp;v.deathmatch=dm;v.player_energy=1000;v.observed_world_level=4;
    v.tick=50;v.x=P(reverse?6:8);v.y=P(2);v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;
    v.target_x=P(reverse?14:1);v.target_y=v.y;v.acid_count=1;v.acid[0]=(AvpMpBotAcid){P(7),P(2)};
    s.physical_valid=1;s.physical_tick=49;s.physical_level=4;s.physical_x=P(7);s.physical_y=v.y;
    /* One real previous contact, followed by a 0.8-tile exit. */
    s.physical_x=reverse?P(7)-13107:P(7)+13107;
    fresh(&v);assert(avp_mp_bot_route(&v.map,v.x>>16,2,v.target_x>>16,2,&r));
    assert(avp_mp_bot_acid_route_memory(&s,&v,risk)==1);
    assert(risk[2*64+7]==3 && s.acid_exit_count==1);
    assert(!avp_mp_bot_route(&v.map,v.x>>16,2,v.target_x>>16,2,&r));
    assert(avp_mp_bot_acid_goal_blocked(&v));
    assert(avp_mp_bot_components(&v.map,labels)==2);
    assert(avp_mp_bot_patrol_pick(&v.map,v.x>>16,2,visits,123,&tx,&ty));
    assert(ty==2 && (reverse?tx<7:tx>7) && tx!=(v.x>>16));
    assert(avp_mp_bot_route(&v.map,v.x>>16,2,tx,ty,&r));
    v.goal=AVP_MP_BOT_HOSTILE;v.hostile_target=1;v.target_visible=1;assert(!avp_mp_bot_acid_goal_blocked(&v));
    v.goal=AVP_MP_BOT_LIFT;v.hostile_target=v.target_visible=0;v.lift_wait=1;assert(!avp_mp_bot_acid_goal_blocked(&v));v.lift_wait=0;
    v.goal=AVP_MP_BOT_SUPPLY;
    /* A real authored bypass around the choke must remain available. */
    for(unsigned x=5;x<=9;x++)floor_at(x,1);
    fresh(&v);assert(avp_mp_bot_acid_route_memory(&s,&v,risk)==0);
    assert(avp_mp_bot_route(&v.map,v.x>>16,2,v.target_x>>16,2,&r));
    /* Removal of the original acid actor restores the ordinary route. */
    v.acid_count=0;fresh(&v);assert(!avp_mp_bot_acid_route_memory(&s,&v,risk));assert(!s.acid_exit_count);
    assert(avp_mp_bot_route(&v.map,v.x>>16,2,v.target_x>>16,2,&r));
    /* New level / nonconsecutive observation cannot inherit old acid. */
    v.acid_count=1;v.observed_world_level=5;fresh(&v);assert(!avp_mp_bot_acid_route_memory(&s,&v,risk));
    v.observed_world_level=4;v.player_species=AVP_MP_ALIEN;fresh(&v);assert(!avp_mp_bot_acid_route_memory(&s,&v,risk));
}
int main(void){for(unsigned sp=0;sp<=2;sp+=2)for(unsigned dm=0;dm<2;dm++)for(unsigned rev=0;rev<2;rev++)scenario(sp,dm,rev);
    puts("PASS 8 protected-choke objective scenarios, alternative patrol, bypass, combat/interaction and lifetime controls");return 0;}
