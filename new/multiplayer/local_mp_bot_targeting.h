#ifndef AVP_LOCAL_MP_BOT_TARGETING_H
#define AVP_LOCAL_MP_BOT_TARGETING_H
#include <stdint.h>
/* Live bot target selection: only native sight or an actually-owned Marine
 * motion-tracker contact can interrupt searching/scavenging. A live optical
 * contact wins over a radar-only one even if the radar contact is closer.
 * Bot fire still requires direct optical contact and original weapon reach.
 * Distances are squared original 16.16 Jaguar map coordinates. */
static inline int avp_mp_bot_immediate_contact_prefer(unsigned species,
    unsigned sight,unsigned tracker,uint64_t distance_sq,
    unsigned selected,unsigned selected_sight,uint64_t selected_distance_sq)
{
    /* Marines and Predators can fight at normal ranged gun distance. Alien
     * melee fighters must physically approach; they can start approaching
     * an observed target just as early, without firing through a wall. */
    const uint64_t range=((uint64_t)6u<<16);
    (void)species;
    if(!(sight||tracker)||distance_sq>=range*range)return 0;
    if(!selected)return 1;
    if((!!sight)!=(!!selected_sight))return !!sight;
    return distance_sq<selected_distance_sq;
}
/* Deathmatch-only arbitration. This preserves native AMP hostiles as valid
 * opponents: a PvP opponent can replace them only when acquired and the
 * opponent is currently visible over a stale AMP, or is closer at equal
 * visibility. Do not discard original enemies when there are no players. */
static inline int avp_mp_bot_dm_prefer_player(unsigned existing_hostile,
    unsigned existing_visible,uint64_t existing_dist,
    unsigned candidate_visible,uint64_t candidate_dist,
    unsigned has_other_goal)
{
    if(existing_hostile){
        if(existing_visible && !candidate_visible)return 0;
        if((!!existing_visible)==(!!candidate_visible) &&
           candidate_dist>=existing_dist)return 0;
        return 1;
    }
    if(has_other_goal && !candidate_visible &&
       candidate_dist>((uint64_t)5u<<16)*((uint64_t)5u<<16))return 0;
    return 1;
}
#endif
