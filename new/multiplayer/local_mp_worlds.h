#ifndef AVP_NEW_LOCAL_MP_WORLDS_H
#define AVP_NEW_LOCAL_MP_WORLDS_H

#include "local_mp.h"
#include "local_mp_compositor.h"
#include <stdint.h>

/* Optional PC-only multiparty session. One independently owned world context
 * per active campaign+level; one player context per participant, carried across
 * level transitions. No original Jaguar globals, save-state swaps or network.
 *
 * Backend CONTRACT:
 * - Each create returns a fresh, non-null independently mutable context.
 * - enter_world is failure-atomic: on failure it must NOT attach the actor.
 * - render_player returns distinct complete ORIGINAL-HUD frames, valid until
 *   compose_frames returns (not a shared/reused framebuffer).
 * - tick_world is non-transactional: a failed tick faults the whole session;
 *   never silently replay a partially advanced multiplayer frame.
 * - The backend, not this coordinator, owns AI/physics/doors, mixed-species
 *   gameplay, source assets, player actor simulation, audio and level saves.
 *   No original gameplay backend has been connected at Sweep 53.
 */

typedef struct AvpMpPlayerCommand {
    uint32_t actions; /* Future original-pad mapping; no implicit Jaguar mapping. */
    int32_t mouse_dx, mouse_dy;
    /* PC-only proportional stick samples. Legacy Jaguar-pad-only users leave
     * these zero. Keep KEYBOARD actions separate from axis-emulated buttons
     * so enabling analog motion does not accelerate movement twice. */
    uint32_t digital_actions;
    uint32_t analog_axis_mask; /* bit 0: move, 1: strafe, 2: turn */
    float analog_forward, analog_strafe, analog_turn;
    int analog_enabled, enhanced_analog_enabled;
    float analog_turn_sensitivity, analog_turn_speed;
} AvpMpPlayerCommand;

typedef struct AvpMpWorldBackend {
    int (*create_player)(void *user, unsigned player, unsigned species, void **out);
    void (*destroy_player)(void *user, void *player_ctx);
    int (*create_world)(void *user, unsigned campaign, unsigned level, void **out);
    void (*destroy_world)(void *user, void *world_ctx);
    int (*enter_world)(void *user, void *world_ctx, unsigned player, void *player_ctx);
    void (*exit_world)(void *user, void *world_ctx, unsigned player, void *player_ctx);
    int (*tick_world)(void *user, void *world_ctx, unsigned member_mask,
                      void *const player_ctx[AVP_LOCAL_MP_MAX_ACTORS],
                      const AvpMpPlayerCommand commands[AVP_LOCAL_MP_MAX_ACTORS],
                      uint64_t tick);
    int (*render_player)(void *user, void *world_ctx, unsigned player,
                         void *player_ctx, AvpLocalMpSourceFrame *out);
    /* Optional failure-atomic batch arrival: validate/prepare ALL actors first,
     * then commit every actor together or leave every actor untouched. */
    int (*enter_world_group)(void *user,void *world_ctx,unsigned mask,
                             void *const player_ctx[AVP_LOCAL_MP_MAX_ACTORS]);
} AvpMpWorldBackend;

typedef struct AvpMpActiveWorld {
    unsigned campaign, level, member_mask;
    void *context;
    uint64_t ticks;
} AvpMpActiveWorld;

typedef struct AvpMpSession {
    AvpLocalMpSetup setup;
    AvpMpWorldBackend backend;
    void *user;
    void *player[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpActiveWorld worlds[AVP_LOCAL_MP_MAX_ACTORS];
    uint64_t tick;
    int running, faulted;
} AvpMpSession;

/* Initialize AvpMpSession to all zero bytes before its first start().
 * start() is all-or-nothing. The setup must be explicitly enabled. */
int avp_mp_session_start(AvpMpSession *s, const AvpLocalMpSetup *setup,
                         const AvpMpWorldBackend *ops, void *user);
void avp_mp_session_stop(AvpMpSession *s);
/* One tick per ACTIVE WORLD, not one tick per player. An error faults the
 * session; stop() is still safe. The host is responsible for 10-Hz pacing. */
int avp_mp_session_tick(AvpMpSession *s,
                        const AvpMpPlayerCommand commands[AVP_LOCAL_MP_MAX_ACTORS]);
/* Transactional transfer for one player. The old shared world is retained
 * while other members remain. No temporary second simulation is executed. */
int avp_mp_session_move(AvpMpSession *s, unsigned player, unsigned new_level);
/* Multi-passenger lift/airlock transfer, all-or-nothing. Requires every player
 * in mask to share one source world; one destination join transaction. */
int avp_mp_session_move_group(AvpMpSession *s,unsigned mask,unsigned new_level);
/* Render one separate player frame each, then reuse the Sweep 52 compositor.
 * On any renderer failure, the destination is NOT modified. */
unsigned avp_mp_session_compose(AvpMpSession *s, const AvpLocalMpOutputFrame *output,
                                uint32_t clear_color);
unsigned avp_mp_session_world_count(const AvpMpSession *s);
const AvpMpActiveWorld *avp_mp_session_player_world(const AvpMpSession *s,
                                                   unsigned player);

#endif
