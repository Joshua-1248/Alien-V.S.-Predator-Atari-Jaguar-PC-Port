#ifndef AVP_LOCAL_MP_ACTOR_NATIVE_H
#define AVP_LOCAL_MP_ACTOR_NATIVE_H

#include "local_mp_player_native.h"
#include "local_mp_weapon_native.h"
#include "local_mp_hudmsg_native.h"
#include "local_mp_hudview_native.h"

/* A single original-game actor owns BOTH PLAYER.S and MAZESCRN.S script state.
 * These structures overlap in original global weapon selectors, so independent
 * per-module scopes must NOT be nested to run a simulated player frame.
 */
typedef struct AvpMpActorNativeState {
    AvpMpPlayerEngineState player;
    AvpMpWeaponEngineState weapon;
    AvpMpHudMessageState hudmsg;
    AvpMpHudViewState hudview;
    /* DOORS.S acs_level represents the current player's access credential;
     * share the physical door slots per world, not this actor inventory. */
    uint8_t access_level;
} AvpMpActorNativeState;

typedef struct AvpMpActorNativeScope {
    AvpMpActorNativeState original;
    int active;
} AvpMpActorNativeScope;

int avp_mp_actor_scope_active(void);
int avp_mp_actor_native_valid(const AvpMpActorNativeState *in);
int avp_mp_actor_native_capture(AvpMpActorNativeState *out);
int avp_mp_actor_native_restore(const AvpMpActorNativeState *in);
int avp_mp_actor_scope_enter(AvpMpActorNativeScope *scope,
                              const AvpMpActorNativeState *actor);
int avp_mp_actor_scope_leave(AvpMpActorNativeScope *scope,
                              AvpMpActorNativeState *actor);

#endif
