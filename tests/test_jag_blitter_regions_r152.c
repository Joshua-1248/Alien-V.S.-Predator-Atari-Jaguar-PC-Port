/* R152 diagnostic-only memory-region segmentation. Uses synthetic data, and
 * checks byte request address bins, never bus-transaction or timing claims. */
#include "jaguar/jag_machine.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRCEN 1u
#define LFU_SRC (0xCu<<21)
#define PS8 (3u<<3)
#define W8 (3u<<11)
#define XADD1 (1u<<16)
static JagMachine m;
static uint8_t cart[0x4000];
static unsigned commands;
static void on_command(JagMachine *machine,const JagBlitMmioEvent *e,void *ctx)
{
    uint64_t reads=0,writes=0;
    (void)ctx;
    assert(machine==&m);
    if(e->addr!=0xF02238u)return;
    commands++;
    for(unsigned i=0;i<JAGM_BLIT_REGION_COUNT;i++){
        assert(e->region_reads_after[i]>=e->region_reads_before[i]);
        assert(e->region_writes_after[i]>=e->region_writes_before[i]);
        reads+=e->region_reads_after[i]-e->region_reads_before[i];
        writes+=e->region_writes_after[i]-e->region_writes_before[i];
    }
    assert(reads==e->reads_after-e->reads_before);
    assert(writes==e->writes_after-e->writes_before);
}
static void w(uint32_t addr,uint32_t value){jagm_write32(&m,addr,value);}
static void copy(uint32_t source,uint32_t dest,unsigned n)
{
    const uint32_t flags=PS8|W8|XADD1;
    w(0xF02200u,dest); w(0xF02204u,flags); w(0xF0220Cu,0u);
    w(0xF02224u,source); w(0xF02228u,flags); w(0xF02230u,0u);
    w(0xF0223Cu,(1u<<16)|n);
    w(0xF02238u,SRCEN|LFU_SRC);
}
static void check(uint64_t rd,uint64_t cart_rd,uint64_t other_rd,uint64_t wr)
{
    assert(m.blit_region_reads[JAGM_BLIT_DRAM]==rd);
    assert(m.blit_region_reads[JAGM_BLIT_CART]==cart_rd);
    assert(m.blit_region_reads[JAGM_BLIT_OTHER]==other_rd);
    assert(m.blit_region_writes[JAGM_BLIT_DRAM]==wr);
    assert(m.blit_region_writes[JAGM_BLIT_CART]==0u);
    assert(m.blit_region_writes[JAGM_BLIT_OTHER]==0u);
    assert(m.blit_read_requests==rd+cart_rd+other_rd);
    assert(m.blit_write_requests==wr);
}
int main(void)
{
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xFE;
    for(unsigned i=0;i<8;i++){m.ram[0x1800u+i]=(uint8_t)(0x80u+i);cart[0x1000u+i]=(uint8_t)(0x40u+i);}
    assert(jagm_init(&m,cart,sizeof cart)==0);
    for(unsigned i=0;i<8;i++)m.ram[0x1800u+i]=(uint8_t)(0x80u+i);
    m.on_blit_mmio=on_command;
    /* Ordinary host/CPU writes are excluded even with a tracing callback. */
    jagm_write8(&m,0x1500u,0x99u);
    jagm_write8(&m,0xF00400u,0x66u);
    check(0,0,0,0);
    copy(0x1800u,0x1600u,4u);
    check(4,0,0,4);
    for(unsigned i=0;i<4;i++)assert(m.ram[0x1600u+i]==(uint8_t)(0x80u+i));
    /* ROM and DRAM have different physical timing; do not combine them in
     * the region ledger even though both produce source byte callbacks. */
    copy(0x801000u,0x1680u,4u);
    check(4,4,0,8);
    for(unsigned i=0;i<4;i++)assert(m.ram[0x1680u+i]==(uint8_t)(0x40u+i));
    /* TOM CLUT source: read-only from this test's perspective. */
    jagm_write16(&m,0xF00400u,0xABCDu);
    copy(0xF00400u,0x1700u,2u);
    check(4,4,2,10);
    assert(m.ram[0x1700u]==0xABu&&m.ram[0x1701u]==0xCDu);
    /* NOGO is not a memory transfer, and the counters remain unchanged. */
    w(0xF02238u,SRCEN|LFU_SRC|0x80u);
    assert(commands==4u);
    check(4,4,2,10);
    puts("RE38 R152 Blitter memory-region request bins PASS");
    return 0;
}
