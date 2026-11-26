/* R157: isolated original-AMP movement-grid decision differential.
 * Build with amp.c plus appended TEST-ONLY wrappers (see Python runner).
 * No production game code or original-ROM assets are modified.
 */
#include "amp.h"
#include "collision.h"
#include <stdio.h>
#include <string.h>

extern int avp_r157_cell(int x,int y);
extern int avp_r157_neighbor(const AvpAmp *a,int dx,int dy);
static u8 maze[8u*8u*8u];
u8 *avp_collision_maze_data(void){return maze;}
u16 avp_collision_maze_width(void){return 8;}
u16 avp_collision_maze_height(void){return 8;}
static void active_mode(AvpAmp *a){(void)a;}

struct Scenario { const char *name; u16 occupant; int floor; int active; int same_cell; int host_static; };
static const struct Scenario sc[]={
    {"empty",0,1,0,0,0},
    {"retail_excluded_floor",0xffffu,1,0,0,0},
    {"retail_excluded_void",0xffffu,0,0,0,0},
    {"live_actor_correct_cell",1,1,1,1,0},
    {"live_actor_stale_cell",1,1,1,0,0},
    {"inactive_actor_owner",1,1,0,1,0},
    {"static_actor_owner",1,1,0,1,1},
    {"invalid_actor_slot",AVP_NUM_AMPS+1u,1,0,0,0},
    {"max_valid_inactive_slot",AVP_NUM_AMPS,1,0,0,0}
};

int main(void)
{
    unsigned n=0;
    printf("scenario,check,occupied\n");
    for(unsigned s=0;s<sizeof(sc)/sizeof(sc[0]);s++){
        const struct Scenario *t=&sc[s];
        const unsigned cell=3u*64u+4u;
        memset(collmap,0,sizeof(collmap));
        memset(level1amps,0,sizeof(level1amps));
        memset(maze,0,sizeof(maze));
        for(unsigned i=0;i<64u;i++)maze[i*8u+5u]=1;
        maze[(3u*8u+4u)*8u+5u]=(u8)t->floor;
        collmap[cell]=t->occupant;
        level1amps[0].xpos=(s32)(((t->same_cell?4u:5u)<<16)|0x8000u);
        level1amps[0].ypos=(s32)((3u<<16)|0x8000u);
        level1amps[0].mode=t->active?active_mode:NULL;
        level1amps[0].host_static=(u8)t->host_static;
        AvpAmp probe={0};
        probe.xpos=(s32)((3u<<16)|0xf000u);
        probe.ypos=(s32)((3u<<16)|0x8000u);
        probe.creature=AC_HUMAN;
#define CHECK(label,result) do{printf("%s,%s,%d\n",t->name,label,(result)!=0);n++;}while(0)
        CHECK("cell_east",avp_r157_cell(4,3));
        CHECK("cell_clear",avp_r157_cell(2,3));
        CHECK("cell_outside_negative",avp_r157_cell(-1,3));
        CHECK("cell_outside_positive",avp_r157_cell(64,3));
        CHECK("move_east_near",avp_r157_neighbor(&probe,1,0));
        probe.xpos=(s32)((3u<<16)|0x8000u);
        CHECK("move_east_far",avp_r157_neighbor(&probe,1,0));
        probe.xpos=(s32)((3u<<16)|0xf000u);
        probe.ypos=(s32)((3u<<16)|0xf000u);
        CHECK("move_diagonal_near",avp_r157_neighbor(&probe,1,1));
        probe.ypos=(s32)((3u<<16)|0x8000u);
        CHECK("move_diagonal_far_y",avp_r157_neighbor(&probe,1,1));
        probe.creature=AC_AQUEEN;
        CHECK("queen_ignores_grid",avp_r157_neighbor(&probe,1,0));
#undef CHECK
    }
    (void)n;
    return 0;
}
