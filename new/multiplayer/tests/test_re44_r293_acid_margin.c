/* R293 regression: original position just inside a conservative bot
 * wall-margin band must permit an inward, collision-legal correction. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
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
    const unsigned movement=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    const unsigned turning=(1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT);
    unsigned inward=0u;
    for(unsigned x=1u;x<=6u;x++)cells[CELL(x,2u,8u)+5u]=1u;
    avp_mp_bot_acid_risk_mark(risk,(4u<<16)|0x8000u,(2u<<16)|0x8000u);
    v.map=(AvpMpBotMap){cells,8u,8u,walls,NULL,0u,risk,NULL,0u};
    v.player_species=AVP_MP_MARINE;v.player_index=3u;
    v.player_collision_disabled=1u;
    v.x=(uint32_t)(3.527267*65536.0);v.y=(uint32_t)(2.197342*65536.0);
    v.heading=0xf7000000u;
    v.target_x=(6u<<16)|0x8000u;v.target_y=(2u<<16)|0x8000u;
    v.has_target=1u;v.goal=AVP_MP_BOT_FOLLOW;
    v.player_energy=100;v.max_energy=100;v.observed_world_level=1u;
    v.acid_count=1u;
    v.acid[0]=(AvpMpBotAcid){(4u<<16)|0x8000u,(2u<<16)|0x8000u};
    /* A blocking step is not a reason to spin. Command must first move
     * toward the corridor center, without noclip or forced state edits. */
    for(unsigned tick=100u;tick<104u;tick++){
        v.tick=tick;
        avp_mp_bot_step(&s,&v,&out);
        printf("tick=%u action=%08x y=%f\n",tick,out.actions,(double)v.y/65536.0);
        if(out.actions&(1u<<AVP_MP_STRAFE_RIGHT))inward++;
        assert(!(out.actions&turning));
        assert(out.actions&movement);
        if(out.actions&(1u<<AVP_MP_STRAFE_RIGHT)){
            v.x-=(uint32_t)(0.019*65536.0);
            v.y+=(uint32_t)(0.065*65536.0);
        }else if(out.actions&(1u<<AVP_MP_FORWARD)){
            v.x+=(uint32_t)(0.06*65536.0);
            v.y+=(uint32_t)(0.01*65536.0);
        }
    }
    assert(inward>0u);
    assert((double)v.y/65536.0>2.30);
    puts("PASS: pre-existing wall-margin intrusion can be corrected without spinning");
    return 0;
}
