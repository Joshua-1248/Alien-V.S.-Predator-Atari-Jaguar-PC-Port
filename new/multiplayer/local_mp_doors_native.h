#ifndef AVP_NEW_LOCAL_MP_DOORS_NATIVE_H
#define AVP_NEW_LOCAL_MP_DOORS_NATIVE_H

#include "doors.h"
#include "levels.h"
#include <stddef.h>
#include <stdint.h>

/* Optional PC-only snapshot of the ORIGINAL DOORS.S C reconstruction.
 * This does not simulate doors: it preserves the original door machine's
 * globals and private active slots between serial world visits.
 * The original maze allocations and metadata must outlive every snapshot.
 * Never call concurrently, re-entrantly, or during a Jaguar game callback.
 * It does not snapshot AMP collision entities, audio, input or other gameplay.
 */
typedef struct AvpMpDoorSlot {
    size_t offset;                /* Relative to this snapshot's maze bytes. */
    uint8_t active, limit;
    int8_t direction;
    int32_t xpos, ypos;
    uint16_t index, grid1, grid2;
} AvpMpDoorSlot;

typedef struct AvpMpDoorEngineState {
    uint32_t valid;
    uint8_t *maze;
    uint16_t width, height;
    int16_t level;
    const AvpDoorLevelMeta *level_meta;
    unsigned level_meta_count;
    const AvpDoorLevelMeta *current_meta;
    AvpDoorPanelIds panels;
    uint8_t panel_list[AVP_MAX_PANELS];
    uint8_t wall_flags[256];
    uint8_t save_arena[AVP_L2L_SIZE];
    int16_t saved_offsets[AVP_MAX_LEVEL];
    uint16_t saved_used;
    uint8_t access_level, lift_key, computer_panel;
    uint16_t computer_offset;
    AvpMpDoorSlot door[2];
    uint8_t double_door;
    int8_t lift_direction;
    size_t lift_offset;
    uint8_t lift_active;
    uint16_t lift_x, lift_y;
    int alien_use_active, alien_use_down, alien_use_previous;
    uint32_t level_effect_id;
} AvpMpDoorEngineState;

/* Capture/restore the ORIGINAL in-process door machine. Failure makes no
 * changes to the target on capture, or to game globals on restore. */
int avp_mp_doors_native_capture(AvpMpDoorEngineState *out);
int avp_mp_doors_native_restore(const AvpMpDoorEngineState *state);

/* Exclude nested scoped calls. save/restore is serial, in the one game
 * process, and does not create windows, workers or network connections. */
typedef struct AvpMpDoorScope {
    AvpMpDoorEngineState original;
    int active;
} AvpMpDoorScope;
int avp_mp_doors_scope_enter(AvpMpDoorScope *scope,
                             const AvpMpDoorEngineState *world);
int avp_mp_doors_scope_leave(AvpMpDoorScope *scope,
                             AvpMpDoorEngineState *world);

/* Create initial original Jaguar door-machine state for one real decoded
 * multiplayer maze. Original single-player globals are restored afterward.
 * Levels should be 1..15, with the map's eight-byte header already skipped.
 * Initialize each world once; subsequent scoped calls retain its door state.
 */
/* Read the original delayed-lift passenger cell without guessing which
 * actor used the lift. Available only in the bound original world scope. */
int avp_mp_doors_native_pending_lift(uint16_t *x,uint16_t *y);

int avp_mp_doors_world_init(AvpMpDoorEngineState *world, uint8_t *maze,
                             uint16_t width, uint16_t height, unsigned level);
#endif
