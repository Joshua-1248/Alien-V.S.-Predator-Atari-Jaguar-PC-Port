#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define FIRE (1u<<AVP_MP_FIRE)
#define CLOAK (1u<<AVP_MP_PRED_CLOAK)
static void setup(AvpMpBotState *s,AvpMpBotObservation *v){
    memset(s,0,sizeof *s);memset(v,0,sizeof *v);
    v->player_species=AVP_MP_PREDATOR;v->deathmatch=1;v->behavior=2;
    v->has_target=v->hostile_target=v->target_visible=1;
    v->goal=AVP_MP_BOT_HOSTILE;v->predator_owned=1u<<3;v->current_weapon=3;
    v->x=10u<<16;v->y=10u<<16;v->target_x=17u<<16;v->target_y=v->y;
    v->player_energy=v->max_energy=1000;v->dm_opponents_on_level=1;
    v->invis_act=255;v->invis_stat=2;v->invisflag=1;v->tick=1;
}
int main(void){AvpMpBotState s;AvpMpBotObservation v;AvpMpPlayerCommand c;
    setup(&s,&v);avp_mp_bot_step_precise(&s,&v,0,&c);assert(c.actions&CLOAK);assert(!(c.actions&FIRE));
    v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!(c.actions&(CLOAK|FIRE)));
    v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(c.actions&CLOAK);
    v.invis_act=0;v.invis_stat=1;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!(c.actions&FIRE));
    v.invis_stat=v.invisflag=0;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(c.actions&FIRE);assert(!(c.actions&CLOAK));
    for(unsigned mode=0;mode<3;mode++){
        setup(&s,&v);if(mode==1){v.invis_act=0;v.invis_stat=1;}if(mode==2){v.invis_act=v.invis_stat=0;}
        for(unsigned t=1;t<=8;t++){v.tick=t;avp_mp_bot_step_precise(&s,&v,0,&c);if(t<8)assert(!(c.actions&FIRE));else assert(c.actions&FIRE);}
        assert(!(c.actions&CLOAK));v.target_visible=0;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!(c.actions&FIRE));
        v.target_visible=1;v.tick++;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!(c.actions&FIRE));
    }
    setup(&s,&v);v.heading=0x80000000u;for(unsigned t=1;t<12;t++){v.tick=t;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!(c.actions&FIRE));}
    setup(&s,&v);v.behavior=0;for(unsigned t=1;t<12;t++){v.tick=t;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!(c.actions&FIRE));}
    setup(&s,&v);v.deathmatch=0;v.target_x=v.x+(2u<<16);v.target_is_alien=1;v.target_slot=1;v.other_player_splash_unsafe=1;
    for(unsigned t=1;t<12;t++){v.tick=t;avp_mp_bot_step_precise(&s,&v,0,&c);assert(!(c.actions&FIRE));}
    puts("PASS Predator cloak request, edge retry, native fade, stuck flag fallback, aim/range/visibility/splash exclusions");return 0;
}
