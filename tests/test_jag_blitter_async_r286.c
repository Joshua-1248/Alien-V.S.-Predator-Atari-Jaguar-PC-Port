/* RE44 R286: opt-in progressive TOM Blitter experiment. ROM-free synthetic
 * bus and cartridge: no assertion about physical Jaguar bus-clock latency. */
#include "jaguar/jag_machine.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CMD 0x38u
#define IDLE 0x805u
#define SRCEN 1u
#define DCOMPEN (1u<<27)
#define SRC_LFU (0xCu<<21)
#define PS8 (3u<<3)
#define W8 (4u<<11)
#define XADD1 (1u<<16)
#define BASE 0x1200u
static uint8_t mem[0x6000];
static uint8_t rb(void *ctx,uint32_t a)
{(void)ctx;assert(a<sizeof mem);return mem[a];}
static void wb(void *ctx,uint32_t a,uint8_t v)
{(void)ctx;assert(a<sizeof mem);mem[a]=v;}
static void w(JagBlitter *b,uint32_t a,uint32_t v)
{assert(jag_blitter_write(b,a,v));}
static uint32_t stat(JagBlitter *b)
{uint32_t v=0xFFFFFFFFu;assert(jag_blitter_read(b,CMD,&v));return v;}
static void setup(JagBlitter *b,uint32_t flags,unsigned n,int dstshift)
{
    JagBlitBus bus={NULL,rb,wb};
    jag_blitter_init(b,bus);
    w(b,0x00,BASE);w(b,0x04,flags);w(b,0x0C,dstshift?1u:0u);
    w(b,0x24,dstshift?BASE:BASE+0x100u);
    w(b,0x28,flags);w(b,0x30,0u);
    w(b,0x3C,(1u<<16)|n);
}
static void progressive_pixel_copy(void)
{
    JagBlitter b;
    memset(mem,0xAA,sizeof mem);
    setup(&b,PS8|W8|XADD1,5,0);
    for(unsigned i=0;i<5u;i++)mem[BASE+0x100u+i]=(uint8_t)(11u+i);
    jag_blitter_set_async(&b,1);
    w(&b,CMD,SRCEN|SRC_LFU);
    assert(b.running&&b.pending_start&&stat(&b)==0u);
    assert(b.blits==0u&&mem[BASE]==0xAAu);
    assert(jag_blitter_advance(&b,1u)==0);
    assert(b.blits==1u&&b.running&&b.pause.active);
    assert(stat(&b)==0u&&mem[BASE]==11u&&mem[BASE+1u]==0xAAu);
    assert(jag_blitter_advance(&b,2u)==0);
    assert(mem[BASE+2u]==13u&&mem[BASE+3u]==0xAAu);
    assert(jag_blitter_advance(&b,10u)==1);
    assert(stat(&b)==IDLE&&!b.running&&!b.pause.active);
    for(unsigned i=0;i<5u;i++)assert(mem[BASE+i]==(uint8_t)(11u+i));
    assert(jag_blitter_advance(&b,10u)==0);
}
static void overlapping_phrase_copy(void)
{
    JagBlitter b;
    uint8_t original[40];
    memset(mem,0,sizeof mem);
    for(unsigned i=0;i<40;i++){mem[BASE+i]=(uint8_t)(0x20u+i);original[i]=mem[BASE+i];}
    setup(&b,PS8|W8,25,1);
    jag_blitter_set_async(&b,1);
    w(&b,CMD,SRCEN|SRC_LFU);
    assert(jag_blitter_advance(&b,1u)==0);
    /* First destination phrase: 7 bytes at X=1, next seven require
     * carrying source phrase 0 into phrase 1 before overlapping writes. */
    for(unsigned i=0;i<7;i++)assert(mem[BASE+1u+i]==original[i]);
    for(unsigned i=0;i<7u;i++)assert(jag_blitter_advance(&b,1u)==(i==2u));
    for(unsigned i=0;i<25u;i++)assert(mem[BASE+1u+i]==original[i]);
    assert(stat(&b)==IDLE);
}
/* Color and Z source phrase snapshots must survive multiple incremental
 * yields across an overlapping 16-bit phrase transfer. */
