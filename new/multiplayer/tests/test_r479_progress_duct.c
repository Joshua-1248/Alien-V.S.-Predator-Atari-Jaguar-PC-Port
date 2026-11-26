#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    AvpMpBotDuctMemory m={0};
    avp_mp_bot_duct_arrived(&m,5,13,0x58000,0x58000,100);
    assert(avp_mp_bot_duct_reverse_block(&m,13,5,0x58000,0x58000,139));
    assert(avp_mp_bot_duct_reverse_block(&m,13,5,0x58000,0x58000,279));
    assert(!avp_mp_bot_duct_reverse_block(&m,13,5,0x58000,0x58000,280));
    assert(!avp_mp_bot_duct_reverse_block(&m,13,4,0x58000,0x58000,101));
    assert(!avp_mp_bot_duct_reverse_block(&m,13,5,0x78000,0x58000,140));
    assert(avp_mp_bot_duct_reverse_block(&m,13,5,0x78000,0x58000,139));
    assert(!avp_mp_bot_duct_reverse_block(&m,5,5,0x58000,0x58000,101));
    avp_mp_bot_duct_arrived(&m,4,5,0x58000,0x58000,200);assert(!m.level);
    uint8_t cells[16*8]={0},flags[256]={0}; for(unsigned i=0;i<16;i++)cells[i*8+5]=1;
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand cmd;
    v.map=(AvpMpBotMap){cells,16,1,flags,NULL,0,NULL,NULL,0};
    v.x=0x72000;v.y=0x8000;v.target_x=0x78000;v.target_y=v.y;
    v.goal=AVP_MP_BOT_AIRLOCK_CLOSE;v.has_target=v.airlock_door_ready=1;
    v.player_energy=v.max_energy=1000;v.target_side=2;v.observed_world_level=5;
    for(unsigned i=1;i<=10;i++){v.tick=i;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(s.physical_requested);assert(s.physical_stall==i-1);assert(!(cmd.actions&(1u<<AVP_MP_USE)));}
    v.tick=11;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(s.physical_stall==10);assert(!(cmd.actions&(1u<<AVP_MP_USE)));
    v.tick=12;v.x=v.target_x;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.physical_stall);assert(!s.physical_requested);
    v.tick=13;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.physical_stall);
    v.tick=14;v.x=0x72000;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(s.physical_requested);
    v.tick=20;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.physical_stall);
    v.tick=21;v.observed_world_level=6;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.physical_stall);
    v.tick=22;v.facehugged=1;avp_mp_bot_step_precise(&s,&v,0,&cmd);assert(!s.physical_stall&&!s.physical_requested);
    puts("PASS reverse-duct bounds/egress and physical progress across panel early returns, gaps, level changes, Use and facehugger");
}
