#include <assert.h>
#include <string.h>
#include "collision.h"
#include "doors.h"
#include "amp.h"
#include "player.h"
#include "levels.h"

/* globals required by collision/player-facing link */
u32 x_pos,y_pos,centre_angle,fire_distance; s16 player_energy,max_energy; u16 maze_width,maze_height;
AvpAmp level1amps[AVP_NUM_AMPS],*amp_data=level1amps,*amps_at=level1amps;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H]; u8 objmap[AVP_AMP_OBJ_ROW_BYTES*AVP_AMP_OBJ_H];
u16 levels_visit,discflag; s16 cur_level; u8 fullbits[256];
static void alive_actor(AvpAmp *a){(void)a;}

int main(void){
    /* 4x4 maze, matching the historical width-as-row-stride layout. */
    u8 maze[4*4*8]; memset(maze,0,sizeof maze); memset(fullbits,0,sizeof fullbits); fullbits[1]=1; avp_collision_bind_maze(maze,4,4);
    maze_width=4;maze_height=4;
    /* Open room is safe. */
    assert(SafePos(64,(1<<16)|0x8000,(1<<16)|0x8000,10)==AVP_COLL_SAFE);
    /* solid left wall type 1 catches near-left position */
    maze[(1*4+1)*8+0]=1;
    assert(SafePos(64,(1<<16)|0x0100,(1<<16)|0x8000,10)==AVP_COLL_WALL);
    maze[(1*4+1)*8+0]=0;

    /* quick path movement: wall number 1 blocks, 0 and >=0x20 pass */
    maze[(1*4+1)*8+2]=1;
    assert((AllowedMoves(1,1)&(1u<<2))==0);
    maze[(1*4+1)*8+2]=0x20;
    assert(AllowedMoves(1,1)&(1u<<2));

    /* Check both door faces, and only offer alignment for an actually
     * passable open doorway. Closed doors and solid opposite faces remain
     * solid; the steering predicate never changes SafePos collision. */
    {
        const unsigned left_cell=(1u*4u+1u)*8u;
        const unsigned right_cell=(1u*4u+2u)*8u;
        fullbits[0x20]=AVP_WB_DOOR|AVP_WB_2SIDES;
        maze[left_cell+2u]=0x20u;
        maze[right_cell+6u]=0u;
        assert(!avp_collision_open_door_ahead(0x0001d000,0x00011900,1,0,68));
        maze[right_cell+6u]=56u;
        assert(avp_collision_open_door_ahead(0x0001d000,0x00011900,1,0,68));
        assert(SafePos(68,0x0001d000,0x00011900,46)!=AVP_COLL_SAFE);
        assert(SafePos(68,0x0001d000,0x00018000,46)==AVP_COLL_SAFE);
        fullbits[1]=AVP_WB_SOLID;
        maze[right_cell+0u]=1u;
        assert(!avp_collision_open_door_ahead(0x0001d000,0x00011900,1,0,68));
        maze[right_cell+0u]=0u;
        maze[left_cell+2u]=0u;
        maze[right_cell+6u]=0u;
        /* Matching south/north wall faces use the same live door byte. */
        {
            const unsigned below=(2u*4u+1u)*8u;
            maze[left_cell+3u]=0x20u;
            assert(!avp_collision_open_door_ahead(0x00018000,0x0001d000,0,1,68));
            maze[below+7u]=56u;
            assert(avp_collision_open_door_ahead(0x00018000,0x0001d000,0,1,68));
            maze[below+1u]=1u;
            assert(!avp_collision_open_door_ahead(0x00018000,0x0001d000,0,1,68));
            maze[below+1u]=0u;
            maze[left_cell+3u]=0u;
            maze[below+7u]=0u;
        }
    }

    /* A right-edge door must never borrow the left-door byte from the next
     * row. Likewise, an open-byte written in adjacent memory must not make
     * a perimeter exit traversable. Both expose invalid map boundaries. */
    {
        const unsigned edge=(1u*4u+3u)*8u;
        const unsigned next_row=(2u*4u)*8u;
        fullbits[0x20]=AVP_WB_DOOR|AVP_WB_2SIDES;
        maze[edge+2u]=0x20u;
        maze[next_row+6u]=56u;
        assert(SafePos(68,(3<<16)|0xf000,(1<<16)|0x8000,46)==AVP_COLL_WALL);
        maze[edge+2u]=0;
        maze[next_row+6u]=0;
        /* A door on the top map edge cannot lead to a valid cell. */
        maze[(0u*4u+1u)*8u+1u]=0x20u;
        maze[(0u*4u+1u)*8u+7u]=56u;
        assert(SafePos(68,(1<<16)|0x8000,0x00001000,46)==AVP_COLL_WALL);
        maze[(0u*4u+1u)*8u+1u]=0;
        maze[(0u*4u+1u)*8u+7u]=0;
    }

    /* AMP collision uses one-based collmap IDs and source collision widths. */
    memset(collmap,0,sizeof collmap); memset(level1amps,0,sizeof level1amps);
    level1amps[0].creature=AC_ALIEN; level1amps[0].xpos=(1<<16)|0x8000; level1amps[0].ypos=(1<<16)|0x8000;
    level1amps[0].mode=alive_actor;
    collmap[1*AVP_AMP_GRID_W+1]=1;
    assert(AMPCollisions((1<<16)|0x8000,(1<<16)|0x8000));
    assert(!AMPCollisions((3<<16)|0x8000,(3<<16)|0x8000));
    /* Recycled/inactive AMP records cannot keep the player physically
     * blocked after their grid entry has become stale.  An intentional
     * stationary object still blocks with host_static set. */
    level1amps[0].mode=NULL;
    assert(!AMPCollisions((1<<16)|0x8000,(1<<16)|0x8000));
    level1amps[0].host_static=1;
    assert(AMPCollisions((1<<16)|0x8000,(1<<16)|0x8000));
    level1amps[0].host_static=0;
    level1amps[0].mode=alive_actor;
    level1amps[0].xpos=(2<<16)|0x8000;
    assert(!AMPCollisions((1<<16)|0x8000,(1<<16)|0x8000));
    collmap[1*AVP_AMP_GRID_W+1]=0;
    collmap[1*AVP_AMP_GRID_W+2]=1;
    assert(AMPCollisions((2<<16)|0x8000,(1<<16)|0x8000));

    /* LOS in an open room succeeds; a solid right wall blocks crossing. */
    { AvpXY a={(1<<16)|0x8000,(1<<16)|0x8000},b={(2<<16)|0x8000,(1<<16)|0x8000};
      assert(LineOfSight(&a,&b)==0);
      maze[(1*4+1)*8+2]=1;
      assert(LineOfSight(&a,&b)!=0);
    }
    return 0;
}
