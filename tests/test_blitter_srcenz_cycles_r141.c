/* RE38 R141: SRCENZ requests a source-Z bus read during the inner loop even
 * when no Z comparison or Z write consumes it. The test observes byte reads,
 * not timing or physical Jaguar bus arbitration. Synthetic memory only. */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRC 0x1000u
#define DST 0x2000u
#define SRCEN (1u<<0)
#define SRCENZ (1u<<1)
#define GOURZ (1u<<13)
#define DSTA2 (1u<<11)
#define LFUS (0xCu<<21)
#define PS16 (4u<<3)
#define WIDTH8 (3u<<11)
#define ZOFF1 (1u<<6)
#define PITCH2 1u
#define XADDPIX (1u<<16)
static uint8_t mem[0x4000];
static unsigned source_color_reads,source_z_reads,first_dest_write_z_reads;
static unsigned dest_writes;
static uint8_t read_byte(void *ctx,uint32_t addr)
{
    (void)ctx;
    assert(addr<sizeof mem);
    if(addr>=SRC&&addr<SRC+8u) source_color_reads++;
    if(addr>=SRC+8u&&addr<SRC+16u) source_z_reads++;
    return mem[addr];
}
static void write_byte(void *ctx,uint32_t addr,uint8_t value)
{
    (void)ctx;
    assert(addr<sizeof mem);
    if(addr>=DST&&addr<DST+8u){
        if(!dest_writes)first_dest_write_z_reads=source_z_reads;
        dest_writes++;
    }
    mem[addr]=value;
}
static void W(JagBlitter *b,uint32_t offset,uint32_t value)
{
    assert(jag_blitter_write(b,offset,value));
}
static void check(int reversed,int phrase,int with_source,int with_z,int compute_z)
{
    JagBlitBus bus={NULL,read_byte,write_byte};
    JagBlitter b;
    uint32_t flags=PS16|WIDTH8|ZOFF1|PITCH2|(phrase?0u:XADDPIX);
    uint32_t cmd=(with_source?SRCEN:0u)|(with_z?SRCENZ:0u)|
                 (compute_z?GOURZ:0u)|(reversed?DSTA2:0u)|LFUS;
    memset(mem,0,sizeof mem);
    source_color_reads=0;source_z_reads=0;
    first_dest_write_z_reads=0;dest_writes=0;
    for(unsigned i=0;i<8u;i++){
        mem[SRC+i]=(uint8_t)(0x20u+i);
        mem[SRC+8u+i]=(uint8_t)(0x60u+i);
        mem[DST+i]=0xA5u;
    }
    jag_blitter_init(&b,bus);
    W(&b,0x00,reversed?SRC:DST); W(&b,0x04,flags); W(&b,0x0C,0u);
    W(&b,0x24,reversed?DST:SRC); W(&b,0x28,flags); W(&b,0x30,0u);
    W(&b,0x3C,(1u<<16)|4u);
    W(&b,0x38,cmd);
    assert(dest_writes==8u);
    assert(source_color_reads==(with_source?8u:0u));
    /* SRCENZ has no effect without SRCEN, and does not depend on a Z write,
     * comparator or selection of the computed-Z data path. */
    assert(source_z_reads==(with_source&&with_z?8u:0u));
    if(with_source&&with_z)
        assert(first_dest_write_z_reads==(phrase?8u:2u));
    else
        assert(first_dest_write_z_reads==0u);
    if(with_source)for(unsigned i=0;i<8u;i++)
        assert(mem[DST+i]==(uint8_t)(0x20u+i));
    assert(!b.stopped&&b.blits==1u);
}
int main(void)
{
    unsigned cases=0;
    for(int reverse=0;reverse<=1;reverse++)
        for(int phrase=0;phrase<=1;phrase++){
            check(reverse,phrase,1,1,0);cases++;
            check(reverse,phrase,1,1,1);cases++;
            check(reverse,phrase,1,0,0);cases++;
            check(reverse,phrase,0,1,0);cases++;
        }
    printf("RE38 R141 SRCENZ inner-loop bus-read coverage passed (%u cases)\n",cases);
    return 0;
}
