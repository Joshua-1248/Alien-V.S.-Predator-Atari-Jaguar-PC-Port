#ifndef AVP_LOCAL_MP_NATIVE_INPUT_H
#define AVP_LOCAL_MP_NATIVE_INPUT_H
#include "local_mp_actor_native.h"
#include "local_mp_worlds.h"
#include "local_mp_input.h"

/* Translate a configured LOCAL player command to the ORIGINAL active-low
 * Jaguar buttons and the existing PC mouse impulse fields, without changing
 * original player.c or the P1 legacy input flow. One call per actor/tick.
 * Does not poll hardware, dispatch terminal screens or start quick turns.
 * Device-specific routing/polling still belongs to the future host loop. */
int avp_mp_native_input_prepare(AvpMpActorNativeState *actor,
                                const AvpMpPlayerCommand *command,
                                const AvpMpInputProfile *profile);
#endif
