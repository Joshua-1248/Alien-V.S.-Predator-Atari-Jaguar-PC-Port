/* Recorded R491 peer-pair position, synthetic floor/prop surroundings. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include "local_mp_move_guard.h"
#include "amp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define W 8u
#define MOVES ((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))
#define ATTACKS ((1u<<AVP_MP_ALIEN_CLAW)|(1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL))
static uint8_t cells[W*W*8],flags[256];
static void setup(AvpMpBotObservation *v,unsigned orientation){
    memset(v,0,sizeof *v);memset(cells,0,sizeof cells);
    for(unsigned y=1;y<W-1;y++)for(unsigned x=1;x<W-1;x++)cells[(y*W+x)*8+5]=1;
    v->map=(AvpMpBotMap){cells,W,W,flags,NULL,0,NULL,NULL,0};
    int32_t dx=0x7b08,dy=-0xa24a;
    if(orientation&4u)dx=-dx;
    for(unsigned i=0;i<(orientation&3u);i++){int32_t temp=dx;dx=-dy;dy=temp;}
    v->x=(3u<<16)|0x50df;v->y=(3u<<16)|0x466a;
    v->target_x=v->x+dx;v->target_y=v->y+dy;
    v->heading=(uint32_t)(int64_t)(atan2(-(double)dy,dx)*4294967296.0/6.283185307179586);
    v->player_species=AVP_MP_ALIEN;v->goal=AVP_MP_BOT_HOSTILE;
    v->has_target=v->hostile_target=v->target_visible=v->deathmatch=1;
    v->player_energy=v->max_energy=1125;v->observed_world_level=4;v->tick=1;
    v->target_slot=AVP_NUM_AMPS+8;v->nearby_peer_valid=1;v->nearby_peer_index=8;
    v->nearby_peer_x=v->target_x;v->nearby_peer_y=v->target_y;
    v->prop_count=1;v->props[0]=(AvpMpBotProp){v->target_x,v->target_y,AVP_MP_PEER_CLEARANCE};
}
static int apply_projection(AvpMpBotObservation *v,const AvpMpPlayerCommand *c){
    double angle=v->heading*(6.283185307179586/4294967296.0);
    double x=0.26*(c->analog_forward*cos(angle)+c->analog_strafe*sin(angle));
    double y=0.26*(-c->analog_forward*sin(angle)+c->analog_strafe*cos(angle));
    uint32_t nx=v->x+(int32_t)(65536*x),ny=v->y+(int32_t)(65536*y);
    if(llabs((long long)nx-v->target_x)<AVP_MP_PEER_CLEARANCE && llabs((long long)ny-v->target_y)<AVP_MP_PEER_CLEARANCE)return 0;
    v->x=nx;v->y=ny;
    v->heading=(uint32_t)(int64_t)(atan2((double)v->y-v->target_y,(double)v->target_x-v->x)*4294967296.0/6.283185307179586);
    return 1;
}
int main(void){
    unsigned completed=0,aligned=0;
    for(unsigned i=0;i<8;i++){
        AvpMpBotObservation v;AvpMpBotState s={0};AvpMpPlayerCommand c;
        setup(&v,i);
        for(unsigned t=1;t<=30;t++){
            v.tick=t;avp_mp_bot_step_precise(&s,&v,0,&c);
            if(c.actions&ATTACKS){completed++;break;}
#ifndef BASELINE
            if(s.peer_melee_align){aligned++;assert(c.actions&MOVES);assert((c.analog_axis_mask&3u)==3u);assert(apply_projection(&v,&c));}
#else
            (void)apply_projection(&v,&c);
#endif
        }
    }
    printf("peer-pair projected alignment: %u/8 attack commands, %u alignment commands\n",completed,aligned);
#ifndef BASELINE
    assert(completed==8&&aligned>=8);
    for(unsigned k=0;k<9;k++){
        AvpMpBotObservation v;AvpMpBotState s={0};AvpMpPlayerCommand c;setup(&v,0);
        if(k==0)v.deathmatch=0;
        if(k==1)v.target_visible=0;
        if(k==2)v.facehugged=1;
        if(k==3)v.player_collision_disabled=1;
        if(k==4)v.target_slot=1;
        if(k==5)v.prop_count=0;
        if(k==6)v.prop_count=2,v.props[1]=(AvpMpBotProp){v.x,v.y,0x8000};
        if(k==7)v.prop_count=AVP_MP_BOT_MAX_NEAR_PROPS;
        if(k==8)v.player_species=AVP_MP_PREDATOR;
        avp_mp_bot_step_precise(&s,&v,0,&c);assert(!s.peer_melee_align);
    }
    puts("PASS nine policy/obstacle exclusions");
#endif
    return 0;
}
