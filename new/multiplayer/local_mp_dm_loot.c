#include "local_mp_dm_loot.h"

void avp_mp_dm_predator_weapons(uint8_t *player_owned,uint8_t *weapon_owned)
{
    const uint8_t combat=(uint8_t)((1u<<1)|(1u<<2)|(1u<<3)|(1u<<4));
    if(player_owned)*player_owned=(uint8_t)(*player_owned|combat);
    if(weapon_owned)*weapon_owned=(uint8_t)(*weapon_owned|combat);
}

int avp_mp_dm_tracker_bonus(uint16_t tracker_roll,int tracker_owned)
{
    /* All 16 low-nibble values occur equally often across 16-bit rolls. */
    return !tracker_owned && (tracker_roll & 15u)==0u;
}

int avp_mp_dm_corpse_loot(uint8_t owned,const AvpAmmoInfo ammo[4],
    uint16_t weapon_roll,uint16_t ammo_roll,AvpMpDmLoot *out)
{
    static const s16 weapon_type[4]={COLL_SHOTGUN,COLL_PULSERIFLE,
                                      COLL_FLAMETHROWER,COLL_SMARTGUN};
    static const s16 ammo_type[4]={COLL_SHOTAMMO,COLL_PULSEAMMO,
                                    COLL_FLAMEAMMO,COLL_SMARTAMMO};
    static const s16 quantities[4]={15,60,25,20};
    unsigned eligible[4],count=0;
    unsigned next=0u,denominator=0u;
    if(!ammo||!out)return 0;
    out->type=COLL_NONE;out->amount=0;
    if(!(owned&(1u<<2))){next=2u;denominator=2u;}
    else if(!(owned&(1u<<3))){next=3u;denominator=4u;}
    else if(!(owned&(1u<<4))){next=4u;denominator=8u;}
    if(denominator && (weapon_roll&(uint16_t)(denominator-1u))==0u){
        out->type=weapon_type[next-1u];out->amount=quantities[next-1u];
        return 1;
    }
    for(unsigned i=0;i<4u;i++)
        if((owned&(1u<<(i+1u))) && ammo[i].cur<ammo[i].max)
            eligible[count++]=i;
    if(!count)return 0;
    {const unsigned i=eligible[(unsigned)ammo_roll%count];
     out->type=ammo_type[i];out->amount=quantities[i];}
    return 1;
}
