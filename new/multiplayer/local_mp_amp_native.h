#ifndef AVP_LOCAL_MP_AMP_NATIVE_H
#define AVP_LOCAL_MP_AMP_NATIVE_H
#include "amp.h"
#include "levels.h"
#include <stdint.h>

/* Original AMP.S mutable WORLD data. The object allocator, grid and saved
 * creature placements belong to one level/world, not to an individual actor.
 * Captured mode/auxiliary pointers remain in-process: never serialize this
 * structure to disk, move its original code, or tick the engine concurrently.
 * Resource bindings, user callbacks, renderer and sound voices stay global.
 */
typedef struct AvpMpAmpNativeState {
    uint32_t valid;
    AvpAmp actors[AVP_NUM_AMPS];
    u16 collmap[AVP_AMP_GRID_W * AVP_AMP_GRID_H];
    u8 objmap[AVP_AMP_OBJ_ROW_BYTES * AVP_AMP_OBJ_H];
    u16 levels_visit, discflag;
    u8 inventory_table[16];
    u32 ampcount;
    u16 ngens, reset_counter;
    u16 free_list[AVP_NUM_AMPS];
    unsigned free_count;
    u32 slot_generation[AVP_NUM_AMPS];
    u32 next_slot_generation;
    AvpAmpSavedPlacement saved_levels[AVP_MAX_LEVEL][AVP_NUM_AMPS+1];
    u32 marine_pulse_id,marine_flame_id;
    u16 marine_pulse_time,marine_flame_time;
    u32 beam_counter,beam_weapon_reads;
    u16 cur_amp_no, entime;
    s16 explo1,queen_respawns,queen_end_timer;
} AvpMpAmpNativeState;

/* Native AMP state capture/restore, implemented inside original amp.c via
 * one optional include. Capture is only valid between whole AMP updates. */
int avp_mp_amp_native_capture(AvpMpAmpNativeState *out);
int avp_mp_amp_native_restore(const AvpMpAmpNativeState *in);
/* Optional host-only read-only validation callback; NULL uses original rules. */
void avp_mp_amp_native_set_free_list_validator(int (*fn)(const u16 *,unsigned));
/* Opt-in direct destination capture, with prevalidation before writes. */
void avp_mp_amp_native_set_direct_capture(int enabled);

typedef struct AvpMpAmpNativeScope {
    AvpMpAmpNativeState original;
    int active;
} AvpMpAmpNativeScope;
int avp_mp_amp_scope_enter(AvpMpAmpNativeScope *scope,
                             const AvpMpAmpNativeState *world);
int avp_mp_amp_scope_leave(AvpMpAmpNativeScope *scope,
                             AvpMpAmpNativeState *world);
#endif
