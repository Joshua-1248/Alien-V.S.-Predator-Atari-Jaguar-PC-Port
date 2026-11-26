#ifndef AVP_PC_MARINE_START_H
#define AVP_PC_MARINE_START_H

/* Optional PC starting loadout. The Jaguar's MAZESCRN.S initialization runs
 * first. Alter only the original shotgun ownership bit and its first ammo
 * record; do not bypass weapon animation/HUD/equipment initialization. */
#include "player.h"
#include "collectables.h"

static inline int avp_pc_marine_start_shotgun_apply(int enabled,int loading_save,
    s16 species,u8 *owned,s16 *pending,AvpAmmoInfo ammo[6])
{
    if(!enabled||loading_save||species!=PT_HUMAN||!owned||!pending||!ammo)
        return 0;
    *owned=(u8)(*owned | (1u<<1)); /* Jaguar Marine shotgun: weapon #1. */
    ammo[0].cur=ammo[0].max;       /* Original capacity: 44 shells. */
    *pending=1;                  /* Normal MAZESCRN.S weapon draw path. */
    return 1;
}

#endif
