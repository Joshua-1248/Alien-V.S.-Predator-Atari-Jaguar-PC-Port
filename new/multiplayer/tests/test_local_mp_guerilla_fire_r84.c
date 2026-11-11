/* R84: Guerilla movement cannot throttle a valid ranged FIRE command.
 * This is a planner/output test; original Jaguar weapon hit/damage remains
 * the only authority after a button press reaches native gameplay. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ACT(k) (1u<<(k))
#define ALIEN_FIRE (ACT(AVP_MP_ALIEN_CLAW)|ACT(AVP_MP_ALIEN_BITE)|ACT(AVP_MP_ALIEN_TAIL))
static AvpMpBotObservation observed(unsigned species,unsigned weapon,unsigned distance)
{
    AvpMpBotObservation v={0};
    v.x=10u<<16;v.y=10u<<16;
    v.target_x=v.x+(distance<<16);v.target_y=v.y;
    v.heading=0u;v.player_species=species;v.current_weapon=weapon;
    v.behavior=2u;v.goal=AVP_MP_BOT_HOSTILE;
    v.has_target=1u;v.hostile_target=1u;v.target_visible=1u;
    v.tick=31u;v.deathmatch=1u;
    if(species==2u)v.predator_owned=(1u<<1)|(1u<<2)|(1u<<3)|(1u<<4);
    if(species==0u){
        v.marine_inventory_valid=1u;v.marine_owned=0x1eu;
        for(unsigned k=0;k<4;k++)v.marine_ammo[k]=50u;
    }
    return v;
}
static void step(AvpMpBotObservation *v,AvpMpPlayerCommand *c)
{
    AvpMpBotState s={0};
    avp_mp_bot_step(&s,v,c);
}
int main(void)
{
    AvpMpPlayerCommand c;
    AvpMpBotObservation v;
    /* All four Marine ranged weapon slots, far outside the legacy 6-tile
     * tactical cap: FIRE accompanies forward movement while target is seen. */
    for(unsigned weapon=1u;weapon<=4u;weapon++){
        v=observed(0u,weapon,12u);step(&v,&c);
        assert(c.actions&ACT(AVP_MP_FIRE));
        assert(c.actions&ACT(AVP_MP_FORWARD));
        assert(!(c.actions&ACT(AVP_MP_BACKWARD)));
        /* Backpedal without cancelling FIRE at close ranged distance. */
        v.target_x=v.x+(2u<<16);step(&v,&c);
        assert(c.actions&ACT(AVP_MP_FIRE));
        assert(c.actions&ACT(AVP_MP_BACKWARD));
        assert(!(c.actions&ACT(AVP_MP_FORWARD)));
        /* In the distance band, flank but keep firing. */
        v.target_x=v.x+(4u<<16);step(&v,&c);
        assert(c.actions&ACT(AVP_MP_FIRE));
        assert(c.actions&(ACT(AVP_MP_STRAFE_LEFT)|ACT(AVP_MP_STRAFE_RIGHT)));
        /* No awareness of target => no speculative shots. */
        v.target_visible=0u;step(&v,&c);
        assert(!(c.actions&ACT(AVP_MP_FIRE)));
        /* Visible behind current aim => turn first, no blind fire. */
        v.target_visible=1u;v.heading=0x80000000u;step(&v,&c);
        assert(!(c.actions&ACT(AVP_MP_FIRE)));
    }
    /* Predator ranged weapons are slots 2 and 3, regardless of which is
     * equipped. Original cloak/shield and weapon animations stay intact. */
    for(unsigned weapon=2u;weapon<=3u;weapon++){
        v=observed(2u,weapon,15u);step(&v,&c);
        assert(c.actions&ACT(AVP_MP_FIRE));
        assert(c.actions&ACT(AVP_MP_FORWARD));
        v.target_x=v.x+(2u<<16);step(&v,&c);
        assert(c.actions&ACT(AVP_MP_FIRE));
        assert(c.actions&ACT(AVP_MP_BACKWARD));
        v.invisflag=1u;step(&v,&c);
        assert(!(c.actions&ACT(AVP_MP_FIRE)));
    }
    /* Predator blades/combi and every Alien attack are melee: never fire
     * outside the original hit window, but advance to reach it. */
    for(unsigned weapon=1u;weapon<=4u;weapon+=3u){
        v=observed(2u,weapon,2u);step(&v,&c);
        assert(!(c.actions&ACT(AVP_MP_FIRE)));
        assert(c.actions&ACT(AVP_MP_FORWARD));
        v.target_x=v.x+0x4000u;step(&v,&c);
        assert(c.actions&ACT(AVP_MP_FIRE));
        assert(c.actions&ACT(AVP_MP_BACKWARD));
    }
    v=observed(1u,0u,2u);step(&v,&c);
    assert(!(c.actions&ALIEN_FIRE));
    assert(c.actions&ACT(AVP_MP_FORWARD));
    v.target_x=v.x+0x4000u;step(&v,&c);
    assert(c.actions&ALIEN_FIRE);
    assert(c.actions&ACT(AVP_MP_BACKWARD));
    /* Source corridor-lure preference may never pause Guerilla ranged
     * fire at a visible Alien. Balanced retains that older tactic. */
    {
        uint8_t cells[5u*5u*8u]={0};
        uint8_t flags[256]={0};
        for(unsigned x=0;x<5u;x++)cells[(2u*5u+x)*8u+5u]=1u;
        v=observed(0u,1u,3u);
        v.x=(0u<<16)|0x8000u;
        v.y=(2u<<16)|0x8000u;
        v.target_x=(3u<<16)|0x8000u;
        v.target_y=v.y;
        v.target_is_alien=1u;v.player_energy=v.max_energy=100;
        v.map.cells=cells;v.map.wall_flags=flags;
        v.map.width=v.map.height=5u;
        step(&v,&c);
        assert(c.actions&ACT(AVP_MP_FIRE));
        v.behavior=0u;
        step(&v,&c);
        assert(!(c.actions&ACT(AVP_MP_FIRE)));
    }
    /* A real, empty Marine inventory still vetoes an impossible shot.
     * This is source-weapon availability, not Guerilla tactical delay. */
    v=observed(0u,1u,12u);v.marine_ammo[0]=0u;step(&v,&c);
    assert(!(c.actions&ACT(AVP_MP_FIRE)));
    puts("R84 Guerilla range-independent FIRE / movement / melee: PASS");
    return 0;
}
