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
                unsigned amp_index, AvpMpActorMask member_mask,
                const AvpMpActorNativeState *const actors[AVP_LOCAL_MP_MAX_ACTORS]);
/* The exact hosted PC simulation-beam cadence, scoped per active world.
 * Never substitute this for the Jaguar's actual beam register in ROM mode. */
uint32_t avp_mp_amp_host_beam_value(uint32_t world_frame);
int avp_mp_amp_step_targets(AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
                            AvpMpActorMask member_mask,
                            AvpMpAmpChooseTarget choose, void *user);
/* Opt-in PC multiplayer per-actor ACIDX cue: original AMP contact, 3 HP
 * damage, and global ALTFRAME remain source-owned. Each newly contacting
 * actor instead receives one private sound event at their own position.
 * No audible cue is sent for non-contacting/immune/dead actors. */
typedef void (*AvpMpAmpPlayerAcidCue)(void *user,unsigned player,
                                      unsigned level,int32_t x,int32_t y);
/* Scope all native enemy animation, attack, pain and death effect calls
 * to the actual AMP's authored coordinates while its mode executes. */
typedef void (*AvpMpAmpEnemySoundScope)(void *user,const AvpAmp *amp,
                                       unsigned level,int entering);
int avp_mp_amp_step_targets_local_acid(
    AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
    AvpMpActorMask member_mask,AvpMpAmpChooseTarget choose,void *user,
    AvpMpActorMask acid_contact_masks[AVP_NUM_AMPS],AvpMpAmpPlayerAcidCue cue);
int avp_mp_amp_step_targets_spatial(
    AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
    AvpMpActorMask member_mask,AvpMpAmpChooseTarget choose,void *user,
    AvpMpActorMask acid_contact_masks[AVP_NUM_AMPS],AvpMpAmpPlayerAcidCue acid_cue,
    AvpMpAmpEnemySoundScope enemy_sound);
/* Host-only damage scope callback: entering=1 before native damage, 0 after. */
typedef void (*AvpMpAmpActorDamage)(void *user,unsigned player,int entering);
int avp_mp_amp_step_targets_with_health(
    AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
    AvpMpActorMask member_mask,AvpMpAmpChooseTarget choose,void *user,
    AvpMpActorMask acid_contact_masks[AVP_NUM_AMPS],AvpMpAmpPlayerAcidCue acid_cue,
    AvpMpAmpEnemySoundScope enemy_sound,AvpMpAmpActorDamage on_damage);
#endif
