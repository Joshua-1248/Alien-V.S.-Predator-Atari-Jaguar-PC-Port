/* RE24/S15 synthetic-only GPU control observer: no commercial game data.
 * Verifies that 68000 G_PC/G_CTRL writes produce committed read-only events,
 * including provenance, sequencing and halfword latching. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static unsigned seen;
static JagGpuControlEvent events[8];
static unsigned pc;
static uint8_t cart[0x4000];
static void w16(uint16_t v){assert(pc+2<=sizeof cart);cart[pc++]=(uint8_t)(v>>8);cart[pc++]=(uint8_t)v;}
static void w32(uint32_t v){w16((uint16_t)(v>>16));w16((uint16_t)v);}
static void movel(uint32_t value,uint32_t addr){w16(0x23FCu);w32(value);w32(addr);}
static void observe(JagMachine *machine,const JagGpuControlEvent *e,void *ctx)
{
    unsigned *count=(unsigned*)ctx;
    assert(machine==&m);
    assert(e->pc_after==m.gpu.pc && e->go_after==(unsigned)m.gpu.go);
    assert(*count<8);
    events[(*count)++]=*e;
}
int main(void)
{
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    pc=0x2000;
    movel(0xF03220u,0xF02110u); /* 68000 writes GPU PC while stopped */
    movel(0x00000000u,0xF02114u); /* explicit GO=0; avoid arbitrary GPU code */
    w16(0x60FEu);                 /* BRA.S * */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    m.on_gpu_control=observe;m.on_gpu_control_ctx=&seen;
    jagm_run_frames(&m,1);
    assert(seen==2);
    assert(events[0].master==0&&events[1].master==0);
    assert(events[0].addr==0xF02110u&&events[0].value==0xF03220u);
    assert(events[0].pc_after==0xF03220u);
    assert(events[1].addr==0xF02114u&&events[1].value==0);
    assert(events[1].go_before==0 && events[1].go_after==0);
    assert(events[0].pc68>=0x800000u && events[1].pc68>=0x800000u);
    assert(events[0].field==0 && events[1].field==0);
    assert(events[1].cycles>=events[0].cycles);
    /* Read-only observation must be inert: disable callback and write control. */
    m.on_gpu_control=NULL;
    jagm_write32(&m,0xF02110u,0xF03300u);
    assert(seen==2 && m.gpu.pc==0xF03300u);
    /* Halfword-pair write yields one committed callback, not two. */
    m.on_gpu_control=observe;
    jagm_write16(&m,0xF02110u,0x00F0u);
    assert(seen==2);
    jagm_write16(&m,0xF02112u,0x3120u);
    assert(seen==3 && events[2].value==0x00F03120u);
    assert(m.gpu.pc==0x00F03120u);
    /* Prove rising/falling GO edges are visible without executing any GPU
       program, and preserve the actual writer rather than guessing 68000. */
    jagm_write32(&m,0xF02114u,1u);
    assert(seen==4 && events[3].addr==0xF02114u);
    assert(events[3].go_before==0 && events[3].go_after==1);
    if(events[3].master!=0)assert(events[3].pc68==0);
    jagm_write32(&m,0xF02114u,0u);
    assert(seen==5 && events[4].go_before==1 && events[4].go_after==0);
    puts("Sweep 15 GPU control observer synthetic tests passed");
    return 0;
}
