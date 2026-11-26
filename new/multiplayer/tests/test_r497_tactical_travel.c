#include "local_mp_bot.h"
#include "local_mp_input.h"
#include "doors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define W 10u
#define P(x) ((uint32_t)((x)*65536u+32768u))
static uint8_t cells[W*W*8],flags[256],risk[4096];
static void setup(AvpMpBotState *s,AvpMpBotObservation *v){
    memset(s,0,sizeof *s);memset(v,0,sizeof *v);memset(cells,0,sizeof cells);memset(risk,0,sizeof risk);
    for(unsigned y=1;y<W-1;y++)for(unsigned x=1;x<W-1;x++)cells[(y*W+x)*8+5]=1;
    v->map=(AvpMpBotMap){cells,W,W,flags,NULL,0,risk,NULL,0};
    v->x=P(5);v->y=P(5);v->melee_x=P(4);v->melee_y=P(5);
    v->target_x=P(2);v->target_y=P(5);v->goal=AVP_MP_BOT_LIFT;
    v->deathmatch=v->marine_unarmed=v->marine_inventory_valid=v->has_target=v->melee_visible=1;
    v->melee_threat=AVP_MP_BOT_THREAT_ALIEN;v->player_species=AVP_MP_MARINE;
    v->player_energy=v->max_energy=1000;v->observed_world_level=5;
    s->physical_valid=1;s->physical_level=5;s->physical_x=v->x;s->physical_y=v->y;
}
static int tick(AvpMpBotState *s,AvpMpBotObservation *v){
    v->tick++;int result=avp_mp_bot_tactical_travel(s,v);
    s->physical_tick=v->tick;s->physical_x=v->x;s->physical_y=v->y;return result;
}
/* Single corridor with a real first-edge closed-door flag. There is a
 * reachable dry refuge to the east, but the original left wall margin can
 * prevent all native movement until the door opens. */
static void door_fixture(AvpMpBotState *s,AvpMpBotObservation *v){
    setup(s,v);
    memset(cells,0,sizeof cells);memset(flags,0,sizeof flags);
    for(unsigned x=1;x<W-1;x++)cells[(5u*W+x)*8u+5u]=1;
    cells[(5u*W+5u)*8u]=64u;cells[(5u*W+4u)*8u+2u]=64u;
    flags[64]=AVP_WB_DOOR|AVP_WB_TBDOOR;
    v->x=(5u<<16)|0x3000u;v->target_x=P(4);v->melee_x=P(4);
    s->physical_x=v->x;
}
int main(void){
    AvpMpBotState s;AvpMpBotObservation v;
    for(unsigned kind=0;kind<2;kind++){
        setup(&s,&v);if(kind)v.goal=AVP_MP_BOT_EXIT;
        for(unsigned i=0;i<10;i++)assert(!tick(&s,&v));
        assert(tick(&s,&v));assert(v.goal==AVP_MP_BOT_PATROL && v.retreat_to_refuge);
        assert(v.target_x>P(5));assert(s.tactical_retargets==1);
        uint32_t x=v.target_x,y=v.target_y;v.goal=AVP_MP_BOT_LIFT;v.target_x=P(2);v.target_y=P(5);
        assert(tick(&s,&v));assert(v.target_x==x && v.target_y==y && s.tactical_retargets==1);
        v.melee_visible=0;assert(!tick(&s,&v));assert(!s.tactical_until);
    }
    for(unsigned k=0;k<12;k++){
        setup(&s,&v);
        switch(k){case 0:v.deathmatch=0;break;case 1:v.marine_unarmed=0;break;
        case 2:v.melee_visible=0;break;case 3:v.lift_wait=1;break;
        case 4:s.objective_wait=2;break;case 5:v.facehugged=1;break;
        case 6:v.airlock_pressure=1;break;case 7:v.target_x=P(7);break;
        case 8:v.acid_count=1;v.acid[0].x=v.x;v.acid[0].y=v.y;break;
        case 9:v.goal=AVP_MP_BOT_MEDICAL;break;case 10:v.player_species=AVP_MP_ALIEN;break;
        case 11:for(unsigned i=0;i<4096;i++)risk[i]=3;break;}
        for(unsigned i=0;i<25;i++)assert(!tick(&s,&v));
        assert(!s.tactical_retargets);
    }
    setup(&s,&v);for(unsigned i=0;i<20;i++){v.y+=256;assert(!tick(&s,&v));}
    setup(&s,&v);for(unsigned i=0;i<10;i++)assert(!tick(&s,&v));v.tick+=2;assert(!tick(&s,&v));assert(!s.tactical_idle);
    door_fixture(&s,&v);
    for(unsigned i=0;i<30;i++)assert(!tick(&s,&v));
    assert(tick(&s,&v)); /* failed interaction cannot defer refuge forever */
    door_fixture(&s,&v);v.x=P(5);s.physical_x=v.x;
    for(unsigned i=0;i<10;i++)assert(!tick(&s,&v));
    assert(tick(&s,&v)); /* centered pose needs no additional wait */
    door_fixture(&s,&v);
    for(unsigned i=0;i<15;i++)assert(!tick(&s,&v));
    cells[(5u*W+5u)*8u+6u]=64u;
    assert(tick(&s,&v)); /* open door removes the reason for deferment */
    door_fixture(&s,&v);
    for(unsigned i=0;i<15;i++)assert(!tick(&s,&v));
    v.active_door_count=1;v.active_door_offsets[0]=(5u*W+5u)*8u+6u;
    for(unsigned i=0;i<30;i++)assert(!tick(&s,&v));
    assert(!s.tactical_retargets); /* no interaction reversal */
    puts("PASS lift/duct stall, commitment, 12 exclusions, displacement, continuity, door overlap, timeout, clear pose and active animation");return 0;
}
