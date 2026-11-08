/* Included only at the very end of original src/game/player.c when the
 * optional local multiplayer extension is compiled. Do not change the
 * original player algorithms or move any original static declarations.
 */
#include "../../new/multiplayer/local_mp_player_native.h"
#include <string.h>

#define AVP_MP_PLAYER_SNAPSHOT_MAGIC 0x504C5952u

int avp_mp_player_native_capture(AvpMpPlayerEngineState *out)
{
    AvpMpPlayerEngineState state;
    if (!out) return 0;
    memset(&state, 0, sizeof(state));
    state.valid=AVP_MP_PLAYER_SNAPSHOT_MAGIC;
#define AVP_MP_CAPTURE(type, name) state.name=name;
    AVP_MP_PLAYER_FIELDS(AVP_MP_CAPTURE)
#undef AVP_MP_CAPTURE
    state.continuous_heading=host_continuous_heading.heading;
    state.continuous_published=host_continuous_heading.published;
    state.continuous_active=host_continuous_heading.active;
    *out=state;
    return 1;
}

int avp_mp_player_native_valid(const AvpMpPlayerEngineState *state)
{
    return state && state->valid==AVP_MP_PLAYER_SNAPSHOT_MAGIC &&
        (state->player_type==PT_HUMAN || state->player_type==PT_ALIEN ||
         state->player_type==PT_PREDATOR) &&
        (state->continuous_active==0 || state->continuous_active==1);
}

int avp_mp_player_native_restore(const AvpMpPlayerEngineState *state)
{
    if (!avp_mp_player_native_valid(state)) return 0;
    /* All validation is complete before modifying a game global. */
#define AVP_MP_RESTORE(type, name) name=state->name;
    AVP_MP_PLAYER_FIELDS(AVP_MP_RESTORE)
#undef AVP_MP_RESTORE
    host_continuous_heading.heading=state->continuous_heading;
    host_continuous_heading.published=state->continuous_published;
    host_continuous_heading.active=state->continuous_active;
    return 1;
}
