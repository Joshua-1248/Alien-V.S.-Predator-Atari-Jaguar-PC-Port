#ifndef AVP_LOCAL_MP_AMP_TARGETS_H
#define AVP_LOCAL_MP_AMP_TARGETS_H
#include "local_mp.h"
#include "local_mp_actor_native.h"
#include "amp.h"
#include <stdint.h>

/* Ordinary hostile AMP modes select exactly ONE live player context using
 * the host's existing gameplay target policy. Dead Alien acid puddles are
 * different: each puddle is evaluated against ALL living members of the
 * current world with the original 30-unit square contact test and 3-energy
 * tick damage, never a single enemy target. Both paths advance the world AMP
 * list once. A world scope must be active with NO actor scope on entry.
 * Non-acid callback -1 means no living target (skip ordinary enemy mode);
 * other invalid target results are errors and must not be replayed.
 */
typedef int (*AvpMpAmpChooseTarget)(void *user, const AvpAmp *creature,
                unsigned amp_index, unsigned member_mask,
                const AvpMpActorNativeState *const actors[AVP_LOCAL_MP_MAX_ACTORS]);
/* The exact hosted PC simulation-beam cadence, scoped per active world.
 * Never substitute this for the Jaguar's actual beam register in ROM mode. */
uint32_t avp_mp_amp_host_beam_value(uint32_t world_frame);
int avp_mp_amp_step_targets(AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
                            unsigned member_mask,
                            AvpMpAmpChooseTarget choose, void *user);
#endif
