#ifndef AVP_LOCAL_MP_DM_LOOT_H
#define AVP_LOCAL_MP_DM_LOOT_H
/* PC-local Deathmatch corpse rewards. Do not modify the Jaguar's authored
 * single-player/co-op corpse loot tables or its ammunition/weapon scripts. */
#include "collectables.h"
#include <stdint.h>

typedef struct AvpMpDmLoot {
    s16 type,amount; /* Existing CollectIt() item and source ammo quantity. */
} AvpMpDmLoot;

/* Predator combat weapons 1..4, retaining cloak/medkit ownership exactly as
 * authored. Both redundant per-actor availability copies must be updated. */
void avp_mp_dm_predator_weapons(uint8_t *player_owned,uint8_t *weapon_owned);

/* Separate 1-in-16 Marine motion-tracker bonus for an untracked Marine.
 * Uses its own independent 16-bit random draw: existing ordered weapon/ammo
 * chances are not reduced. Only the native Marine-player corpse claim path
 * calls this; a tracker already owned never produces a duplicate pickup. */
int avp_mp_dm_tracker_bonus(uint16_t tracker_roll,int tracker_owned);

/* Exactly 1/2 for the first missing Pulse Rifle, 1/4 for Flamethrower,
 * 1/8 for Smart Gun. Never skip an earlier gun; no roll once all are owned.
 * A non-winning roll picks uniform ammo from OWNED and NOT-FULL weapons.
 * Returns zero when neither an ammo refill nor a new weapon is possible. */
int avp_mp_dm_corpse_loot(uint8_t owned,const AvpAmmoInfo ammo[4],
    uint16_t weapon_roll,uint16_t ammo_roll,AvpMpDmLoot *out);
#endif