static void overlapping_source_z(void)
{
    JagBlitter b;
    uint16_t colors[16],zs[16];
    const uint32_t f=(4u<<3)|W8|1u|(1u<<6); /* 16-bpp, pitch=2, Z offset 1 */
    memset(mem,0,sizeof mem);
    for(unsigned i=0;i<16u;i++){
        uint32_t base=BASE+(i/4u)*16u+(i%4u)*2u;
        colors[i]=(uint16_t)(0x1800u+9u*i);
        zs[i]=(uint16_t)(0x7100u+7u*i);
        mem[base]=(uint8_t)(colors[i]>>8);mem[base+1u]=(uint8_t)colors[i];
        mem[base+8u]=(uint8_t)(zs[i]>>8);mem[base+9u]=(uint8_t)zs[i];
    }
    setup(&b,f,9u,1);
    jag_blitter_set_async(&b,1);
    w(&b,CMD,SRCEN|(1u<<1)|(1u<<5)|SRC_LFU);
    for(unsigned i=0;i<3u;i++){
        int done=jag_blitter_advance(&b,1u);
        if(i==2u)assert(done);else assert(!done);
    }
    for(unsigned i=0;i<9u;i++){
        uint32_t base=BASE+((i+1u)/4u)*16u+((i+1u)%4u)*2u;
        assert(((uint16_t)mem[base]<<8|mem[base+1u])==colors[i]);
        assert(((uint16_t)mem[base+8u]<<8|mem[base+9u])==zs[i]);
    }
    assert(stat(&b)==IDLE&&!b.pause.active);
}

static void collision_stop_and_abort(void)
{
    JagBlitter b;
    memset(mem,0xA5,sizeof mem);
    setup(&b,PS8|W8|XADD1,5,0);
    mem[BASE+0x100u]=0x11u;mem[BASE+0x101u]=0;
    mem[BASE+0x102u]=0x22u;mem[BASE+0x103u]=0;
    mem[BASE+0x104u]=0x33u;
    jag_blitter_set_async(&b,1);
    w(&b,0x78,4u);
    w(&b,CMD,SRCEN|SRC_LFU|DCOMPEN);
    assert(jag_blitter_advance(&b,2u)==0);
    assert(b.stopped&&!b.running&&b.pause.active&&stat(&b)==2u);
    assert(mem[BASE]==0x11u&&mem[BASE+2u]==0xA5u);
    w(&b,0x78,5u);
    assert(b.running&&!b.stopped&&stat(&b)==0u);
    assert(jag_blitter_advance(&b,2u)==0);
    assert(b.stopped&&mem[BASE+2u]==0x22u);
    w(&b,0x78,1u);
    assert(jag_blitter_advance(&b,2u)==1);
    assert(stat(&b)==IDLE&&mem[BASE+4u]==0x33u);
    w(&b,0x0C,0u);w(&b,0x30,0u);
    w(&b,CMD,SRCEN|SRC_LFU);
    assert(b.running&&stat(&b)==0u);
    w(&b,0x78,2u); /* abort before first source read or write */
    assert(!b.running&&!b.stopped&&stat(&b)==IDLE);
}
static JagMachine machine;
static uint8_t cart[0x4000];
static void wm(uint32_t addr,uint32_t value){jagm_write32(&machine,addr,value);}
static void machine_irq(void)
{
    uint32_t f=PS8|W8|XADD1;
    cart[0x404]=0u;cart[0x405]=0x80u;cart[0x406]=0x20u;
    cart[0x2000]=0x60u;cart[0x2001]=0xFEu;
    assert(jagm_init(&machine,cart,sizeof cart)==0);
    jagm_set_blitter_async(&machine,1);
    wm(0xF02200u,0x1600u);wm(0xF02204u,f);wm(0xF0220Cu,0u);
    wm(0xF02224u,0x1500u);wm(0xF02228u,f);wm(0xF02230u,0u);
    wm(0xF0223Cu,(1u<<16)|5u);
    for(unsigned i=0;i<5u;i++)machine.ram[0x1500u+i]=(uint8_t)(0x70u+i);
    memset(machine.ram+0x1600u,0xAA,5u);
    wm(0xF02238u,SRCEN|SRC_LFU);
    assert(jagm_read32(&machine,0xF02238u)==0u);
    assert(machine.ram[0x1600u]==0xAAu);
    assert(!(machine.gpu.latches&(1u<<4)));
    jagm_run_frames(&machine,1u);
    assert(jagm_read32(&machine,0xF02238u)==IDLE);
    assert((machine.gpu.latches&(1u<<4))!=0);
    for(unsigned i=0;i<5u;i++)assert(machine.ram[0x1600u+i]==(uint8_t)(0x70u+i));
}
int main(void)
{
    progressive_pixel_copy();
    overlapping_phrase_copy();
    overlapping_source_z();
    collision_stop_and_abort();
    machine_irq();
    puts("RE44 R286 incremental Blitter scheduling/phrase/stop/IRQ PASS");
    return 0;
}
