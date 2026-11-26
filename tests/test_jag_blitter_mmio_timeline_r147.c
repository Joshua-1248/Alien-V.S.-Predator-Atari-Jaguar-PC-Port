/* RE38 R147: passive observation of command/stop/IRQ lifecycle.
 * No commercial ROM and no guesses about actual Jaguar bus-cycle timing. */
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
#define DCOMPEN (1u<<27)
#define PS8 (3u<<3)
#define W8 (3u<<11)
#define XADD1 (1u<<16)
#define IRQ 0x10u
static JagMachine machine;
static uint8_t cart[0x4000];
static JagBlitMmioEvent events[32];
static unsigned observed;
static void capture(JagMachine *m,const JagBlitMmioEvent *e,void *ctx)
{
    (void)ctx;
    assert(m==&machine);
    assert(observed<32u);
    events[observed++]=*e;
    assert(e->cycles_after==e->cycles_before); /* synchronous MODEL, not silicon */
    assert(e->field==0u);
    assert(e->master==0u); /* direct synthetic 68000-view MMIO */
    assert(e->blits_after>=e->blits_before);
    assert(e->pixels_written_after>=e->pixels_written_before);
    assert(e->estimated_ticks_after>=e->estimated_ticks_before);
}
static void w(uint32_t a,uint32_t v){jagm_write32(&machine,a,v);}
static void setup(void)
{
    uint32_t flags=PS8|W8|XADD1;
    w(0xF02200u,0x1600u);w(0xF02204u,flags);w(0xF0220Cu,0);
    w(0xF02224u,0x1500u);w(0xF02228u,flags);w(0xF02230u,0);
    w(0xF0223Cu,(1u<<16)|5u);w(0xF02268u,0);w(0xF0226Cu,0);
    machine.ram[0x1500]=0x11;
    machine.ram[0x1501]=0;
    machine.ram[0x1502]=0x22;
    machine.ram[0x1503]=0;
    machine.ram[0x1504]=0x33;
    memset(&machine.ram[0x1600],0xA5,5);
}
int main(void)
{
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xFE;
    assert(jagm_init(&machine,cart,sizeof cart)==0);
    setup();
    machine.on_blit_mmio=capture;
    w(0xF02238u,SRCEN|LFU_SRC|0x80u); /* NOGO */
    assert(observed==1u&&events[0].addr==0xF02238u);
    assert(!events[0].stopped_before&&!events[0].stopped_after);
    assert(events[0].blits_before==0u&&events[0].blits_after==0u);
    assert(!events[0].gpu_irq_after);
    w(0xF02278u,4u);  /* STOPEN */
    assert(observed==2u&&events[1].addr==0xF02278u);
    w(0xF02238u,SRCEN|LFU_SRC|DCOMPEN);
    assert(observed==3u&&events[2].blits_after==1u);
    assert(!events[2].stopped_before&&events[2].stopped_after);
    assert(!events[2].gpu_irq_after);
    assert(events[2].pixels_written_after==1u);
    w(0xF02238u,SRCEN|LFU_SRC); /* ignored during STOPPED */
    assert(observed==4u&&events[3].stopped_before&&events[3].stopped_after);
    assert(events[3].blits_before==events[3].blits_after);
    w(0xF02278u,5u); /* stop again */
    assert(observed==5u&&events[4].stopped_before&&events[4].stopped_after);
    assert(!events[4].gpu_irq_after);
    w(0xF02278u,1u); /* complete */
    assert(observed==6u&&events[5].stopped_before&&!events[5].stopped_after);
    assert(!events[5].gpu_irq_before&&events[5].gpu_irq_after);
    assert(events[5].blits_after==1u);
    assert(events[5].pixels_written_after==3u);
    w(0xF02100u,1u<<13); /* clear GPU Blitter latch */
    assert(!(machine.gpu.latches&IRQ));
    w(0xF0220Cu,0);w(0xF02230u,0);
    w(0xF02238u,SRCEN|LFU_SRC);
    assert(observed==7u&&events[6].blits_after==2u);
    assert(!events[6].gpu_irq_before&&events[6].gpu_irq_after);
    w(0xF02100u,1u<<13);
    w(0xF0220Cu,0);w(0xF02230u,0);
    w(0xF02278u,4u);
    w(0xF02238u,SRCEN|LFU_SRC|DCOMPEN);
    assert(observed==9u&&events[8].stopped_after);
    w(0xF02278u,2u); /* abort no IRQ */
    assert(observed==10u&&events[9].stopped_before&&!events[9].stopped_after);
    assert(!events[9].gpu_irq_before&&!events[9].gpu_irq_after);

    /* Disabled observer must not be called or affect machine state. */
    machine.on_blit_mmio=NULL;
    w(0xF02238u,SRCEN|LFU_SRC|0x80u);
    assert(observed==10u);
    puts("RE38 R147 Jaguar Blitter MMIO lifecycle timeline PASS (10 events)");
    return 0;
}
