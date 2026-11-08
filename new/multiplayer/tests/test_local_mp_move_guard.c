#include "local_mp_move_guard.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Fixture {
    const uint8_t *cells;
    unsigned width,height;
    int walls;
    unsigned probes;
} Fixture;
static int point(int32_t x,int32_t y,void *opaque)
{
    Fixture *f=(Fixture *)opaque;
    f->probes++;
    /* Original source wall tests reject a player center within 40 pixels
     * of a wall. A high-speed step can have safe endpoints on both sides.
     * This fixture makes the invalid interval explicit without inventing a
     * new runtime damage or movement calculation. */
    if(f->walls && x>=((3<<16)-(40<<9)) &&
                   x<=((3<<16)+(40<<9)))return 1;
    if(!avp_mp_move_guard_floor(f->cells,f->width,f->height,
                                x,y,40u<<9))return 1;
    return 0;
}
int main(void)
{
    uint8_t cells[5u*5u*8u]={0};
    Fixture f={cells,5u,5u,0,0};
    for(unsigned y=1u;y<4u;y++)for(unsigned x=1u;x<4u;x++)
        cells[(y*5u+x)*8u+5u]=1u;
    assert(avp_mp_move_guard_floor(cells,5,5,(2<<16)|0x8000,
                   (2<<16)|0x8000,40u<<9));
    /* A centered floor cell with no floor next door is NOT safe if the
     * player physically straddles that edge. */
    cells[(2u*5u+3u)*8u+5u]=0;
    assert(avp_mp_move_guard_floor(cells,5,5,(2<<16)|0x8000,
                   (2<<16)|0x8000,40u<<9));
    assert(!avp_mp_move_guard_floor(cells,5,5,(2<<16)|0xe000,
                   (2<<16)|0x8000,40u<<9));
    assert(!avp_mp_move_guard_floor(cells,5,5,0,
                   (2<<16)|0x8000,40u<<9));
    /* A fast step across floorless space is rejected even when its target
     * cell is walkable.  Without the sweep both endpoints are accepted. */
    cells[(2u*5u+3u)*8u+5u]=1u;
    cells[(2u*5u+2u)*8u+5u]=0;
    assert(!point((1<<16)|0x8000,(2<<16)|0x8000,&f));
    assert(!point((3<<16)|0x8000,(2<<16)|0x8000,&f));
    assert(avp_mp_move_guard_sweep((1<<16)|0x8000,(2<<16)|0x8000,
                 (3<<16)|0x8000,(2<<16)|0x8000,0x2000u,point,&f));
    assert(f.probes>=4u);
    /* Even with floors everywhere, a mid-segment original wall blocks. */
    cells[(2u*5u+2u)*8u+5u]=1u;
    f.walls=1;f.probes=0;
    assert(!point((2<<16)|0x8000,(2<<16)|0x8000,&f));
    assert(!point((3<<16)|0x8000,(2<<16)|0x8000,&f));
    assert(avp_mp_move_guard_sweep((2<<16)|0x8000,(2<<16)|0x8000,
                 (3<<16)|0x8000,(2<<16)|0x8000,0x2000u,point,&f)==1);
    /* A safe diagonal high-speed step remains allowed and has its exact
     * requested endpoint; no extra physics ticks or acceleration change. */
    f.walls=0;f.probes=0;
    assert(avp_mp_move_guard_sweep((1<<16)|0x8000,(1<<16)|0x8000,
                 (2<<16)|0x8000,(2<<16)|0x8000,0x2000u,point,&f)==0);
    assert(f.probes==8u);
    assert(avp_mp_move_guard_sweep((2<<16)|0x8000,(2<<16)|0x8000,
                 (2<<16)|0x8000,(2<<16)|0x8000,0x2000u,point,&f)==0);
    assert(avp_mp_move_guard_sweep(0,0,INT32_MAX,INT32_MAX,
                 0x2000u,point,&f)!=0); /* implausible teleport fails closed */
    puts("PASS: player footprint, void cell, outside bounds, fast swept wall/void, safe diagonal");
    return 0;
}
