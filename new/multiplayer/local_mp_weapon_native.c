#include "local_mp_weapon_native.h"

/* No nested or concurrent use of original single-global state. */
static int weapon_scope_busy;

int avp_mp_weapon_scope_enter(AvpMpWeaponScope *scope,
                               const AvpMpWeaponEngineState *weapon)
{
    if (!scope || !weapon || scope->active || weapon_scope_busy) return 0;
    if (!avp_mp_weapon_native_capture(&scope->original)) return 0;
    if (!avp_mp_weapon_native_restore(weapon)) return 0;
    scope->active=1;
    weapon_scope_busy=1;
    return 1;
}

int avp_mp_weapon_scope_leave(AvpMpWeaponScope *scope,
                               AvpMpWeaponEngineState *weapon)
{
    AvpMpWeaponEngineState modified;
    int captured;
    if (!scope || !weapon || !scope->active || !weapon_scope_busy) return 0;
    captured=avp_mp_weapon_native_capture(&modified);
    if (!avp_mp_weapon_native_restore(&scope->original)) return 0;
    scope->active=0;
    weapon_scope_busy=0;
    if (captured) *weapon=modified;
    return captured;
}
