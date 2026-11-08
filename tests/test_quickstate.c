#include "pc/pc_quickstate.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Minimal linker-model of the writable 68000 state segment.  The production
 * executable supplies these same two symbols through avp_game_state.ld. */
__asm__(".pushsection .data\n"
        ".globl __avp_game_state_start\n"
        "__avp_game_state_start:\n"
        ".zero 64\n"
        ".globl __avp_game_state_end\n"
        "__avp_game_state_end:\n"
        ".popsection\n");
extern unsigned char __avp_game_state_start[];

int main(void)
{
    u8 maze_a[32],maze_b[32];
    u64 tick=0;
    AvpPcQuickState *q;
    assert(avp_pc_quickstate_core_bytes()==64u);
    q=avp_pc_quickstate_create();assert(q);
    assert(!avp_pc_quickstate_valid(q));
    assert(!avp_pc_quickstate_load(q,&tick));
    memset(maze_a,0x71,sizeof(maze_a));
    memset(maze_b,0x92,sizeof(maze_b));
    memset(__avp_game_state_start,0x36,64);
    assert(avp_pc_quickstate_save(q,maze_a,2,2,12345u));
    memset(maze_a,0x51,sizeof(maze_a));
    memset(__avp_game_state_start,0x99,64);
    /* Simulate switching to another level: the current renderer uses maze_b
     * while the original level's allocation (maze_a) remains live. */
    assert(avp_pc_quickstate_load(q,&tick));
    assert(tick==12345u);
    assert(maze_a[0]==0x71u&&maze_a[31]==0x71u);
    assert(maze_b[0]==0x92u&&maze_b[31]==0x92u);
    assert(__avp_game_state_start[0]==0x36u);
    assert(__avp_game_state_start[63]==0x36u);
    assert(avp_pc_quickstate_width(q)==2&&avp_pc_quickstate_height(q)==2);
    avp_pc_quickstate_destroy(q);
    puts("Session quickstate cross-level restore tests passed");
    return 0;
}
