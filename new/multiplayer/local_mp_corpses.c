#include "local_mp_corpses.h"
#include "local_mp.h"
#include "amp.h"
#include "avp_sfx_ids.h"
#include <stddef.h>
unsigned avp_mp_death_sfx(unsigned species)
{
    /* MAZE.S::Process_EndGame death event, exact source mapping. */
    switch(species){
    case 0u:return AVP_FX_DEATH;
    case 1u:return AVP_FX_AHIT;
    case 2u:return AVP_FX_PD2;
    default:return 0u;
    }
}
int avp_mp_corpse_add(AvpMpCorpses *store,unsigned level,unsigned species,
                      uint32_t x,uint32_t y,uint16_t angle,uint64_t tick)
{
    unsigned slot=AVP_MP_CORPSE_LIMIT;
    uint64_t oldest=UINT64_MAX;
    unsigned oldest_slot=0u;
    if(!store || level<1u||level>15u||species>2u)return 0;
    /* Never touch original AMP allocator/free list or per-level collmap. */
    for(unsigned i=0;i<AVP_MP_CORPSE_LIMIT;i++){
        if(!store->records[i].occupied){slot=i;break;}
        if(store->records[i].death_tick<oldest){
            oldest=store->records[i].death_tick;oldest_slot=i;
        }
    }
    if(slot==AVP_MP_CORPSE_LIMIT){
        /* Keep a fixed memory budget for indefinite co-op sessions. */
        slot=oldest_slot;++store->evictions;
    }
    store->records[slot]=(AvpMpCorpse){
        .x=x,.y=y,.angle=angle,.level=(uint16_t)level,
        .creature=(uint16_t)(species==0u?AC_HUMAN:
                     species==1u?AC_ALIEN:AC_PREDATOR),
        .death_tick=tick,.occupied=1u
    };
    return 1;
}
int avp_mp_corpse_claim_at(AvpMpCorpses *store,unsigned level,
                           unsigned player,uint32_t x,uint32_t y)
{
    if(!store||level<1u||level>15u||player>=AVP_LOCAL_MP_MAX_ACTORS)return 0;
    for(unsigned i=0u;i<AVP_MP_CORPSE_LIMIT;i++){
        AvpMpCorpse *c=&store->records[i];
        const uint16_t bit=(uint16_t)(1u<<player);
        /* Only a fallen Marine PLAYER carries scavengable weapons/ammo.
         * Alien/Predator corpses remain visible but never yield Marine gear. */
        if(!c->occupied ||c->creature!=AC_HUMAN ||c->level!=level ||
           (c->looted_mask&bit) ||
           (c->x>>16)!=(x>>16)||(c->y>>16)!=(y>>16))continue;
        c->looted_mask|=bit;
        return 1;
    }
    return 0;
}
unsigned avp_mp_corpses_for_level(const AvpMpCorpses *store,unsigned level,
                uint64_t tick,AvpGpuOtherPlayer *out,unsigned capacity)
{
    unsigned count=0;
    if(!store||!out||level<1u||level>15u)return 0;
    for(unsigned i=0;i<AVP_MP_CORPSE_LIMIT && count<capacity;i++){
        const AvpMpCorpse *c=&store->records[i];
        unsigned age,step,last;
        if(!c->occupied || c->level!=level)continue;
        /* AMP.S death sequences: Marine 0,1,2; Alien 0..3;
         * Predator 0..4 with alternate-frame advancement. The frame
         * remains at the original death sheet's final pose thereafter. */
        age=(unsigned)(tick>=c->death_tick?tick-c->death_tick:0u);
        step=c->creature==AC_ALIEN?age:age/2u;
        last=c->creature==AC_HUMAN?2u:c->creature==AC_ALIEN?3u:4u;
        if(step>last)step=last;
        out[count++]=(AvpGpuOtherPlayer){
            .x=(int32_t)c->x,.y=(int32_t)c->y,.angle=c->angle,
            .creature=c->creature,.animation=AS_DEATH,.frame=(uint16_t)step
        };
    }
    return count;
}
