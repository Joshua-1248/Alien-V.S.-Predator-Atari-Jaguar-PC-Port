#include "local_mp_bot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define W 9u
#define P(x) (((x)<<16)|32768u)
static uint8_t cells[W*W*8],flags[256],risk[4096];
static uint16_t visits[4096];
static void run(unsigned sp,unsigned dm,unsigned fringe){
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpBotRoute route={0};unsigned tx,ty;
    memset(cells,0,sizeof cells);memset(risk,0,sizeof risk);memset(visits,0,sizeof visits);
    for(unsigned y=1;y<=7;y++)for(unsigned x=1;x<=7;x++)cells[(y*W+x)*8+5]=1;
    v.map=(AvpMpBotMap){cells,W,W,flags,NULL,0,risk,NULL,0};
    v.player_species=sp;v.deathmatch=dm;v.player_energy=1000;v.observed_world_level=4;
    v.tick=50;v.x=P(5)-16384u;v.y=P(4);v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;
    v.target_x=P(4)+(fringe?24576u:0u);v.target_y=v.y;
    v.acid_count=1;v.acid[0]=(AvpMpBotAcid){v.target_x,v.target_y};
    /* Last tick was inside this puddle; current tick is outside. */
    s.physical_valid=1;s.physical_tick=49;s.physical_level=4;s.physical_x=v.target_x;s.physical_y=v.y;
    avp_mp_bot_acid_risk_mark(risk,v.acid[0].x,v.acid[0].y);
    assert(!avp_mp_bot_acid_route_memory(&s,&v,risk)); /* open room, no choke */
    assert(s.acid_exit_count==1);
    assert(avp_mp_bot_route(&v.map,v.x>>16,4,v.target_x>>16,4,&route));
    assert(!avp_mp_bot_acid_goal_blocked(&v));
    assert(avp_mp_bot_acid_goal_unsafe(&s,&v));
    /* Same tile, safe exact target: do not mistake the entire tile for acid. */
    v.target_y=P(4)+24576u;assert(!avp_mp_bot_acid_goal_unsafe(&s,&v));v.target_y=P(4);
    /* Check the edge of the original contact square, not the sprite. */
    v.target_x=v.acid[0].x+(30u<<9);assert(avp_mp_bot_acid_goal_unsafe(&s,&v));
    v.target_x++;assert(!avp_mp_bot_acid_goal_unsafe(&s,&v));v.target_x=v.acid[0].x;
    v.goal=AVP_MP_BOT_HOSTILE;v.hostile_target=1;assert(!avp_mp_bot_acid_goal_unsafe(&s,&v));
    v.goal=AVP_MP_BOT_LIFT;v.hostile_target=0;v.lift_wait=1;assert(!avp_mp_bot_acid_goal_unsafe(&s,&v));v.lift_wait=0;
    v.goal=AVP_MP_BOT_SUPPLY;v.player_species=AVP_MP_ALIEN;assert(!avp_mp_bot_acid_goal_unsafe(&s,&v));v.player_species=sp;
    v.acid_count=0;assert(!avp_mp_bot_acid_goal_unsafe(&s,&v));v.acid_count=1;
    v.tick++;assert(!avp_mp_bot_acid_goal_unsafe(&s,&v));v.tick--;
    v.observed_world_level++;assert(!avp_mp_bot_acid_goal_unsafe(&s,&v));v.observed_world_level--;
    /* Only dangerous centre vs safe centre available. Never patrol into acid. */
    memset(visits,0xff,sizeof visits);visits[4*64+1]=0;visits[4*64+7]=100;
    memset(risk,0,sizeof risk);avp_mp_bot_acid_risk_mark(risk,P(1),P(4));
    assert(avp_mp_bot_patrol_pick(&v.map,5,4,visits,7,&tx,&ty));assert(tx==7 && ty==4);
    visits[4*64+7]=65535;assert(!avp_mp_bot_patrol_pick(&v.map,5,4,visits,7,&tx,&ty));
    memset(risk,0,sizeof risk);assert(avp_mp_bot_patrol_pick(&v.map,5,4,visits,7,&tx,&ty));assert(tx==1 && ty==4);
}
int main(void){for(unsigned sp=0;sp<=2;sp+=2)for(unsigned dm=0;dm<2;dm++)for(unsigned fringe=0;fringe<2;fringe++)run(sp,dm,fringe);puts("PASS 8 exact acid destination scenarios; safe same-cell point, patrol centre, contact boundary and lifetime controls");return 0;}
