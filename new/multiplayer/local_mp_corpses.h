#ifndef AVP_LOCAL_MP_CORPSES_H
#define AVP_LOCAL_MP_CORPSES_H
/* Co-op player corpses are visual, non-colliding original AAF actors.
 * The commercial game has no multiplayer player corpses; use the ORIGINAL
 * enemy human/alien/predator death sheets and frame order. Unlike an AMP
 * NPC, a player corpse cannot consume an enemy slot, damage its killer,
 * award score or erase the level collision grid. */
#include "jaguar/avp_gpu/state.h"
#include <stdint.h>
#define AVP_MP_CORPSE_LIMIT 128u

typedef struct AvpMpCorpse {
    uint32_t x,y;
    uint16_t angle, creature, level;
    uint64_t death_tick;
    uint8_t occupied;
    uint16_t looted_mask;
} AvpMpCorpse;
typedef struct AvpMpCorpses {
    AvpMpCorpse records[AVP_MP_CORPSE_LIMIT];
    uint64_t evictions;
} AvpMpCorpses;

/* Input species uses PT_... / 4 (0=Marine, 1=Alien, 2=Predator). */
unsigned avp_mp_death_sfx(unsigned species);
int avp_mp_corpse_add(AvpMpCorpses *store,unsigned level,unsigned species,
                      uint32_t x,uint32_t y,uint16_t angle,uint64_t tick);
/* One fixed corpse may be scavenged once per actor, across respawns. Same
 * original tile-cell proximity as the Jaguar's Collectables() test. */
int avp_mp_corpse_claim_at(AvpMpCorpses *store,unsigned level,
                           unsigned player,uint32_t x,uint32_t y);
unsigned avp_mp_corpses_for_level(const AvpMpCorpses *store,unsigned level,
                uint64_t tick,AvpGpuOtherPlayer *out,unsigned capacity);
#endif
