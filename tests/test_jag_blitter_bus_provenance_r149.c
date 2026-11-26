/* RE38 R149: source-owned synthetic CPU and GPU commands, no retail ROM.
 * Counters are software callback observations, never physical bus timings. */
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
static JagBlitMmioEvent ev[12];
static unsigned observed;
static void capture(JagMachine *machine,const JagBlitMmioEvent *e,void *ctx)
{
    (void)ctx;
    assert(machine==&m&&observed<12u);
    assert(e->reads_after>=e->reads_before);
    assert(e->writes_after>=e->writes_before);
    assert(e->gpu_steps_completed<=m.gpu_steps_completed);
    ev[observed++]=*e;
}
static void w(uint32_t a,uint32_t value){jagm_write32(&m,a,value);}
static void setup(void)
{
    uint32_t flags=PS8|W8|XADD1;
    w(0xF02200u,0x1600u);w(0xF02204u,flags);w(0xF0220Cu,0);
    w(0xF02224u,0x1500u);w(0xF02228u,flags);w(0xF02230u,0);
    w(0xF0223Cu,(1u<<16)|5u);w(0xF02268u,0);w(0xF0226Cu,0);
    for(unsigned i=0;i<5;i++)m.ram[0x1500u+i]=(uint8_t)(i+7u);
    memset(&m.ram[0x1600],0,5);
}
static unsigned gp;
static uint16_t I(unsigned op,unsigned r1,unsigned r2)
{ return (uint16_t)((op<<10)|((r1&31u)<<5)|(r2&31u)); }
static void emit(uint16_t value)
{
    unsigned off=gp-0xF03000u;
    assert(off+2u<=m.gpu.ram_size);
    m.gpu.ram[off]=(uint8_t)(value>>8);m.gpu.ram[off+1u]=(uint8_t)value;
    gp+=2u;
}
static void movei(uint32_t value,unsigned reg)
{ emit(I(38,0,reg));emit((uint16_t)value);emit((uint16_t)(value>>16)); }
int main(void)
{
    uint64_t p0=(uint64_t)2u|((uint64_t)0x7FFu<<3);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xFE;
    assert(jagm_init(&m,cart,sizeof cart)==0);
    setup();m.on_blit_mmio=capture;
    w(0xF02238u,SRCEN|LFU_SRC|0x80u); /* NOGO must not touch bus */
    assert(observed==1u&&ev[0].reads_after==ev[0].reads_before);
    assert(ev[0].writes_after==ev[0].writes_before);
    w(0xF02238u,SRCEN|LFU_SRC);
    assert(observed==2u&&ev[1].reads_after>ev[1].reads_before);
    assert(ev[1].writes_after>ev[1].writes_before);
    assert(ev[1].gpu_steps_completed==0u);
    /* Prime the reference scheduler so its prior master is DSP (2), then
     * issue a GPU Blitter command inside an OP GPU-object wait.  R148
     * attributed that special-path command to the DSP. */
    m.on_blit_mmio=NULL;
    jagm_run_frames(&m,1);
    m.on_blit_mmio=capture;
    w(0xF0220Cu,0);w(0xF02230u,0);
    gp=0xF03000u;
    movei(0xF02238u,1u);movei(SRCEN|LFU_SRC,2u);
    {uint32_t store_pc=gp;
     emit(I(47,1u,2u));
     movei(0xF00026u,3u);emit(I(35,0u,4u));emit(I(46,3u,4u)); /* clear OBF */
     movei(0xF02114u,3u);emit(I(47,3u,4u)); /* stop GPU after OP wait */
     /* Actual TYPE2 object forces the Object Processor to run the GPU. */
     w(0x1000u,(uint32_t)(p0>>32));w(0x1004u,(uint32_t)p0);
     w(0x1008u,0u);w(0x100Cu,4u); /* STOP object */
     m.olp=0x1000u;m.vmode=1u;m.vdb=0u;m.vde=2u;
     /* The hand-assembled instruction stream is high-word-first.  R238
      * correctly made BIG_INSTR=0 fetch the low instruction half first;
      * this synthetic fixture must explicitly select BIG_INSTR=1. */
     m.gpu.bigend|=4u;
     m.gpu.pc=0xF03000u;m.gpu.go=1u;
     jagm_run_frames(&m,1);
     assert(observed==3u);
     assert(ev[2].master==1u); /* GPU, not leftover DSP owner */
     assert(ev[2].issuer_pc==store_pc);
     assert(ev[2].gpu_steps_completed==2u);
     assert(ev[2].gpu_model_ticks_completed>0u);
     assert(ev[2].reads_after>ev[2].reads_before);
     assert(ev[2].writes_after>ev[2].writes_before);
     assert(m.gpu_steps_completed>ev[2].gpu_steps_completed);
     assert(m.gpu.go==0u);
    }
    puts("RE38 R149 GPU/Blitter provenance and model-access regression PASS");
    return 0;
}
