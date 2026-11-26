/* R158: test-only AMP collision owner / allocator lifecycle probe.
 * Compiles with wrappers appended to either unmodified original or port amp.c.
 * It deliberately does not simulate a physical Jaguar or the main game loop.
 */
#include "amp.h"
#include "collision.h"
#include <stdio.h>
#include <string.h>

extern void avp_r158_reset_free_list(void);
extern unsigned avp_r158_free_count(void);
extern unsigned avp_r158_number(const AvpAmp *a);
extern void avp_r158_clear(AvpAmp *a);
extern void avp_r158_release(AvpAmp *a);
extern int avp_r158_occupied(int x,int y);

static u8 maze[8u*8u*8u];
u8 *avp_collision_maze_data(void){return maze;}
u16 avp_collision_maze_width(void){return 8u;}
u16 avp_collision_maze_height(void){return 8u;}
static void live_mode(AvpAmp *a){(void)a;}

enum { A=3*64+4, B=3*64+5, C=3*64+6 };
static void setup(void){
    initialise();
    avp_r158_reset_free_list();
    memset(maze,0,sizeof(maze));
    for(unsigned i=0;i<64u;i++)maze[i*8u+5u]=1u;
}
static void locate(AvpAmp *a,unsigned x,unsigned y){
    a->xpos=(s32)((x<<16)|0x8000u);
    a->ypos=(s32)((y<<16)|0x8000u);
}
static void snapshot(const char *scenario,const char *step,unsigned allocated, unsigned other){
    printf("%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",scenario,step,
        collmap[A],collmap[B],collmap[C],
        avp_r158_occupied(4,3)?1u:0u,
        avp_r158_occupied(5,3)?1u:0u,
        avp_r158_occupied(6,3)?1u:0u,
        avp_r158_free_count(),allocated,other);
}
static AvpAmp *spawn(unsigned x,unsigned y){
    AvpAmp *a=amp_req();
    locate(a,x,y);
    a->mode=live_mode;
    amp_setgrid(a);
    return a;
}
int main(void){
    AvpAmp *a,*b,*c,*d;
    unsigned i,j;
    printf("scenario,step,grid_a,grid_b,grid_c,block_a,block_b,block_c,free_slots,allocation,second_allocation\n");

    setup();
    snapshot("clear_move_release_reuse","reset",0,0);
    a=spawn(4,3);
    i=avp_r158_number(a);
    snapshot("clear_move_release_reuse","spawn_a",i,0);
    avp_r158_clear(a);locate(a,5,3);amp_setgrid(a);
    snapshot("clear_move_release_reuse","clear_then_move_b",i,0);
    avp_r158_release(a);
    snapshot("clear_move_release_reuse","release_b",i,0);
    b=spawn(6,3);
    j=avp_r158_number(b);
    snapshot("clear_move_release_reuse","reuse_c",i,j);

    setup();
    a=spawn(4,3);i=avp_r158_number(a);
    snapshot("move_without_clear","spawn_a",i,0);
    locate(a,5,3);amp_setgrid(a);
    snapshot("move_without_clear","move_b",i,0);
    avp_r158_release(a);
    snapshot("move_without_clear","release_b",i,0);
    b=spawn(6,3);j=avp_r158_number(b);
    snapshot("move_without_clear","reuse_c",i,j);
    setup();
    snapshot("move_without_clear","level_reset_primitives",0,0);

    setup();
    a=spawn(4,3);b=spawn(4,3);
    i=avp_r158_number(a);j=avp_r158_number(b);
    snapshot("shared_cell","overwrite_owner",i,j);
    avp_r158_release(a);
    snapshot("shared_cell","release_first",i,j);
    avp_r158_release(b);
    snapshot("shared_cell","release_second",i,j);

    setup();
    a=spawn(4,3);b=spawn(4,3);
    i=avp_r158_number(a);j=avp_r158_number(b);
    avp_r158_clear(a);
    snapshot("explicit_clear_foreign","clear_first",i,j);

    setup();
    a=spawn(4,3); b=spawn(5,3);
    i=avp_r158_number(a);j=avp_r158_number(b);
    snapshot("duplicate_release","two_spawned",i,j);
    amp_release(i);
    snapshot("duplicate_release","release_slot_1",i,j);
    amp_release(i);
    snapshot("duplicate_release","release_slot_1_twice",i,j);
    c=amp_req();i=avp_r158_number(c);
    snapshot("duplicate_release","allocate_after_duplicate",i,j);
    d=amp_req();j=avp_r158_number(d);
    snapshot("duplicate_release","allocate_again",i,j);

    setup();
    collmap[A]=0xffffu;
    snapshot("sentinel","excluded_floor",0,0);
    maze[(3u*8u+4u)*8u+5u]=0;
    snapshot("sentinel","excluded_void",0,0);
    maze[(3u*8u+4u)*8u+5u]=1;
    a=spawn(4,3);i=avp_r158_number(a);
    snapshot("sentinel","actor_overlays_exclusion",i,0);
    return 0;
}
