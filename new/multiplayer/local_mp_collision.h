#ifndef AVP_NEW_LOCAL_MP_COLLISION_H
#define AVP_NEW_LOCAL_MP_COLLISION_H

#include "local_mp_retail_maps.h"
#include <stdint.h>

/* Narrow read-only interface to the ACTUAL Jaguar COLLIDE.S C reconstruction.
 * Calls are serialized on the owning game thread: collision.c has one legacy
 * bound maze pointer. Every successful query saves/restores that binding.
 * This is NOT an engine snapshot, a level tick, or a substitute collision
 * algorithm. The door-classification table must have been initialized by
 * the original InitDoors() before any query.
 *
 * The bytes after the 8-byte retail FILES header are passed unchanged to
 * original SafePos/AllowedMoves/NMoves/LineOfSight. A hit code retains original
 * meanings: 0=clear, 1=wall, 2=door (SafePos may OR wall/door bits).
 * No original gameplay files or any player / AMP state are altered.
 */
typedef struct AvpMpCollisionPoint {
    int32_t x, y; /* Jaguar 16.16 coordinates */
} AvpMpCollisionPoint;

int avp_mp_collision_safe_pos(const AvpMpRetailMap *map,
                              uint8_t sprite_height, int32_t x, int32_t y,
                              uint16_t sprite_width, uint8_t *out_hit);
int avp_mp_collision_allowed_moves(const AvpMpRetailMap *map,
                                   uint16_t cell_x, uint16_t cell_y,
                                   uint8_t *out_mask);
int avp_mp_collision_n_moves(const AvpMpRetailMap *map,
                             uint16_t cell_x, uint16_t cell_y,
                             uint8_t direction, uint16_t wanted,
                             uint16_t *out_count);
int avp_mp_collision_line_of_sight(const AvpMpRetailMap *map,
                                   AvpMpCollisionPoint from,
                                   AvpMpCollisionPoint to, int *out_blocked);
int avp_mp_collision_open_door_ahead(const AvpMpRetailMap *map,
                                     int32_t x, int32_t y, int dx, int dy,
                                     uint8_t sprite_height, int *out_open);

#endif
