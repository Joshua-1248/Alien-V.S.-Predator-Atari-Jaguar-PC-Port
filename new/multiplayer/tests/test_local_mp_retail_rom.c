#include "local_mp_retail_maps.h"
#include "local_mp_collision.h"
#include "collision.h"
#include "levels.h"
#include "local_mp_move_guard.h"
#include "doors.h"
#include "maze.h"
#include "avp_resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Original door table initialization writes this Jaguar game-global. */
u32 levfx_ID;
#define CHECK(X) do { if (!(X)) {fprintf(stderr,"FAIL %s at %d\n",#X,__LINE__);goto fail;} } while (0)
int main(int argc, char **argv)
{
    FILE *fp = NULL;
    AvpRomImage rom = {0};
    AvpMazeCandidate candidates[64] = {{0}};
    AvpMpRetailMapSource sources[AVP_MP_RETAIL_LEVELS] = {{0}};
    AvpMpRetailMaps store = {0};
    unsigned found = 0, i, seen = 0;
    int result = 1;
    long size;
    if (argc != 2) {fprintf(stderr,"usage: test_local_mp_retail_rom ORIGINAL_AVP.jag\n");return 2;}
    fp = fopen(argv[1], "rb"); CHECK(fp != NULL);
    CHECK(fseek(fp,0,SEEK_END)==0);
    size = ftell(fp); CHECK(size == 4194304L);
    CHECK(fseek(fp,0,SEEK_SET)==0);
    rom.data = malloc((size_t)size); CHECK(rom.data != NULL);
    CHECK(fread(rom.data,1,(size_t)size,fp)==(size_t)size);
    fclose(fp); fp=NULL;
    rom.size = (size_t)size;
    rom.exact_retail_sha256 = 1; /* Caller supplies the audited retail ROM. */
    CHECK(avp_resources_bind_retail_level_info(&rom));
    found = avp_resources_find_mazes(&rom, candidates, 64u);
    CHECK(found && found <= 64u);
    for (i = 0; i < found; ++i) {
        AvpMazeCandidate *m = &candidates[i];
        if (m->file_index < 87u || m->file_index > 101u) continue;
        CHECK(seen < AVP_MP_RETAIL_LEVELS);
        sources[seen].file_index = m->file_index;
        sources[seen].decoded = m->decoded;
        sources[seen].decoded_size = m->decoded_size;
        sources[seen].width = m->width;
        sources[seen].height = m->height;
        ++seen;
    }
    CHECK(seen == AVP_MP_RETAIL_LEVELS);
    CHECK(avp_mp_retail_maps_init(&store, sources, seen));
    /* Original Jaguar InitDoors() provides the original wall classifications.
     * A standalone test has no live game state or renderer to disturb. */
    InitDoors();
    for (i = 1; i <= AVP_MP_RETAIL_LEVELS; ++i) {
        AvpMpRetailMap *m = avp_mp_retail_maps_acquire(&store,2u,i);
        CHECK(m && m->bytes);
        CHECK(m->size == store.original_size[i-1u]);
        CHECK(memcmp(m->bytes,store.original[i-1u],m->size)==0);
        /* R70: original authored level starts must fit on their map floor,
         * not just pass a point-only SafePos check. */
        {
            u32 sx,sy,sa;
            CHECK(avp_levels_start_pose(i,&sx,&sy,&sa));
            CHECK(avp_mp_move_guard_floor(m->bytes+8u,m->width,m->height,
                                        (int32_t)sx,(int32_t)sy,40u<<9));
        }
        /* Exact collision.C reconstruction parity against the SAME retail
         * bytes and the real InitDoors() table, including near-wall checks.
         * Test multiple positions on each of all 15 decoded ROM levels. */
        {
            unsigned sample;
            u8 prior[8] = {0};
            unsigned xs[4] = {0u, m->width / 2u, m->width-1u, m->width > 2u ? 1u : 0u};
            unsigned ys[4] = {0u, m->height / 2u, m->height-1u, m->height > 2u ? 1u : 0u};
            avp_collision_bind_maze(prior, 1u, 1u);
            for (sample=0; sample<4; ++sample) {
                s32 xpos=(s32)((xs[sample]<<16)|0x1000u);
                s32 ypos=(s32)((ys[sample]<<16)|0x8000u);
                u8 direct, world, moves_direct, moves_world;
                u16 steps_direct, steps_world;
                avp_collision_bind_maze(m->bytes+8u, (u16)m->width, (u16)m->height);
                direct=SafePos(64u, xpos, ypos, 40u);
                moves_direct=AllowedMoves((u16)xs[sample],(u16)ys[sample]);
                steps_direct=NMoves((u16)xs[sample],(u16)ys[sample],2u,2u);
                avp_collision_bind_maze(prior, 1u, 1u);
                CHECK(avp_mp_collision_safe_pos(m,64u,xpos,ypos,40u,&world));
                CHECK(world==direct);
                CHECK(avp_mp_collision_allowed_moves(m,(u16)xs[sample],(u16)ys[sample],&moves_world));
                CHECK(moves_world==moves_direct);
                CHECK(avp_mp_collision_n_moves(m,(u16)xs[sample],(u16)ys[sample],2u,2u,&steps_world));
                CHECK(steps_world==steps_direct);
                CHECK(avp_collision_maze_data()==prior);
                CHECK(avp_collision_maze_width()==1u && avp_collision_maze_height()==1u);
            }
            avp_collision_bind_maze(NULL, 0u, 0u);
        }
        CHECK(avp_mp_retail_maps_release(&store,2u,i,m));
    }
    /* Actual retail map from two separate levels remains distinct when
       one is modified. Return to a level must retain the modified bytes. */
    {
        AvpMpRetailMap *a = avp_mp_retail_maps_acquire(&store,2u,7u);
        AvpMpRetailMap *b = avp_mp_retail_maps_acquire(&store,2u,12u);
        uint8_t *cell;
        uint8_t before;
        CHECK(a && b);
        cell = avp_mp_retail_map_cell(a,0,0); CHECK(cell);
        before = cell[2]; cell[2] ^= 0xFFu;
        CHECK(memcmp(a->bytes,store.original[6u],a->size)!=0);
        CHECK(memcmp(b->bytes,store.original[11u],b->size)==0);
        CHECK(avp_mp_retail_maps_release(&store,2u,7u,a));
        CHECK(avp_mp_retail_maps_release(&store,2u,12u,b));
        a = avp_mp_retail_maps_acquire(&store,2u,7u);
        CHECK(a && avp_mp_retail_map_cell(a,0,0)[2] == (uint8_t)(before ^ 0xFFu));
        CHECK(avp_mp_retail_maps_release(&store,2u,7u,a));
        CHECK(avp_mp_retail_maps_reset_campaign(&store,2u));
        CHECK(memcmp(a->bytes,store.original[6u],a->size)==0);
    }
    puts("PASS: all 15 real retail levels copied and 60 original COLLIDE.S SafePos/AllowedMoves/NMoves parity samples through independently bound contexts; resource bytes unchanged");
    result=0;
fail:
    if(fp)fclose(fp);
    (void)avp_mp_retail_maps_destroy(&store);
    avp_resources_free_mazes(candidates, found <= 64u ? found : 64u);
    free(rom.data);
    return result;
}
