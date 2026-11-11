#ifndef AVP_LOCAL_MP_CHEAT_TARGETS_H
#define AVP_LOCAL_MP_CHEAT_TARGETS_H

#include "local_mp_input.h"

typedef struct AvpMpTargetCheatFlags {
    int no_enemy_attack;
    int no_facehug;
} AvpMpTargetCheatFlags;

/* Host-only co-op targeting policy: an AMP checks the restrictions of its
 * ACTUAL chosen player, not the world owner or the previous target. Bots have
 * no player cheat profile. Never OR flags across participants. */
static inline AvpMpTargetCheatFlags avp_mp_target_cheats(
    const AvpMpInputConfig *profiles,unsigned target,
    int p1_no_enemy_attack,int p1_no_facehug)
{
    AvpMpTargetCheatFlags f={0,0};
    if(target==0u){
        f.no_enemy_attack=!!p1_no_enemy_attack;
        f.no_facehug=!!p1_no_facehug;
    }else if(profiles && target<AVP_MP_INPUT_PLAYERS){
        f.no_enemy_attack=!!profiles->player[target].cheat_no_enemy_attack;
        f.no_facehug=!!profiles->player[target].cheat_no_facehug;
    }
    return f;
}
#endif
