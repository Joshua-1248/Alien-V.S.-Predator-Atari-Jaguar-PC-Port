#ifndef AVP_LOCAL_MP_NATIVE_CONTEXT_H
#define AVP_LOCAL_MP_NATIVE_CONTEXT_H

#include "local_mp.h"
#include "local_mp_actor_native.h"
#include "local_mp_amp_native.h"
#include "local_mp_doors_native.h"
#include "local_mp_retail_maps.h"

/* Per-level original-engine state. The map remains owned by the retail-map
 * store, and must stay leased for the full lifetime of every world visit.
 * AMP state is shared by all participants in this level; actor state is not.
 * This is an in-process *serialized binding*, NOT a second game engine.
 * No display, HUD, original assets, sound or game loop are owned here.
 */
typedef struct AvpMpNativeWorld {
    AvpMpAmpNativeState amp;
    AvpMpDoorEngineState doors;
    AvpMpRetailMap *map; /* borrowed, never freed here */
    AvpMpHudWorldState hud_world;
    uint32_t pad_frame_counter; /* original readpad clock, ticked once/world */
} AvpMpNativeWorld;

typedef struct AvpMpNativeScope {
    AvpMpAmpNativeScope amp;
    AvpMpDoorScope doors;
    AvpMpActorNativeScope actor;
    uint8_t *old_collision;
    uint16_t old_collision_width, old_collision_height;
    uint16_t old_maze_width, old_maze_height;
    uint32_t old_pad_frame_counter;
    AvpMpHudWorldState old_hud_world;
    const AvpMpNativeWorld *bound_world;
    int active, actor_active;
} AvpMpNativeScope;

/* The original game's world initializer/retail loader must provide both
 * snapshots. The map must match the captured original door-level binding.
 * In particular, NEVER infer creature templates or level metadata here. */
int avp_mp_native_world_scope_active(void);
int avp_mp_native_world_valid(const AvpMpNativeWorld *world);

/* Capture a CURRENTLY LOADED ORIGINAL LEVEL as the seed for a co-op world.
 * The caller supplies a retained, private, mutable copy of that level's
 * decoded retail maze (including its eight-byte header). This operation
 * captures the actual original AMP/DOORS/HUD state, including the original
 * active door/access tables, rather than constructing incomplete metadata.
 * The host must already have entered the same level through the original
 * campaign loader. A successful capture copies the current mutable cell
 * bytes into the destination map; on failure neither output is changed.
 * The host engine globals are never rebound or modified here.
 */
int avp_mp_native_capture_active_world(AvpMpNativeWorld *out,
                                       AvpMpRetailMap *retained_map);

/* Bind a world, then bind/advance one actor at a time, then advance this
 * world's enemies and doors ONCE. Every leave restores the host legacy state.
 * Nested world or actor visits are forbidden on the gameplay thread.
 * actor_step must NOT call readpad(); the input sample was prepared by the
 * session, and run_world advances the original frame counter once per world. */
/* A scope must be zero-initialized before first use. */
int avp_mp_native_world_enter(AvpMpNativeScope *scope, const AvpMpNativeWorld *world);
int avp_mp_native_actor_enter(AvpMpNativeScope *scope, const AvpMpActorNativeState *actor);
int avp_mp_native_actor_leave(AvpMpNativeScope *scope, AvpMpActorNativeState *actor);
int avp_mp_native_world_leave(AvpMpNativeScope *scope, AvpMpNativeWorld *world);

/* Callback steps MUST call the original Jaguar-derived update routines and
 * must not reenter this bridge. A failed callback still saves its mutations
 * and restores the host; its owning session MUST fault, not replay that tick.
 * This layer deliberately does not decide which Jaguar routines are safe to
 * invoke until HUD/audio/renderer/resource and cross-player targets are bound.
 */
typedef int (*AvpMpNativeActorStep)(void *user, unsigned player);
typedef int (*AvpMpNativeWorldStep)(void *user);
int avp_mp_native_run_world(AvpMpNativeWorld *world,
                            AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS], unsigned member_mask,
                            AvpMpNativeActorStep actor_step,
                            AvpMpNativeWorldStep world_step, void *user);
#endif
