#ifndef AVP_MAP_MAKER_PLAYTEST_H
#define AVP_MAP_MAKER_PLAYTEST_H
#include "map_maker.h"
#include "doors.h"
/* Isolated child-process copy of original LEVDOOR.S runtime tables.
 * All pointers refer to storage inside this struct. Never save or transfer
 * these pointers, and never bind this in the editor or retail campaign. */
typedef struct AvpMmPlaytestDoorMeta {
    AvpDoorLevelMeta levels[AVP_MM_LEVELS];
    AvpDoorAccessEntry access[AVP_MM_MAX_SCRIPTS];
    AvpDoorExitEntry exits[AVP_MM_MAX_SCRIPTS];
    AvpDoorInterlockEntry interlocks[AVP_MM_MAX_SCRIPTS];
    u16 required[AVP_MM_MAX_SCRIPTS];
} AvpMmPlaytestDoorMeta;
/* Extra Play Test preflight: verifies that every relocated script reference
 * still points to the expected edited panel or canonical door-state owner.
 * Unmoved retail script rows are never reinterpreted. */
int avp_mm_playtest_validate_relocated_geometry(const AvpMapMaker *m);
/* Build an independent copy of every retail level, substituting only the
 * selected draft's validated offset-only relocation. On failure, no live
 * game table is touched. Exported separately for deterministic testing. */
int avp_mm_playtest_build_door_meta(const AvpMapMaker *m, AvpMmPlaytestDoorMeta *out);
/* Call ONLY in the child game process after retail metadata initialization. */
int avp_mm_playtest_bind_scripts(const AvpMapMaker *m);
/* The retail SaveDoors/ResetDoors routine saves state only for a cell's
 * LEFT/TOP panel. An edited RIGHT/BOTTOM-only door uses the next cell's
 * canonical state byte and would otherwise lose its open state on travel.
 * Preserve only those missing-owner states in the isolated PC child. */
unsigned avp_mm_playtest_capture_noncanonical_door_states(
    const AvpMapMaker *m,unsigned level,const u8 *maze,size_t bytes);
unsigned avp_mm_playtest_restore_noncanonical_door_states(
    const AvpMapMaker *m,unsigned level,u8 *maze,size_t bytes);
/* Test-process only: append independently authored AMP positions to the
 * read-only retail placement lists, without changing src/game/amp.c. */
int avp_mm_playtest_bind_amp(const AvpMapMaker *m);
/* Apply the draft BEFORE the retail campaign's first collision/door bind.
 * Also used at transitions back to the tested level. */
int avp_mm_playtest_overlay_maze(const AvpMapMaker *m,unsigned level,
                                  u8 *decoded,size_t decoded_size);
#endif
