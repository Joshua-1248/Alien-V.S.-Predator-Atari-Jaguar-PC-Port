/* RE38 R151: only Blitter bus callbacks may affect Blitter request counters.
 * This is a synthetic ROM test, not a measurement of Jaguar bus cycles. */
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
static unsigned n;
static uint64_t command_reads,command_writes;
static void observe(JagMachine *machine,const JagBlitMmioEvent *e,void *ctx)
{
    (void)ctx;
    assert(machine==&m);
    if(e->addr!=0xF02238u)return;
    n++;
    command_reads=e->reads_after-e->reads_before;
    command_writes=e->writes_after-e->writes_before;
}
static void w(uint32_t addr,uint32_t value){jagm_write32(&m,addr,value);}
int main(void)
{
    uint32_t flags=PS8|W8|XADD1;
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xFE;
    assert(jagm_init(&m,cart,sizeof cart)==0);
    m.on_blit_mmio=observe;
    /* Ordinary 8-bit external writes are not Blitter source writes. R150
     * incorrectly counts each as a Blitter request solely because the
     * command observer is installed. This must fail against R150. */
    jagm_write8(&m,0x123u,0xAAu);
    jagm_write8(&m,0x124u,0x55u);
    assert(m.blit_read_requests==0u);
    assert(m.blit_write_requests==0u);
    /* Also exercise the fallback byte RMW path through TOM, still unrelated
     * to the Blitter bus interface. */
    jagm_write8(&m,0xF00400u,0xC3u);
    assert(m.blit_write_requests==0u);
    /* True synchronous Blitter writes must still be counted exactly once. */
    w(0xF02200u,0x1600u);w(0xF02204u,flags);w(0xF0220Cu,0u);
    w(0xF02224u,0x1500u);w(0xF02228u,flags);w(0xF02230u,0u);
    w(0xF0223Cu,(1u<<16)|5u);
    for(unsigned i=0;i<5u;i++)m.ram[0x1500u+i]=(uint8_t)(0x10u+i);
    memset(m.ram+0x1600u,0,5u);
    w(0xF02238u,SRCEN|LFU_SRC);
    assert(n==1u);
    assert(command_reads==5u);
    assert(command_writes==5u);
    assert(m.blit_read_requests==5u);
    assert(m.blit_write_requests==5u);
    for(unsigned i=0;i<5u;i++)assert(m.ram[0x1600u+i]==(uint8_t)(0x10u+i));
    /* NOGO performs no source or destination bus request, and CPU bytes
     * still do not contaminate the counters after a completed transfer. */
    w(0xF02238u,SRCEN|LFU_SRC|0x80u);
    assert(n==2u&&command_reads==0u&&command_writes==0u);
    jagm_write8(&m,0x900u,0x44u);
    assert(m.blit_read_requests==5u);
    assert(m.blit_write_requests==5u);
    puts("RE38 R151 Blitter bus-byte request provenance PASS");
    return 0;
}
