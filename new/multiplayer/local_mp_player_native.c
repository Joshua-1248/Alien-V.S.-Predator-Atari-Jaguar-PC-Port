#include "local_mp_player_native.h"

/* Serials scopes only: the native engine is single-global and not thread safe.
 * The player-specific original routines run unchanged while borrowed.
 */
static int player_scope_busy;

int avp_mp_player_scope_enter(AvpMpPlayerScope *scope,
                              const AvpMpPlayerEngineState *player)
{
    if (!scope || !player || player_scope_busy || scope->active) return 0;
    if (!avp_mp_player_native_capture(&scope->original)) return 0;
    if (!avp_mp_player_native_restore(player)) return 0;
    player_scope_busy=1;
    scope->active=1;
    return 1;
}

int avp_mp_player_scope_leave(AvpMpPlayerScope *scope,
                              AvpMpPlayerEngineState *player)
{
    AvpMpPlayerEngineState modified;
    int captured;
    if (!scope || !player || !player_scope_busy || !scope->active) return 0;
    captured=avp_mp_player_native_capture(&modified);
    if (!avp_mp_player_native_restore(&scope->original)) return 0;
    player_scope_busy=0;
    scope->active=0;
    if (captured) *player=modified;
    return captured;
}
