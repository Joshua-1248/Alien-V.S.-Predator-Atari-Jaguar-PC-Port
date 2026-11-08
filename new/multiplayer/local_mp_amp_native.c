#include "local_mp_amp_native.h"

/* Serialize original global engine access on the main thread. This is a
 * state-switch boundary, NOT a separate thread, window or game process. */
static int amp_scope_busy;
int avp_mp_amp_scope_enter(AvpMpAmpNativeScope *scope,
                             const AvpMpAmpNativeState *world)
{
    if (!scope || !world || scope->active || amp_scope_busy) return 0;
    if (!avp_mp_amp_native_capture(&scope->original)) return 0;
    if (!avp_mp_amp_native_restore(world)) return 0;
    scope->active=1;
    amp_scope_busy=1;
    return 1;
}
int avp_mp_amp_scope_leave(AvpMpAmpNativeScope *scope,
                             AvpMpAmpNativeState *world)
{
    AvpMpAmpNativeState modified;
    int captured, restored;
    if (!scope || !world || !scope->active || !amp_scope_busy) return 0;
    captured=avp_mp_amp_native_capture(&modified);
    restored=avp_mp_amp_native_restore(&scope->original);
    if (!restored) return 0; /* Fail closed; don't claim engine was restored. */
    amp_scope_busy=0;
    scope->active=0;
    if (captured) *world=modified;
    return captured;
}
