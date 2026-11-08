/* R249 DOORS.S signed 16.16 boundary reconstruction.
 * Directly call the actual static init_dcoll routine in its source translation
 * unit: preserve original 68000 fixed-point word and grid indices. */
#include "doors.h"
#include <assert.h>
#include <stdint.h>
#include "../src/game/doors.c"
u32 x_pos, y_pos;
static void check(s16 x, s16 y, int dx, int dy, unsigned side)
{
    DoorState d={0};
    const s16 grid_sides[4][2]={{0,0},{0,0},{1,0},{0,1}};
    s16 ax=(s16)(x+dx+grid_sides[side&3u][0]);
    s16 ay=(s16)(y+dy+grid_sides[side&3u][1]);
    x_pos=(u32)(u16)x<<16;
    y_pos=(u32)(u16)y<<16;
    init_dcoll(&d,side,dx,dy);
    assert((u32)d.xpos==((u32)(u16)ax<<16));
    assert((u32)d.ypos==((u32)(u16)ay<<16));
    assert(d.index==((side&1u)?4u:0u));
    assert(d.grid1==(u16)((u16)ay*64u+(u16)ax));
}
int main(void)
{
    check(-1,-2,0,0,0);
    check(0,0,-1,-1,0);
    check(0,0,0,-1,3);
    check(1,1,0,0,2);
    check(32767,-32768,0,0,1);
    for(int n=-32768;n<=32767;n++){
        const s16 a=(s16)n,b=(s16)(n^0x5a5a);
        check(a,b,0,0,(unsigned)n&3u);
    }
    return 0;
}
