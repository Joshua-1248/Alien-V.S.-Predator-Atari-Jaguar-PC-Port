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
    /* Two living players in one world collide at the exact combined source
     * half-width; both solo and fast-swept moves use this same geometry. */
    {
        AvpMpPeerCollisionView peers={0};
        const int32_t px=(3<<16)|0x8000,py=(3<<16)|0x8000;
        peers.count=16u;peers.active[1]=1u;
        peers.x[1]=(uint32_t)px;peers.y[1]=(uint32_t)py;
        assert(!avp_mp_move_guard_peers(&peers,px,py));
        assert(!avp_mp_move_guard_peers(&peers,px+AVP_MP_PEER_CLEARANCE-1,py));
        assert(avp_mp_move_guard_peers(&peers,px+AVP_MP_PEER_CLEARANCE,py));
        /* On enabling the toggle mid-overlap, moving OUT remains legal,
         * moving further INTO the other player remains blocked. */
        peers.from_x=px+0x3000;peers.from_y=py;
        assert(avp_mp_move_guard_peers(&peers,px+0x5000,py));
        assert(!avp_mp_move_guard_peers(&peers,px+0x2000,py));
        assert(!avp_mp_move_guard_peers(&peers,px+0x2000,py+0x800));
        peers.active[1]=0u;
        assert(avp_mp_move_guard_peers(&peers,px,py));
    }
    /* When players are spawned/transported atop each other, they remain
     * non-solid ONLY until the two source-sized footprints separate. */
    {
        AvpMpPeerGrace grace={0};
        uintptr_t world[AVP_MP_PEER_COUNT]={0};
        uint32_t x[AVP_MP_PEER_COUNT]={0},y[AVP_MP_PEER_COUNT]={0};
        world[0]=world[1]=1u;x[0]=x[1]=3u<<16;
        y[0]=y[1]=3u<<16;
        avp_mp_peer_grace_update(&grace,3u,world,x,y,3u);
        assert(avp_mp_peer_grace_exempt(&grace,0u,1u));
        x[1]+=AVP_MP_PEER_CLEARANCE;
        avp_mp_peer_grace_update(&grace,3u,world,x,y,0u);
        assert(!avp_mp_peer_grace_exempt(&grace,0u,1u));
        x[1]=x[0];
        avp_mp_peer_grace_update(&grace,3u,world,x,y,0u);
        assert(!avp_mp_peer_grace_exempt(&grace,0u,1u));
        avp_mp_peer_grace_update(&grace,3u,world,x,y,2u);
        assert(avp_mp_peer_grace_exempt(&grace,0u,1u));
        world[1]=2u;
        avp_mp_peer_grace_update(&grace,3u,world,x,y,3u);
        assert(!avp_mp_peer_grace_exempt(&grace,0u,1u));
        world[1]=1u;
        /* All 16 actors use the same pair rule; bot P5 vs P16. */
        world[4]=world[15]=1u;x[4]=x[15]=5u<<16;
        y[4]=y[15]=3u<<16;
        avp_mp_peer_grace_update(&grace,(1u<<4)|(1u<<15),world,x,y,
                                 (1u<<4)|(1u<<15));
        assert(avp_mp_peer_grace_exempt(&grace,4u,15u));
    }
    puts("PASS: player footprint, void cell, outside bounds, fast swept wall/void, safe diagonal");
    return 0;
}
