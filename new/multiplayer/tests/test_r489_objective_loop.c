#include "local_mp_bot.h"
#include <assert.h>
#include <stdio.h>
#define P(x) ((uint32_t)((x)*65536u+32768u))
static void tick(AvpMpBotState *s,AvpMpBotObservation *v,unsigned n,uint32_t x,uint32_t y){
    v->tick=n;v->x=x;v->y=y;
    avp_mp_bot_loop_observe(s,v);
    s->physical_valid=1;s->physical_level=v->observed_world_level;
    s->physical_tick=v->tick;s->physical_x=x;s->physical_y=y;s->physical_requested=1;
}
static void run(unsigned scenario){
    AvpMpBotState s={0};AvpMpBotObservation v={0};
    v.deathmatch=1;v.has_target=1;v.goal=AVP_MP_BOT_EXIT;v.player_energy=1000;
    v.target_x=P(12);v.target_y=P(4);v.observed_world_level=4;s.objective_route=1;
    if(scenario==1)v.deathmatch=0;
    if(scenario==2)v.target_x=P(4)+8192;
    if(scenario==3)s.objective_wait=1;
    if(scenario==4)v.lift_wait=1;
    if(scenario==5)v.hostile_target=v.target_visible=1;
    if(scenario==6)v.facehugged=1;
    if(scenario==7)s.objective_route=2;
    for(unsigned i=0;i<=40;i++){
        uint32_t x=P(4)+(i%2)*8192u,y=P(4);
        if(scenario==8)x=P(4)-i*4096u; /* genuine detour away from target */
        if(scenario==9)x=P(4)+i*4096u; /* approach */
        if(scenario==10&&i==20)v.target_x=P(13);
        if(scenario==11&&i==20)v.observed_world_level=5;
        if(scenario==12)x=P(4); /* inactivity handled by existing watchdog */
        if(scenario==13&&i==20)s.physical_tick-=2;
        tick(&s,&v,i+1,x,y);
    }
    v.tick++;
    assert(!!avp_mp_bot_loop_ready(&s,&v)==(scenario==0));
    if(scenario==0){v.target_x++;assert(!avp_mp_bot_loop_ready(&s,&v));}
}
int main(void){for(unsigned i=0;i<14;i++)run(i);puts("PASS 14 compact-loop, detour, progress, continuity, combat, wait and Co-op controls");return 0;}
