/* Exercise unmodified original collision.c / doors.c through the optional
 * per-world map adapter. No fake collision algorithms or gameplay stubs. */
#include "local_mp_collision.h"
#include "local_mp_bot.h"
#include "collision.h"
#include "doors.h"
#include "maze.h"
#include <stdio.h>
#include <string.h>

#define CHECK(X) do { if (!(X)) { fprintf(stderr, "FAIL: %s @ %d\n", #X, __LINE__); return 1; } } while (0)

/* The original InitDoors() writes this genuine game global. Other game
 * modules are excluded from this collision-only test using linker GC. */
u32 levfx_ID;

int main(void)
{
    AvpMpRetailMaps store = {0};
    AvpMpRetailMapSource sources[AVP_MP_RETAIL_LEVELS] = {{0}};
    uint8_t originals[AVP_MP_RETAIL_LEVELS][8u + 5u*5u*8u] = {{0}};
    uint8_t external_bound_maze[8] = {0};
    AvpMpRetailMap *a, *a_shared, *b, *c;
    AvpMpCollisionPoint from = {(1<<16)|0x8000, (2<<16)|0x8000};
    AvpMpCollisionPoint to = {(3<<16)|0x8000, (2<<16)|0x8000};
    uint8_t result = 0xffu, mask = 0xffu;
    uint16_t steps = 0xffffu;
    int sight = -1, open = -1;
    unsigned i;
    const int32_t hit_x = (2<<16)|0x1000;
    const int32_t hit_y = (2<<16)|0x8000;
    const size_t center = 8u + (2u*5u+2u)*8u;

    for (i = 0; i < AVP_MP_RETAIL_LEVELS; ++i) {
        uint8_t *p = originals[i];
        p[1] = 5; p[3] = 5;
        sources[i].file_index = AVP_MP_RETAIL_MAZE_FIRST_FILE + i;
        sources[i].decoded = p;
        sources[i].decoded_size = sizeof(originals[0]);
        sources[i].width = 5u; sources[i].height = 5u;
    }
    /* An original Jaguar wall-type 1 (solid) at level 3's center-left face. */
    originals[2][center] = 1;
    CHECK(avp_mp_retail_maps_init(&store, sources, AVP_MP_RETAIL_LEVELS));
    a = avp_mp_retail_maps_acquire(&store, 0u, 3u);
    a_shared = avp_mp_retail_maps_acquire(&store, 0u, 3u);
    b = avp_mp_retail_maps_acquire(&store, 0u, 4u);
    c = avp_mp_retail_maps_acquire(&store, 1u, 3u);
    CHECK(a && a_shared && b && c && a == a_shared && a != b && a != c);

    InitDoors(); /* Original game implementation sets all 256 wall classes. */
    CHECK((fullbits[1]&AVP_WB_SOLID) != 0u);
    avp_collision_bind_maze(external_bound_maze, 1u, 1u);
    CHECK(avp_mp_collision_safe_pos(a, 64, hit_x, hit_y, 40, &result));
    CHECK(result == AVP_COLL_WALL);
    CHECK(avp_collision_maze_data() == external_bound_maze);
    CHECK(avp_collision_maze_width() == 1u && avp_collision_maze_height() == 1u);
    CHECK(avp_mp_collision_safe_pos(b, 64, hit_x, hit_y, 40, &result));
    CHECK(result == AVP_COLL_SAFE);
    /* R24 documented source gap: SafePos accepts an open, wall-free map
     * cell even when the retail floor byte is zero. The multiplayer bot
     * guard rejects that non-floor cell without changing SafePos itself. */
    CHECK(!avp_mp_bot_playable_position(b->bytes+8u,5u,5u,
                                       (u32)hit_x,(u32)hit_y));
    avp_mp_retail_map_cell(b,2u,2u)[5u]=1u;
    CHECK(avp_mp_bot_playable_position(b->bytes+8u,5u,5u,
                                      (u32)hit_x,(u32)hit_y));
    CHECK(avp_mp_collision_safe_pos(b, 64, hit_x, hit_y, 40, &result));
    CHECK(result == AVP_COLL_SAFE);
    CHECK(avp_mp_collision_safe_pos(c, 64, hit_x, hit_y, 40, &result));
    CHECK(result == AVP_COLL_WALL);
    CHECK(avp_mp_collision_allowed_moves(a, 2, 2, &mask));
    CHECK((mask & 1u) == 0u && (mask & 0xeu) == 0xeu);
    CHECK(avp_mp_collision_allowed_moves(b, 2, 2, &mask));
    CHECK((mask & 0xfu) == 0xfu);
    CHECK(avp_mp_collision_n_moves(a, 2, 2, 0, 2, &steps));
    CHECK(steps == 0);
    CHECK(avp_mp_collision_n_moves(b, 2, 2, 0, 2, &steps));
    CHECK(steps == 2);
    CHECK(avp_mp_collision_line_of_sight(a, from, to, &sight));
    /* Reference is the actual COLLIDE.S reconstruction on the SAME map. */
    {
        int direct;
        AvpXY src = {from.x, from.y}, dst = {to.x, to.y};
        avp_collision_bind_maze(a->bytes + 8, 5, 5);
        direct = LineOfSight(&src, &dst);
        avp_collision_bind_maze(external_bound_maze, 1u, 1u);
        CHECK(sight == direct);
    }
    CHECK(avp_mp_collision_open_door_ahead(a, hit_x, hit_y, 1, 0, 64, &open));
    CHECK(open == 0);
    CHECK(avp_collision_maze_data() == external_bound_maze);

    /* Change one CAMPAIGN's door/wall map; other campaign remains separate. */
    avp_mp_retail_map_cell(a, 2, 2)[0] = 0;
    CHECK(avp_mp_collision_safe_pos(a_shared, 64, hit_x, hit_y, 40, &result));
    CHECK(result == AVP_COLL_SAFE);
    CHECK(avp_mp_collision_safe_pos(c, 64, hit_x, hit_y, 40, &result));
    CHECK(result == AVP_COLL_WALL);
    CHECK(originals[2][center] == 1);

    /* An invalid query must not clobber the result or legacy game binding. */
    CHECK(!avp_mp_collision_safe_pos(NULL, 64, hit_x, hit_y, 40, &result));
    CHECK(!avp_mp_collision_safe_pos(a, 64, hit_x, hit_y, 40, NULL));
    a->bytes[1] = 4; /* corrupted header */
    CHECK(!avp_mp_collision_safe_pos(a, 64, hit_x, hit_y, 40, &result));
    a->bytes[1] = 5;
    CHECK(avp_collision_maze_data() == external_bound_maze);
    CHECK(avp_collision_maze_width() == 1u && avp_collision_maze_height() == 1u);

    CHECK(avp_mp_retail_maps_release(&store, 0u, 3u, a));
    CHECK(avp_mp_retail_maps_release(&store, 0u, 3u, a_shared));
    CHECK(!avp_mp_collision_allowed_moves(a, 2, 2, &mask)); /* stale lease */
    CHECK(avp_mp_retail_maps_release(&store, 0u, 4u, b));
    CHECK(avp_mp_retail_maps_release(&store, 1u, 3u, c));
    CHECK(avp_mp_retail_maps_destroy(&store));
    CHECK(avp_collision_maze_data() == external_bound_maze);
    puts("PASS: original Jaguar SafePos, AllowedMoves, NMoves, LOS and door checks on independent level maps; original binding always restored");
    return 0;
}
