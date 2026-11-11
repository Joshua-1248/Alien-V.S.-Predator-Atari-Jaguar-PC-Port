#ifndef AVP_LOCAL_MP_REVEAL_POLICY_H
#define AVP_LOCAL_MP_REVEAL_POLICY_H
#include "local_mp_input.h"
/* F12 multiplayer Show Whole Map is a separately named session-wide setting.
 * Cheats > New > Reveal Entire Map is ALWAYS private to its target player. */
static inline int avp_mp_reveal_for_player(const AvpMpInputConfig *profiles,
                                           unsigned player,int session_show_whole_map)
{
    if(session_show_whole_map)return 1;
    return profiles && player<AVP_MP_INPUT_PLAYERS &&
           !!profiles->player[player].cheat_reveal_map;
}
#endif
