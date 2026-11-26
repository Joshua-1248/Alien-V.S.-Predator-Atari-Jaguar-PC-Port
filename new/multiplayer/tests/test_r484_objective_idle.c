#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    AvpMpBotState s={0}; AvpMpBotObservation v={0}; AvpMpPlayerCommand cmd;
    uint8_t cells[16*8]={0},flags[256]={0};
    for(unsigned i=0;i<16;i++)cells[i*8+5]=1;
    v.map=(AvpMpBotMap){cells,16,1,flags,NULL,0,NULL,NULL,0};
    v.deathmatch=1;v.has_target=1;v.goal=AVP_MP_BOT_LIFT;
    v.player_species=AVP_MP_ALIEN;v.x=0x38000;v.y=0x8000;
    v.target_x=0x78000;v.target_y=v.y;v.observed_world_level=4;
    v.player_energy=v.max_energy=1125;
    /* Invalid alien lift is deliberately rejected; watchdog must not issue Use. */
    for(unsigned i=1;i<=21;i++){
        v.tick=i;avp_mp_bot_step_precise(&s,&v,0,&cmd);
        assert(cmd.actions==0);assert(s.objective_stall==i-1);
    }
    assert(s.physical_stall==0 && s.objective_stall==20);
    v.tick++;v.x+=256;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.objective_stall);
    v.tick++;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(s.objective_stall==1);
    v.tick+=2;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.objective_stall);
    v.tick++;v.observed_world_level=5;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.objective_stall);
    v.tick++;v.facehugged=1;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.objective_requested&&!s.objective_stall);
    v.facehugged=0;v.deathmatch=0;assert(!avp_mp_bot_objective_recoverable(&v));
    v.tick++;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.objective_requested);
    v.deathmatch=1;v.hostile_target=v.target_visible=1;assert(!avp_mp_bot_objective_recoverable(&v));
    v.hostile_target=v.target_visible=0;v.airlock_pressure=1;assert(!avp_mp_bot_objective_recoverable(&v));
    v.airlock_pressure=0;v.goal=AVP_MP_BOT_AIRLOCK_CLOSE;assert(!avp_mp_bot_objective_recoverable(&v));
    v.goal=AVP_MP_BOT_EXIT;assert(avp_mp_bot_objective_recoverable(&v));
    puts("PASS objective inactivity, displacement, gaps, level changes, combat, facehugger, Co-op and airlock exclusions");
    return 0;
}
