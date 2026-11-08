#include "local_mp_actor_native.h"
#include "doors.h"
#include <string.h>

static int actor_scope_busy;

int avp_mp_actor_scope_active(void) { return actor_scope_busy!=0; }

/* Four globals were captured by earlier player-native snapshots and are now
 * also in MAZESCRN.S weapon snapshots. Keep them coherent as one transaction.
 */
static int aligned(const AvpMpActorNativeState *s)
{
    return s && s->player.cur_weps==s->weapon.cur_weps &&
        s->player.old_weps==s->weapon.old_weps &&
        s->player.cur_wepno==s->weapon.cur_wepno &&
        s->player.new_wepno==s->weapon.new_wepno;
}

int avp_mp_actor_native_valid(const AvpMpActorNativeState *state)
{
    return aligned(state) && state->access_level<=99u /* Deathmatch Marine clearance 99; no co-op change */ &&
        avp_mp_player_native_valid(&state->player) &&
        avp_mp_weapon_native_valid(&state->weapon) &&
        avp_mp_hudmsg_native_valid(&state->hudmsg) &&
        avp_mp_hudview_native_valid(&state->hudview);
}

int avp_mp_actor_native_capture(AvpMpActorNativeState *out)
{
    AvpMpActorNativeState state;
    memset(&state,0,sizeof(state)); /* stable padding for exact actor snapshots */
    if (!out || !avp_mp_player_native_capture(&state.player) ||
        !avp_mp_weapon_native_capture(&state.weapon) ||
        !avp_mp_hudmsg_native_capture(&state.hudmsg) ||
        !avp_mp_hudview_native_capture(&state.hudview) || !aligned(&state)) return 0;
    state.access_level=acs_level;
    *out=state;
    return 1;
}

int avp_mp_actor_native_restore(const AvpMpActorNativeState *in)
{
    /* Validate BOTH halves before mutating ANY original global.  This also
     * allows restoring a good host snapshot if a script left its current
     * machine state invalid, without trying to capture that invalid state. */
    if (!avp_mp_actor_native_valid(in)) return 0;
    if (!avp_mp_player_native_restore(&in->player)) return 0;
    if(!avp_mp_weapon_native_restore(&in->weapon) ||
       !avp_mp_hudmsg_native_restore(&in->hudmsg) ||
       !avp_mp_hudview_native_restore(&in->hudview))return 0;
    acs_level=in->access_level;
    return 1;
}

int avp_mp_actor_scope_enter(AvpMpActorNativeScope *scope,
                              const AvpMpActorNativeState *actor)
{
    if (!scope || !actor || scope->active || actor_scope_busy ||
        !avp_mp_actor_native_capture(&scope->original)) return 0;
    if (!avp_mp_actor_native_restore(actor)) return 0;
    scope->active=1;
    actor_scope_busy=1;
    return 1;
}

int avp_mp_actor_scope_leave(AvpMpActorNativeScope *scope,
                              AvpMpActorNativeState *actor)
{
    AvpMpActorNativeState changed;
    int captured, restored;
    if (!scope || !actor || !scope->active || !actor_scope_busy) return 0;
    captured=avp_mp_actor_native_capture(&changed);
    restored=avp_mp_actor_native_restore(&scope->original);
    if (!restored) return 0;
    if (captured) *actor=changed;
    scope->active=0;
    actor_scope_busy=0;
    return captured;
}
