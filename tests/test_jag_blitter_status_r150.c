/* RE38 R150: Jaguar B_CMD status semantics and noninvasive poll observer.
 * Original Jaguar TRM Rev.8, pp. 76-77: bits 0,2,11 are idle indicators.
 * Synthetic cartridge only; no measured hardware wait timings are claimed. */
#include "jaguar/jag_machine.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define B_CMD 0xF02238u
#define PS8 (3u << 3)
#define W8 (3u << 11)
#define XADD1 (1u << 16)
#define LFU_SRC (0xCu << 21)
#define DCOMPEN (1u << 27)
#define STOPPED 2u
#define IDLE (1u | 4u | 0x800u)
static JagMachine m;
static uint8_t cart[0x4000];
static JagBlitStatusReadEvent events[20];
static unsigned observed;
static void capture(JagMachine *p, const JagBlitStatusReadEvent *e,void *ctx)
{
    (void)ctx;
    assert(p==&m && observed<20u);
    assert(e->status== (m.blit.stopped?STOPPED:IDLE));
    events[observed++]=*e;
}
static void w(uint32_t a,uint32_t v){jagm_write32(&m,a,v);}
static unsigned gp;
static uint16_t I(unsigned op,unsigned src,unsigned dst)
{ return (uint16_t)((op<<10)|((src&31u)<<5)|(dst&31u)); }
static void emit(uint16_t x)
{
    unsigned off=gp-0xF03000u;
    assert(off+2u<=m.gpu.ram_size);
    m.gpu.ram[off]=(uint8_t)(x>>8);
    m.gpu.ram[off+1u]=(uint8_t)x;
    gp+=2u;
}
static void movei(uint32_t value,unsigned reg)
{emit(I(38u,0u,reg));emit((uint16_t)value);emit((uint16_t)(value>>16));}
int main(void)
{
    uint32_t flags=PS8|W8|XADD1;
    cart[0x404]=0u; cart[0x405]=0x80u; cart[0x406]=0x20u;
    cart[0x2000]=0x60u; cart[0x2001]=0xFEu;
    assert(jagm_init(&m,cart,sizeof cart)==0);
    /* Untouched R149 returns 1 rather than 0x805 here: regression must fail. */
    assert(jagm_read32(&m,B_CMD)==IDLE);
    assert(jagm_read16(&m,B_CMD)==(uint16_t)IDLE);
    assert(jagm_read16(&m,B_CMD+2u)==0u);
    m.on_blit_status_read=capture;
    assert(jagm_read32(&m,B_CMD)==IDLE);
    assert(observed==1u && events[0].status==IDLE);
    assert(events[0].stopped==0u);
    assert(jagm_read16(&m,B_CMD+2u)==0u);
    assert(observed==2u); /* one read per 16-bit half access */
    assert(jagm_read16(&m,B_CMD)==(uint16_t)IDLE);
    assert(observed==3u);
    /* B_CMD write must not trigger the status-read callback. */
    w(0xF02200u,0x1600u); w(0xF02204u,flags);w(0xF0220Cu,0u);
    w(0xF02224u,0x1500u); w(0xF02228u,flags);w(0xF02230u,0u);
    w(0xF0223Cu,(1u<<16)|5u);w(0xF02268u,0u);w(0xF0226Cu,0u);
    m.ram[0x1500]=0x11u;m.ram[0x1502]=0x22u;m.ram[0x1504]=0x33u;
    memset(&m.ram[0x1600],0xA5,5u);
    w(0xF02278u,4u); /* STOPEN */
    w(B_CMD,1u|LFU_SRC|DCOMPEN);
    assert(observed==3u);
    assert(m.blit.stopped);
    assert((jagm_read32(&m,B_CMD)&(IDLE|STOPPED))==STOPPED);
    assert(observed==4u && events[3].stopped==1u);
    assert((m.gpu.latches&(1u<<4))==0u);
    w(0xF02278u,1u); /* resume without STOPEN */
    assert(!m.blit.stopped);
    assert(jagm_read32(&m,B_CMD)==IDLE);
    assert(observed==5u && events[4].gpu_irq_latched==1u);
    /* GPU-side B_CMD LOAD records its actual LOAD instruction PC and the
     * result, with the model's completed-step marker from before the LOAD. */
    m.on_blit_status_read=capture;
    gp=0xF03000u;
    movei(B_CMD,1u);
    {uint32_t loadpc=gp;
     emit(I(41u,1u,2u));    /* LOAD (r1),r2 */
     movei(0xF02114u,3u); /* GPU CTRL */
     emit(I(35u,0u,4u)); /* MOVEQ #0,r4 */
     emit(I(47u,3u,4u)); /* STORE r4,(r3) => stop GPU */
     /* This hand-emitted GPU stream uses high-word-first instructions. */
     m.gpu.bigend|=4u; /* BIG_INSTR, not the BIG_IO word lane. */
     m.gpu.pc=0xF03000u; m.gpu.go=1u;
     jagm_run_frames(&m,1u);
     assert(!m.gpu.go);
     assert(jrisc_regs(&m.gpu)[2u]==IDLE);
     assert(observed==6u);
     assert(events[5].master==1u);
     assert(events[5].issuer_pc==loadpc);
     assert(events[5].gpu_steps_completed==1u);
     assert(events[5].status==IDLE);
    }
    puts("RE38 R150 B_CMD idle diagnostics and GPU status-read provenance PASS");
    return 0;
}
