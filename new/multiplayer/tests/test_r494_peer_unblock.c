#include "../local_mp_bot.c"
#include <assert.h>
#include <stdio.h>
#define W 8u
#define MOVES ((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))
static uint8_t cells[W*W*8],flags[256];
static void setup(AvpMpBotObservation *v,AvpMpBotState *s,unsigned angle){
    memset(v,0,sizeof *v);memset(s,0,sizeof *s);memset(cells,0,sizeof cells);
    for(unsigned y=1;y<W-1;y++)for(unsigned x=1;x<W-1;x++)cells[(y*W+x)*8+5]=1;
    v->map=(AvpMpBotMap){cells,W,W,flags,NULL,0,NULL,NULL,0};
    v->x=0x384ef;v->y=0x3d9a0;v->nearby_peer_x=0x38000;v->nearby_peer_y=0x48000;
    v->target_x=0x38000;v->target_y=0x58000;v->heading=angle*0x20000000u;
    v->deathmatch=v->has_target=v->nearby_peer_valid=1;v->player_species=AVP_MP_MARINE;
    v->player_energy=v->max_energy=1000;v->goal=AVP_MP_BOT_PATROL;
    v->prop_count=1;v->props[0]=(AvpMpBotProp){v->nearby_peer_x,v->nearby_peer_y,AVP_MP_PEER_CLEARANCE};
    s->objective_stall=6;
}
int main(void){
    AvpMpBotObservation v;AvpMpBotState s;AvpMpPlayerCommand c;
    for(unsigned i=0;i<8;i++){
        setup(&v,&s,i);memset(&c,0,sizeof c);
        assert(bot_peer_unblock(&s,&v,&c));assert(c.actions&MOVES);assert((c.analog_axis_mask&3u)==3u);
        const double h=v.heading*(2.0*BOT_PI/4294967296.0),fx=cos(h),fy=-sin(h);
        const double dx=c.analog_forward*fx-c.analog_strafe*fy,dy=c.analog_forward*fy+c.analog_strafe*fx;
        assert(dy<0.0);assert(hypot(dx,dy)<0.251);
    }
    for(unsigned i=0;i<10;i++){
        setup(&v,&s,0);memset(&c,0,sizeof c);
        switch(i){case 0:v.deathmatch=0;break;case 1:s.objective_stall=5;break;
        case 2:v.facehugged=1;break;case 3:v.prop_count=0;break;case 4:v.player_collision_disabled=1;break;
        case 5:v.melee_threat=1;break;case 6:s.objective_wait=1;break;case 7:c.actions=1u<<AVP_MP_USE;break;
        case 8:v.acid_count=1;v.acid[0].x=v.x;v.acid[0].y=v.y;break;case 9:v.prop_count=2;v.props[1]=(AvpMpBotProp){v.x,v.y,0x8000};break;}
        assert(!bot_peer_unblock(&s,&v,&c));
    }
    puts("PASS 8 recorded-offset heading projections and 10 safety exclusions; synthetic floor");return 0;
}
