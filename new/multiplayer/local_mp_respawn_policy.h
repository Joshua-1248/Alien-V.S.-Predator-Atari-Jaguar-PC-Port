#ifndef AVP_LOCAL_MP_RESPAWN_POLICY_H
#define AVP_LOCAL_MP_RESPAWN_POLICY_H
#include "local_mp_actor_native.h"
/* Player-requested local co-op respawn rules. They are NEW multiplayer policy,
 * not part of retail single-player. The separate level factory and original
 * InitPlayer/Init1stOvers execution still own the eventual state mutation. */
typedef enum AvpMpRespawnKind {
    AVP_MP_RESPAWN_NONE=0,
    AVP_MP_RESPAWN_COCOON,
    AVP_MP_RESPAWN_SPECIES_START /* Also used for campaign-start option; player keeps own species. */
} AvpMpRespawnKind;
typedef struct AvpMpRespawnPlan {
    AvpMpRespawnKind kind;
    unsigned target_level;
    int32_t new_x,new_y;
    int reset_inventory;
    int reset_honor;
    int start_marine_shotgun;
} AvpMpRespawnPlan;
int avp_mp_respawn_plan(const AvpMpActorNativeState *player,
                        int marine_shotgun_option,unsigned campaign,
                        int campaign_start_enabled,AvpMpRespawnPlan *out);
/* Restore a species' original fully-initialized starting actor only.
 * Does NOT touch shared world/AMP/door state or any other player. */
int avp_mp_respawn_restart(AvpMpActorNativeState *target,
    const AvpMpActorNativeState *original_start,uint32_t spawn_x,
    uint32_t spawn_y,int start_shotgun);
#endif
