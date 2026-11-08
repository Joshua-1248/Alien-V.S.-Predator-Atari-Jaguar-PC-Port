#ifndef AVP_LOCAL_MP_NATIVE_BACKEND_H
#define AVP_LOCAL_MP_NATIVE_BACKEND_H
#include "local_mp_native_context.h"
#include "local_mp_native_input.h"
#include "local_mp_amp_targets.h"
#include "local_mp_worlds.h"

/* Real original-engine binding for the existing generic session coordinator.
 * Factories must prepare state from original initializers and verified retail
 * resources; caller retains ROM and map storage until destroy_world returns.
 * User callbacks may ONLY call original game operations (or provide renderer
 * frames); this adapter does not invent levels, physics, sprites or AI. */
typedef struct AvpMpNativeBackendHost {
    void *user;
    const AvpMpInputConfig *profiles;
    int (*create_actor)(void *user,unsigned player,unsigned species,
                        AvpMpActorNativeState *out);
    int (*create_level)(void *user,unsigned campaign,unsigned level,
                        AvpMpNativeWorld *out);
    void (*destroy_level)(void *user,AvpMpNativeWorld *level);
    /* Join must be failure-atomic: no mutation of actor/world on return 0. */
    int (*join_level)(void *user,AvpMpNativeWorld *world,unsigned player,
                      AvpMpActorNativeState *actor);
    void (*leave_level)(void *user,AvpMpNativeWorld *world,unsigned player,
                        AvpMpActorNativeState *actor);
    /* Optional source-scoped batch validator. Must leave world unchanged on
     * failure; all actor writes remain in private staged copies. */
    int (*join_group)(void *user,AvpMpNativeWorld *world,unsigned mask,
                      AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS]);
    /* Player 1 retains the original PC keyboard/gamepad/pad conversion. It is
     * deliberately NOT overwritten using the secondary-player profile.
     * Caller must supply its current legacy input as original-state fields. */
    int (*prepare_player1)(void *user, AvpMpActorNativeState *actor,
                           const AvpMpPlayerCommand *command);
    AvpMpNativeActorStep advance_actor;
    /* Called once per bound world after player steps and BEFORE original AMP
     * updates. A player in a death fade skips advance_actor, but still needs
     * fresh enemy/door render snapshots while their camera stays stationary.
     * The original game world and actor state must not be changed here. */
    int (*snapshot_world_presentation)(void *user,unsigned member_mask);
    /* Optional original one-tick AMP dispatcher, with an explicit gameplay
     * target policy for each original mode. When set, advance_level must NOT
     * call UpdateAMPs a second time; it owns other world work only. */
    AvpMpAmpChooseTarget choose_amp_target;
    AvpMpNativeWorldStep advance_level;
    /* Optional original post-world weapon/HUD script pass. Original NextFrame
     * updates players, then AMP/world, then calls PreFrame/UpdtScrOverlays.
     * The session binds each original actor separately after world work. */
    AvpMpNativeActorStep advance_actor_post_world;
    /* Must render full original-HUD independent frames into storage retained
     * until the current avp_mp_session_compose() returns. The callback sees
     * the original world and actor state bound, in the same PC process. */
    int (*draw_actor)(void *user,AvpMpNativeWorld *world,unsigned player,
                      AvpMpActorNativeState *actor,AvpLocalMpSourceFrame *out);
} AvpMpNativeBackendHost;

int avp_mp_native_backend_valid(const AvpMpNativeBackendHost *host);
AvpMpWorldBackend avp_mp_native_backend_ops(void);
#endif
