/* RE #44 R293 -- real bot input regression for acid-adjacent blind spins.
 * This exercises the production command generator; the retail native
 * MovePlayer field replay remains the separate acceptance test. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CELL(X,Y,W) (((Y)*(W)+(X))*8u)
int main(void)
{
    uint8_t cells[8u*8u*8u]={0};
    uint8_t walls[256]={0};
    uint8_t risk[4096]={0};
    AvpMpBotState s={0};
    AvpMpBotObservation v={0};
    AvpMpPlayerCommand out={0};
    const unsigned motion=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    const unsigned turning=(1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT);
    unsigned moves=0u,spins=0u;
    for(unsigned x=1u;x<=6u;x++)cells[CELL(x,2u,8u)+5u]=1u;
    avp_mp_bot_acid_risk_mark(risk,(4u<<16)|0x8000u,(2u<<16)|0x8000u);
    v.map=(AvpMpBotMap){cells,8u,8u,walls,NULL,0u,risk,NULL,0u};
    v.player_species=AVP_MP_MARINE;v.player_index=3u;
    v.player_collision_disabled=1u;
    v.x=(3u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
    v.heading=0u;
    v.target_x=(6u<<16)|0x8000u;v.target_y=v.y;
    v.has_target=1u;v.goal=AVP_MP_BOT_FOLLOW;
    v.player_energy=100;v.max_energy=100;v.observed_world_level=1u;
    v.acid_count=1u;
    v.acid[0]=(AvpMpBotAcid){(4u<<16)|0x8000u,(2u<<16)|0x8000u};
    /* A previous failed move initiated a wall-recovery turn even though
     * the next observation contains a valid route through the corridor. */
    s.initialized=1u;s.previous_x=v.x;s.previous_y=v.y;
    s.last_motion=1u<<AVP_MP_FORWARD;s.evade_ticks=18u;
    for(unsigned tick=40u;tick<105u;tick++){
        v.tick=tick;
        avp_mp_bot_step(&s,&v,&out);
        if(out.actions&turning)spins++;
        if(out.actions&motion)moves++;
        /* A blocked original SafePos step: do not fake progress. */
    }
    printf("R293 acid corridor: turning=%u motion=%u evade=%u\n",
           spins,moves,s.evade_ticks);
    assert(spins==0u);
    assert(moves>=45u);
    assert(s.evade_ticks==0u);
    /* Clear the hazard: normal combat may turn to optically seen enemies. */
    v.acid_count=0u;v.map.risk=NULL;
    v.hostile_target=1u;v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;
    v.heading=0x40000000u;
    v.tick=105u;
    avp_mp_bot_step(&s,&v,&out);
    assert(out.actions&turning);
    puts("PASS: acid does not cause blind spin; real visual aiming remains");
    return 0;
}
