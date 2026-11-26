#ifndef AVP_LOCAL_MP_CHAMBER_H
#define AVP_LOCAL_MP_CHAMBER_H
#include "local_mp_retail_maps.h"
#include <stdint.h>

/* Co-op only: an arriving pair may temporarily share a legal floor position.
 * Pair exemptions are set only for spawn/transition arrivals and retire once
 * there is room for both players. The current co-op core does not implement
 * physical player-vs-player blocking; this state is for that future option. */
typedef struct AvpMpSharedArrivalGrace {
    uint8_t ignore_player_collision[4];
} AvpMpSharedArrivalGrace;
void avp_mp_shared_arrival_update(AvpMpSharedArrivalGrace *state,
                                  const uint8_t present[4],
                                  const unsigned world_level[4],
                                  const uint32_t x[4],const uint32_t y[4],
                                  unsigned arriving_mask);
int avp_mp_shared_arrival_exempt(const AvpMpSharedArrivalGrace *state,
                                 unsigned p,unsigned q);
/* Identify occupants of the SAME closed, authored elevator/airlock chamber.
 * Door openings are always treated as room boundaries, regardless of their
 * current animation. Never use point distance alone to pick up a bystander.
 * Returns 1 only if both points belong to a single bounded small chamber;
 * returns 0 on invalid map, unbounded corridor, ambiguous open plan, or error.
 */
int avp_mp_chamber_contains(const AvpMpRetailMap *map,
                            uint32_t leader_x,uint32_t leader_y,
                            uint32_t other_x,uint32_t other_y);
/* Preserve each passenger's original 16.16 offset from the activating
 * player.  Return failure rather than wrapping an authored lift destination.
 * This low-level helper reports overflow so callers can choose policy. */
int avp_mp_chamber_translate(int32_t destination_x,int32_t destination_y,
                              uint32_t leader_x,uint32_t leader_y,
                              uint32_t passenger_x,uint32_t passenger_y,
                              int32_t *out_x,int32_t *out_y);
/* Co-op transfer policy: preserve valid 16.16 passenger offsets, but when
 * their translated position would overflow, share the initiator's authored
 * destination instead of leaving that passenger behind or wrapping outside
 * the level. The caller MUST still validate geometry before committing. */
int avp_mp_chamber_translate_or_stack(int32_t destination_x,int32_t destination_y,
                              uint32_t leader_x,uint32_t leader_y,
                              uint32_t passenger_x,uint32_t passenger_y,
                              int32_t *out_x,int32_t *out_y);
/* Source-authored ship-airlock geometry, read-only. Each recess is a
 * distinct 16.16-centred floor cell against an actual left/right wall.
 * face_side points AWAY from that wall (the bot's back faces the wall).
 * A failed lookup must never invent a chamber or a ship transition. */
#define AVP_MP_AIRLOCK_SEATS 4u
typedef struct AvpMpAirlockRoom {
    unsigned exit_x,exit_y,exit_side,destination;
    unsigned entry_x,entry_y,entry_side;
    uint32_t exit_door_offset,entry_door_offset;
    unsigned seat_count;
    struct { unsigned x,y,face_side; } seats[AVP_MP_AIRLOCK_SEATS];
} AvpMpAirlockRoom;
/* R299: a completed ORIGINAL ship-airlock transfer must not become an
 * immediate invitation to press the reverse panel. This is per BOT, not
 * per map, and never changes any Jaguar interlock or door state. A co-op
 * party re-arms the reverse trip only after both the bot and a living human
 * teammate have actually disembarked on the new side. */
typedef struct AvpMpAirlockCycle {
    unsigned arrived_from,arrived_level;
    uint64_t arrived_tick;
} AvpMpAirlockCycle;
void avp_mp_airlock_cycle_arrived(AvpMpAirlockCycle *cycle,
    unsigned from_level,unsigned to_level,uint64_t tick);
int avp_mp_airlock_cycle_reverse_block(AvpMpAirlockCycle *cycle,
    unsigned level,unsigned destination,int bot_inside,
    int human_disembarked,int any_living_human,uint64_t tick);
/* R425: native door-byte/active-slot driven sequence. No state writes. */
typedef enum AvpMpAirlockAction {
    AVP_MP_AIRLOCK_WAIT, AVP_MP_AIRLOCK_CLOSE_ENTRY,
    AVP_MP_AIRLOCK_CLOSE_EXIT, AVP_MP_AIRLOCK_PRESSURE
} AvpMpAirlockAction;
AvpMpAirlockAction avp_mp_airlock_action(const AvpMpAirlockRoom *room,
    const uint8_t *cells, size_t cell_bytes,
    const uint32_t *active_offsets,unsigned active_count);
int avp_mp_airlock_commitment_valid(uint64_t now,uint64_t last_intent,
    int inside,int matching_exit_goal);
struct AvpDoorLevelMeta;
int avp_mp_airlock_room(const AvpMpRetailMap *map,
                       const struct AvpDoorLevelMeta *meta,
                       unsigned exit_panel_offset,AvpMpAirlockRoom *out);
#endif
