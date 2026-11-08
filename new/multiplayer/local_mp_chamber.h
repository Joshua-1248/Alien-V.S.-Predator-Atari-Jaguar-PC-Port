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
#endif
