#include "local_mp_collision.h"
#include "collision.h"
#include <limits.h>
#include <stddef.h>

/* Only temporarily rebound state is the ORIGINAL collision.c maze pointer and
 * dimensions. Every exit from a valid query restores all three values.
 * The selected map and its lease must outlive the query. Never call these
 * functions from a second thread, re-entrantly, or during an original engine
 * collision call. No other Jaguar globals are saved or exchanged. */
typedef struct AvpMpCollisionBinding {
    uint8_t *bytes;
    uint16_t width, height;
} AvpMpCollisionBinding;

static int begin(const AvpMpRetailMap *map, AvpMpCollisionBinding *saved)
{
    size_t required;
    if (!map || !saved || !map->bytes || !map->leases ||
        !map->width || !map->height || map->width > 64u || map->height > 64u)
        return 0;
    required = 8u + (size_t)map->width * map->height * 8u;
    if (map->size != required ||
        map->bytes[0] != (uint8_t)(map->width >> 8) ||
        map->bytes[1] != (uint8_t)map->width ||
        map->bytes[2] != (uint8_t)(map->height >> 8) ||
        map->bytes[3] != (uint8_t)map->height) return 0;
    saved->bytes = avp_collision_maze_data();
    saved->width = avp_collision_maze_width();
    saved->height = avp_collision_maze_height();
    avp_collision_bind_maze(map->bytes + 8u, (uint16_t)map->width,
                            (uint16_t)map->height);
    return 1;
}

static void finish(const AvpMpCollisionBinding *saved)
{
    avp_collision_bind_maze(saved->bytes, saved->width, saved->height);
}

int avp_mp_collision_safe_pos(const AvpMpRetailMap *map,
                              uint8_t height, int32_t x, int32_t y,
                              uint16_t width, uint8_t *out_hit)
{
    AvpMpCollisionBinding old;
    uint8_t result;
    if (!out_hit || !begin(map, &old)) return 0;
    result = SafePos(height, x, y, width);
    finish(&old);
    *out_hit = result;
    return 1;
}

int avp_mp_collision_allowed_moves(const AvpMpRetailMap *map,
                                   uint16_t x, uint16_t y, uint8_t *out_mask)
{
    AvpMpCollisionBinding old;
    uint8_t result;
    if (!out_mask || !begin(map, &old)) return 0;
    result = AllowedMoves(x, y);
    finish(&old);
    *out_mask = result;
    return 1;
}

int avp_mp_collision_n_moves(const AvpMpRetailMap *map,
                             uint16_t x, uint16_t y, uint8_t direction,
                             uint16_t wanted, uint16_t *out_count)
{
    AvpMpCollisionBinding old;
    uint16_t result;
    if (!out_count || !begin(map, &old)) return 0;
    result = NMoves(x, y, direction, wanted);
    finish(&old);
    *out_count = result;
    return 1;
}

int avp_mp_collision_line_of_sight(const AvpMpRetailMap *map,
                                   AvpMpCollisionPoint from,
                                   AvpMpCollisionPoint to, int *out_blocked)
{
    AvpMpCollisionBinding old;
    AvpXY a, b;
    int result;
    if (!out_blocked || !begin(map, &old)) return 0;
    a.x = from.x; a.y = from.y;
    b.x = to.x; b.y = to.y;
    result = LineOfSight(&a, &b);
    finish(&old);
    *out_blocked = result;
    return 1;
}

int avp_mp_collision_open_door_ahead(const AvpMpRetailMap *map,
                                     int32_t x, int32_t y, int dx, int dy,
                                     uint8_t height, int *out_open)
{
    AvpMpCollisionBinding old;
    int result;
    if (!out_open || !begin(map, &old)) return 0;
    result = avp_collision_open_door_ahead(x, y, dx, dy, height);
    finish(&old);
    *out_open = result;
    return 1;
}
