#include "jaguar/avp_gpu/state.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
/* Test the authored maze cell topology used by GPU_AUTOMAP discovery.
   Observation is a CPU simulation operation and never needs a framebuffer. */
static unsigned count_seen(const AvpAvpGpuState *s)
{
    unsigned n=0;
    for(unsigned i=0;i<64u*64u;++i)n+=(s->automap_seen[i]!=0u);
    return n;
}
static u8 *cell(u8 *m,unsigned x,unsigned y){return m+((y*8u)+x)*8u;}
int main(void)
{
    AvpAvpGpuState *s=(AvpAvpGpuState*)calloc(1,sizeof(*s));
    u8 m[8u*8u*8u]={0};
    assert(s);
    /* An east-facing corridor with a wall to the east and north of (2,2)
       must never reveal the cells behind those opaque walls. */
    cell(m,2,2)[2]=1u;
    cell(m,2,2)[1]=1u;
    avp_gpu_automap_observe(s,m,8,8,2u<<16,2u<<16,0,3);
    assert(s->automap_seen[2u*64u+2u]);
    assert(!s->automap_seen[2u*64u+3u]);
    assert(!s->automap_seen[1u*64u+2u]);
    assert(s->automap_seen[3u*64u+2u]);
    unsigned n=count_seen(s);
    assert(n>0);
    /* A repeated higher-FPS drawing must not alter discovery state. */
    assert(count_seen(s)==n);
    /* A different level starts with an independent empty exploration map. */
    memset(m,0,sizeof(m));
    avp_gpu_automap_observe(s,m,8,8,6u<<16,6u<<16,0,4);
    assert(s->automap_level==4u);
    assert(!s->automap_seen[2u*64u+2u]);
    assert(s->automap_seen[6u*64u+6u]);
    /* Crossing between cells is controlled by the wall authored on the
       current side.  The facing edge of the adjacent cell may differ. */
    memset(m,0,sizeof(m));avp_gpu_automap_reset(s);
    cell(m,3,3)[2]=1u; /* east wall from cell 3,3 */
    avp_gpu_automap_observe(s,m,8,8,3u<<16,3u<<16,0,4);
    assert(s->automap_seen[3u*64u+3u]);
    assert(!s->automap_seen[3u*64u+4u]);
    /* The opposite approach is open if the cell on that side has no wall. */
    avp_gpu_automap_reset(s);
    avp_gpu_automap_observe(s,m,8,8,4u<<16,3u<<16,0x80000000u,4);
    assert(s->automap_seen[3u*64u+3u]);
    /* The 64x64 explored state is a host visual snapshot, separate from
       gameplay save data: a restored stage keeps its own discovery bitmap. */
    { AvpGpuAutomapSnapshot saved;
      avp_gpu_automap_save(s,&saved);
      avp_gpu_automap_reset(s);
      assert(!s->automap_seen[3u*64u+3u]);
      avp_gpu_automap_load(s,&saved);
      assert(s->automap_level==4u && s->automap_seen[3u*64u+3u]);
    }
    /* Reject out-of-range positions, even if the host is navigating noclip. */
    n=count_seen(s);
    avp_gpu_automap_observe(s,m,8,8,99u<<16,99u<<16,0,4);
    assert(count_seen(s)==n);
    /* MAZE5.GAS::map_jumps: exact octant rounding at a 45-degree
       boundary, no floating-point trigonometry or ambiguous ties. */
    memset(m,0,sizeof(m));
    avp_gpu_automap_reset(s);
    cell(m,3,3)[2]=1u; /* block east */
    avp_gpu_automap_observe(s,m,8,8,3u<<16,3u<<16,0x1fffffff,4);
    assert(!s->automap_seen[3u*64u+4u]);
    avp_gpu_automap_reset(s);
    avp_gpu_automap_observe(s,m,8,8,3u<<16,3u<<16,0x20000000u,4);
    assert(s->automap_seen[2u*64u+3u]); /* rounded toward north */
    /* Source fullbits classifies panels 13..15 as non-solid and
       non-door.  They may be rendered in the world, but the GPU map
       must see through them rather than truncating corridor traces. */
    memset(m,0,sizeof(m));
    avp_gpu_automap_reset(s);
    cell(m,2,2)[2]=(u8)(13u*8u+1u);
    avp_gpu_automap_observe(s,m,8,8,2u<<16,2u<<16,0,4);
    assert(s->automap_seen[2u*64u+3u]);
    avp_gpu_automap_reset(s);
    cell(m,2,2)[2]=(u8)(5u*8u+1u); /* door stops map trace */
    avp_gpu_automap_observe(s,m,8,8,2u<<16,2u<<16,0,4);
    assert(!s->automap_seen[2u*64u+3u]);
    /* Check all eight input octants against the source jump table.
       Block three exits to distinguish the forward trace from side scans. */
    for(unsigned o=0;o<8u;o++){
        static const unsigned source_jumps[4]={2u,1u,0u,3u};
        unsigned dir=source_jumps[((o+1u)>>1)&3u];
        int tx=3,ty=3;
        memset(m,0,sizeof(m));
        avp_gpu_automap_reset(s);
        for(unsigned k=0;k<4u;k++)cell(m,3,3)[k]=(k==dir)?0u:1u;
        if(dir==0u)--tx;
        else if(dir==1u)--ty;
        else if(dir==2u)++tx;
        else ++ty;
        avp_gpu_automap_observe(s,m,8,8,3u<<16,3u<<16,(u32)(o<<29),4);
        assert(s->automap_seen[(unsigned)ty*64u+(unsigned)tx]);
    }
    /* Original MAZE5.GAS one-bit wall and door raster projection. */
    {
        u8 bits[AVP_AUTOMAP_RASTER_BYTES];
        memset(m,0,sizeof(m));
        avp_gpu_automap_reset(s);
        cell(m,4,4)[1]=41u; /* type 5 door, north */
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        for(size_t k=0;k<sizeof(bits);k++)assert(bits[k]==0u);
        s->automap_seen[4u*64u+4u]=1u;
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        /* pat_topdoor = $B3, pat_botdoor = $CD, in adjacent rows. */
        assert(bits[31u*AVP_AUTOMAP_ROW_BYTES+4u]==0xb3u);
        assert(bits[32u*AVP_AUTOMAP_ROW_BYTES+4u]==0xcdu);
        cell(m,4,4)[1]=1u; /* solid wall: two full rows */
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        assert(bits[31u*AVP_AUTOMAP_ROW_BYTES+4u]==0xffu);
        assert(bits[32u*AVP_AUTOMAP_ROW_BYTES+4u]==0xffu);
        cell(m,4,4)[1]=(u8)(13u*8u+1u); /* decorative edge skipped */
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        assert(bits[31u*AVP_AUTOMAP_ROW_BYTES+4u]==0u);
        cell(m,4,4)[1]=0u;
        cell(m,4,4)[2]=(u8)(5u*8u+1u); /* right-side stair-step door */
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        assert((bits[32u*AVP_AUTOMAP_ROW_BYTES+4u]&1u)!=0u);
        assert((bits[32u*AVP_AUTOMAP_ROW_BYTES+5u]&0x80u)!=0u);
        assert((bits[34u*AVP_AUTOMAP_ROW_BYTES+4u]&1u)==0u);
        assert((bits[34u*AVP_AUTOMAP_ROW_BYTES+5u]&0x80u)!=0u);
        assert((bits[36u*AVP_AUTOMAP_ROW_BYTES+4u]&1u)!=0u);
        assert((bits[36u*AVP_AUTOMAP_ROW_BYTES+5u]&0x80u)==0u);
        /* Original right_door byte-stepping pattern from MAZE5.GAS:
           both sides, inner only, outer only, outer only, inner only,
           inner only, outer only, both sides. */
        for(unsigned row=0;row<8u;row++){
            static const u8 right[8]={3u,2u,1u,1u,2u,2u,1u,3u};
            u8 inner=bits[(32u+row)*AVP_AUTOMAP_ROW_BYTES+4u];
            u8 outer=bits[(32u+row)*AVP_AUTOMAP_ROW_BYTES+5u];
            assert(!!(inner&1u)==!!(right[row]&2u));
            assert(!!(outer&0x80u)==!!(right[row]&1u));
        }
        cell(m,4,4)[2]=0u;
        cell(m,4,4)[0]=(u8)(5u*8u+1u);
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        for(unsigned row=0;row<8u;row++){
            static const u8 left[8]={3u,1u,2u,2u,1u,1u,2u,3u};
            u8 outer=bits[(32u+row)*AVP_AUTOMAP_ROW_BYTES+3u];
            u8 inner=bits[(32u+row)*AVP_AUTOMAP_ROW_BYTES+4u];
            assert(!!(outer&1u)==!!(left[row]&1u));
            assert(!!(inner&0x80u)==!!(left[row]&2u));
        }
        cell(m,4,4)[0]=0u;
        cell(m,4,4)[2]=(u8)(5u*8u+1u);
        /* The original check_right wall script ORs a rotating one-bit
         * stipple into the next cell when its opposite side is not solid. */
        cell(m,4,4)[2]=1u;
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        assert(bits[32u*AVP_AUTOMAP_ROW_BYTES+5u]==0x91u);
        assert(bits[33u*AVP_AUTOMAP_ROW_BYTES+5u]==0xa2u);
        assert(bits[34u*AVP_AUTOMAP_ROW_BYTES+5u]==0xc4u);
        assert(bits[35u*AVP_AUTOMAP_ROW_BYTES+5u]==0x88u);
        /* A solid wall facing another solid wall must not stipple over it. */
        cell(m,5,4)[0]=1u;
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        assert(bits[32u*AVP_AUTOMAP_ROW_BYTES+5u]==0x80u);
        cell(m,4,4)[2]=(u8)(5u*8u+1u);
        cell(m,5,4)[0]=0u;
        avp_gpu_automap_rasterize(s,m,8,8,bits);
        /* The projection is read-only with respect to persistent discovery. */
        assert(s->automap_seen[4u*64u+4u]);
        {AvpGpuAutomapSnapshot saved;
         avp_gpu_automap_save(s,&saved);
         avp_gpu_automap_reset(s);
         avp_gpu_automap_load(s,&saved);
         avp_gpu_automap_rasterize(s,m,8,8,bits);
         assert((bits[32u*AVP_AUTOMAP_ROW_BYTES+5u]&0x80u)!=0u);
        }
    }
    /* Gameplay-tick map strokes must persist even when the authored
       geometry later changes.  Redrawing never adds or removes strokes. */
    {
        AvpGpuAutomapSnapshot snap;
        int mx,my;
        memset(m,0,sizeof(m));
        avp_gpu_automap_reset(s);
        cell(m,3,3)[2]=1u;
        avp_gpu_automap_observe(s,m,8,8,3u<<16,3u<<16,0,9);
        assert(s->automap_bits[24u*AVP_AUTOMAP_ROW_BYTES+4u]&0x80u);
        avp_gpu_automap_save(s,&snap);
        cell(m,3,3)[2]=0u;
        /* A new trace sees the changed maze, but the previous stroke
           remains present in the Jaguar's persistent one-bit map. */
        avp_gpu_automap_observe(s,m,8,8,5u<<16,5u<<16,0,9);
        assert(s->automap_bits[24u*AVP_AUTOMAP_ROW_BYTES+4u]&0x80u);
        avp_gpu_automap_reset(s);
        assert(s->automap_bits[24u*AVP_AUTOMAP_ROW_BYTES+4u]==0u);
        avp_gpu_automap_load(s,&snap);
        assert(s->automap_bits[24u*AVP_AUTOMAP_ROW_BYTES+4u]&0x80u);
        avp_gpu_automap_observe(s,m,8,8,1u<<16,1u<<16,0,10);
        assert(s->automap_level==10u);
        assert(s->automap_bits[24u*AVP_AUTOMAP_ROW_BYTES+4u]==0u);
        /* MAZE5.GAS 16.16 start/inc/step, no host floating rounding. */
        avp_gpu_automap_sample(5u<<16,6u<<16,0,32768,120,128,60,64,&mx,&my);
        assert(mx==40&&my==48);
        avp_gpu_automap_sample(5u<<16,6u<<16,0,32768,120,128,61,64,&mx,&my);
        assert(mx==40&&my==49);
        avp_gpu_automap_sample(5u<<16,6u<<16,0,32768,120,128,60,65,&mx,&my);
        assert(mx==39&&my==48);
        avp_gpu_automap_sample(5u<<16,6u<<16,32768,0,120,128,60,64,&mx,&my);
        assert(mx==40&&my==48);
        avp_gpu_automap_sample(5u<<16,6u<<16,32768,0,120,128,61,64,&mx,&my);
        assert(mx==41&&my==48);
        /* Smooth-camera sine/cosine must retain the original signed 1.15
         * amplitude. Half amplitude magnifies the map by two times. */
        avp_gpu_automap_sample(5u<<16,6u<<16,0,32767,120,128,60,64,&mx,&my);
        assert(mx==40&&my==48);
        avp_gpu_automap_sample(5u<<16,6u<<16,0,32767,120,128,76,64,&mx,&my);
        assert(my==63);
        avp_gpu_automap_sample(5u<<16,6u<<16,0,16384,120,128,76,64,&mx,&my);
        assert(my==56);
    }
    free(s);
    return 0;
}
