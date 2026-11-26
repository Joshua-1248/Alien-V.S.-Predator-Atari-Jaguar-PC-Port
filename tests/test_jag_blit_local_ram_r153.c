/* RE38 R153: true GPU/DSP local byte transfers, canonical and write alias.
 * Read semantics of the GPU 0xF0B000 write alias must NOT be changed. */
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
static uint8_t rom[0x4000];
static unsigned cmds;
static uint64_t counted_reads,counted_writes;
static void on_command(JagMachine *machine,const JagBlitMmioEvent *e,void *ctx)
{
    (void)ctx;
    assert(machine==&m);
    if(e->addr!=0xF02238u)return;
    cmds++;
    assert(e->reads_after>=e->reads_before);
    assert(e->writes_after>=e->writes_before);
    counted_reads+=e->reads_after-e->reads_before;
    counted_writes+=e->writes_after-e->writes_before;
}
static void w(uint32_t a,uint32_t v){jagm_write32(&m,a,v);}
static void copy(uint32_t source,uint32_t target,unsigned n)
{
    uint32_t flags=PS8|W8|XADD1;
    w(0xF02200u,target);w(0xF02204u,flags);w(0xF0220Cu,0u);
    w(0xF02224u,source);w(0xF02228u,flags);w(0xF02230u,0u);
    w(0xF0223Cu,(1u<<16)|n);
    w(0xF02238u,SRCEN|LFU_SRC);
}
static void same_bytes(const uint8_t *a,const uint8_t *b,unsigned n)
{
    for(unsigned i=0;i<n;i++)assert(a[i]==b[i]);
}
int main(void)
{
    uint8_t expected[32];
    rom[0x404]=0;rom[0x405]=0x80;rom[0x406]=0x20;
    rom[0x2000]=0x60;rom[0x2001]=0xFE;
    assert(jagm_init(&m,rom,sizeof rom)==0);
    m.on_blit_mmio=on_command;
    for(unsigned i=0;i<32u;i++)expected[i]=(uint8_t)(0x35u+i);
    memcpy(m.ram+0x2000u,expected,32u);
    /* Normal GPU RAM write: compare all four byte lanes of 32-bit words. */
    copy(0x2000u,0xF03060u,32u);
    same_bytes(m.gpu.ram+0x60u,expected,32u);
    /* GPU write alias must address exactly the same local RAM bytes. */
    copy(0x2000u,0xF0B0C0u,32u);
    same_bytes(m.gpu.ram+0xC0u,expected,32u);
    /* Normal GPU RAM reads must continue to use the canonical local window. */
    copy(0xF03060u,0x2200u,32u);
    same_bytes(m.ram+0x2200u,expected,32u);
    /* DSP RAM similarly requires no RMW of the 32-bit local register. */
    copy(0x2000u,0xF1B100u,32u);
    same_bytes(m.dsp.ram+0x100u,expected,32u);
    copy(0xF1B100u,0x2300u,32u);
    same_bytes(m.ram+0x2300u,expected,32u);
    /* Test the last byte in local RAM: no OOB near the 4K/8K boundary. */
    copy(0x2000u,0xF03FE0u,32u);
    assert(m.gpu.ram[4095u]==expected[31]);
    copy(0x2000u,0xF1CFE0u,32u);
    assert(m.dsp.ram[8191u]==expected[31]);
    /* Original byte reads from the write alias must not be silently remapped
     * to GPU RAM; they retain R152 TOM/unhandled read behavior. */
    {uint8_t old=jagm_read8(&m,0xF0B0C0u);
     copy(0xF0B0C0u,0x2400u,1u);
     assert(m.ram[0x2400u]==old);}
    assert(cmds==8u);
    assert(counted_reads==32u*7u+1u);
    assert(counted_writes==counted_reads);
    assert(m.blit_read_requests==counted_reads);
    assert(m.blit_write_requests==counted_writes);
    puts("RE38 R153 GPU RAM byte fast-path and DSP parity PASS");
    return 0;
}
