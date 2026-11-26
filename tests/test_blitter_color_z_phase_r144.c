/* R144: TOM source color and source Z phrase reads have distinct phases.
 * Observe byte bus callbacks, not physical 64-bit transactions or cycles.
 * Untouched R143 alternates C-pixel/Z-pixel reads (C0 Z0 C1 Z1 ...). */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRC 0x1000u
#define DST 0x3000u
#define SRCEN (1u<<0)
#define SRCENZ (1u<<1)
#define SRCENX (1u<<2)
#define DSTA2 (1u<<11)
#define LFU_S (0xCu<<21)
#define MAX_EVENTS 80u
static uint8_t memory[0x5000];
static uint32_t observed[MAX_EVENTS];
static unsigned reads, writes;
static uint8_t rb(void *ctx,uint32_t addr)
{
    (void)ctx;
    assert(addr<sizeof memory);
    if(addr>=SRC&&addr<SRC+32u){
        assert(reads<MAX_EVENTS);
        observed[reads++]=addr;
    }
    return memory[addr];
}
static void wb(void *ctx,uint32_t addr,uint8_t v)
{
    (void)ctx;
    assert(addr<sizeof memory);
    if(addr>=DST&&addr<DST+64u){
        assert(reads>0u);
        writes++;
    }
    memory[addr]=v;
}
static void W(JagBlitter *b,uint32_t reg,uint32_t v)
{ assert(jag_blitter_write(b,reg,v)); }
static void expected_read(uint32_t addr,unsigned index)
{
    if(observed[index]!=addr){
        fprintf(stderr,"source bus order mismatch at %u: got 0x%X expected 0x%X\n",
                index,observed[index],addr);
    }
    assert(observed[index]==addr);
}
static void setup(JagBlitter *b,uint32_t a1_flags,uint32_t a2_flags,
                  uint32_t count,int reversed,uint32_t command)
{
    JagBlitBus bus={NULL,rb,wb};
    uint32_t src_flags=reversed?a1_flags:a2_flags;
    uint32_t dst_flags=reversed?a2_flags:a1_flags;
    (void)src_flags;(void)dst_flags;
    jag_blitter_init(b,bus);
    W(b,0x00,reversed?SRC:DST);
    W(b,0x04,a1_flags);
    W(b,0x0C,0u);
    W(b,0x24,reversed?DST:SRC);
    W(b,0x28,a2_flags);
    W(b,0x2C,0x0FFF0FFFu);
    W(b,0x30,0u);
    W(b,0x3C,(1u<<16)|count);
    W(b,0x38,command|(reversed?DSTA2:0u));
    assert(b->blits==1u&&!b->stopped);
}
static void run_cached(int reversed,int extra,int with_z)
{
    JagBlitter b;
    const uint32_t flags=(4u<<3)|(4u<<11)|1u|(1u<<6);
    memset(memory,0,sizeof memory);reads=0u;writes=0u;
    for(unsigned i=0;i<32;i++)memory[SRC+i]=(uint8_t)(i+1u);
    setup(&b,flags,flags,1u,reversed,SRCEN|LFU_S|
          (extra?SRCENX:0u)|(with_z?SRCENZ:0u));
    /* Each source phrase is 8 color bytes followed by 8 Z bytes. */
    unsigned index=0u;
    for(unsigned phrase=0;phrase<(extra?2u:1u);phrase++){
        for(unsigned byte=0;byte<8u;byte++)
            expected_read(SRC+phrase*16u+byte,index++);
        if(with_z)
            for(unsigned byte=0;byte<8u;byte++)
                expected_read(SRC+phrase*16u+8u+byte,index++);
    }
    assert(reads==index);
    assert(writes==2u);
    assert(memory[DST]==memory[SRC]&&memory[DST+1u]==memory[SRC+1u]);
}
static void run_masked_fallback(void)
{
    JagBlitter b;
    const uint32_t flags=(4u<<3)|(4u<<11)|1u|(1u<<6);
    memset(memory,0,sizeof memory);reads=0u;writes=0u;
    for(unsigned i=0;i<16u;i++)memory[SRC+i]=(uint8_t)(i+1u);
    /* A2 masked source disables cross-phrase cache.  Three 16-bit pixels
     * must still be fetched as C0 C1 C2, then Z0 Z1 Z2. */
    setup(&b,flags,flags|(1u<<15),3u,0,SRCEN|SRCENZ|LFU_S);
    unsigned index=0u;
    for(unsigned i=0;i<6u;i++)expected_read(SRC+i,index++);
    for(unsigned i=0;i<6u;i++)expected_read(SRC+8u+i,index++);
    assert(reads==index);
    assert(writes==6u);
    for(unsigned i=0;i<6u;i++)assert(memory[DST+i]==memory[SRC+i]);
}
int main(void)
{
    for(int reversed=0;reversed<2;reversed++)
        for(int extra=0;extra<2;extra++)
            for(int with_z=0;with_z<2;with_z++)
                run_cached(reversed,extra,with_z);
    run_masked_fallback();
    puts("RE38 R144 source color-before-Z phrase read ordering passed (9 cases)");
    return 0;
}
