/* Independent synthetic checks based on Atari JDEV0001 and JDEV0002.
 * The errata describe hazards, not deterministic hardware corruption:
 * these tests verify warnings and non-mutating observation only. */
#include "jaguar/blitter_trm.h"
#include "jaguar/jrisc.h"
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char ram[65536];
static uint8_t r8(void *p,uint32_t a){(void)p;return ram[a&65535u];}
static void w8(void *p,uint32_t a,uint8_t v){(void)p;ram[a&65535u]=v;}
static uint32_t rd(void *p,uint32_t a,int w){(void)p;uint32_t v=0;for(int i=0;i<w;i++)v=(v<<8)|r8(0,a+(uint32_t)i);return v;}
static void wr(void *p,uint32_t a,uint32_t v,int w){(void)p;for(int i=w;i-->0;){w8(0,a+(uint32_t)i,(uint8_t)v);v>>=8;}}
static uint16_t ins(unsigned op,unsigned a,unsigned b){return (uint16_t)((op<<10)|(a<<5)|b);}
static void emit(JRisc *j,unsigned n,uint16_t v){j->ram[n*2]=(uint8_t)(v>>8);j->ram[n*2+1]=(uint8_t)v;}
static JRisc J;
static void start(int kind){JRiscBus b={0,rd,wr};jrisc_init(&J,kind,b);J.check_hazards=1;J.go=1;}

static void blitter(void)
{
    JagBlitter b;JagBlitBus bb={0,r8,w8};
    jag_blitter_init(&b,bb);
    assert(jag_blitter_errata_risks(&b)==0);
    b.a1_flags=4u<<3; b.a1_clip=3u; /* 16bpp, x3 not phrase aligned (4) */
    assert(jag_blitter_errata_risks(&b)==JB_ERRATA_PHANTOM_CLIP);
    b.cmd=1u<<11; /* DSTA2 does not suppress A1 phantom-clip problem */
    assert(jag_blitter_errata_risks(&b)==JB_ERRATA_PHANTOM_CLIP);
    b.cmd=1u<<6;  /* neither does explicitly requesting clipping */
    assert(jag_blitter_errata_risks(&b)==JB_ERRATA_PHANTOM_CLIP);
    b.cmd=0u;
    b.a1_clip=4u;
    assert(jag_blitter_errata_risks(&b)==0);
    b.a1_clip=0u;
    b.a1_flags=1u<<3;b.a1_pixel=1u;
    assert(jag_blitter_errata_risks(&b)==JB_ERRATA_2BPP_ALIGN);
    b.a1_pixel=0u;b.a2_flags=1u<<3;b.a2_pixel=32u;
    assert(jag_blitter_errata_risks(&b)==0u);
    b.cmd=(1u<<13)|(1u<<30); /* GOURZ on its own is not a depth compare/write */
    assert(jag_blitter_errata_risks(&b)==0u);
    b.cmd|=1u<<5;
    assert(jag_blitter_errata_risks(&b)==JB_ERRATA_Z_COLOUR);
    b.cmd=(1u<<29);
    assert(jag_blitter_errata_risks(&b)==JB_ERRATA_PRIORITY);
    /* Concurrent risk configurations accumulate; observer does not modify blit pixels. */
    b.a1_flags=4u<<3;b.a2_flags=4u<<3;b.a1_clip=3u;
    b.cmd=(1u<<29)|(1u<<16);b.count=(1u<<16)|1u;b.a1_base=0x1000u;
    assert(jag_blitter_write(&b,0x38u,b.cmd));
    assert(b.errata_risks==(JB_ERRATA_PHANTOM_CLIP|JB_ERRATA_PRIORITY));
    assert(b.errata_counts[0]==1u&&b.errata_counts[3]==1u);
    assert(ram[0x1000]==0); /* empty PATD, deliberate zero data */
}
static void risc(void)
{
    start(JRISC_GPU);
    assert(jrisc_local_write(&J,J.base+0x100u,JRISC_FLAG_DMAEN));
    assert(J.errata_risks==JRISC_ERRATA_DMAEN);
    /* No external-branch warning for a JR executed in local GPU RAM. */
    start(JRISC_GPU);emit(&J,0,ins(53,1,0));emit(&J,1,ins(57,0,0));
    assert(jrisc_step(&J)>0);assert(!(J.errata_risks&JRISC_ERRATA_EXT_BRANCH));
    start(JRISC_GPU);wr(0,0x200u,ins(53,1,0),2);J.pc=0x200u;
    assert(jrisc_step(&J)>0);assert(J.errata_risks&JRISC_ERRATA_EXT_BRANCH);
    /* External load after LOADP invalidates the old GPU HIDATA assumption. */
    start(JRISC_GPU);J.reg[0][1]=0x400u;J.reg[0][2]=0x480u;
    emit(&J,0,ins(42,1,3));emit(&J,1,ins(41,2,4));
    wr(0,0x400u,0xCAFE1234u,4);wr(0,0x404u,0xABCD1234u,4);
    assert(jrisc_step(&J)>0);assert(!(J.errata_risks&JRISC_ERRATA_HIDATA_CHANGED));
    assert(jrisc_step(&J)>0);assert(J.errata_risks&JRISC_ERRATA_HIDATA_CHANGED);
    assert(J.hidata==0xCAFE1234u); /* deliberately unchanged; corrupt value unknown */
    /* An external DSP store with no previous LOAD is a JDEV0002 risk. */
    start(JRISC_DSP);J.reg[0][1]=0x600u;J.reg[0][2]=0xAABBCCDDu;
    emit(&J,0,ins(47,1,2));assert(jrisc_step(&J)>0);
    assert(J.errata_risks&JRISC_ERRATA_DSP_STORE);
    start(JRISC_DSP);J.reg[0][1]=0x600u;J.reg[0][2]=0xAABBCCDDu;
    emit(&J,0,ins(41,1,3));emit(&J,1,ins(47,1,2));
    assert(jrisc_step(&J)>0);assert(jrisc_step(&J)>0);
    assert(!(J.errata_risks&JRISC_ERRATA_DSP_STORE));
    /* JDEV0002 flags write pipeline: next two ops should be NOP. */
    start(JRISC_GPU);J.reg[0][1]=J.base+0x100u;J.reg[0][2]=0;
    emit(&J,0,ins(47,1,2));emit(&J,1,ins(57,0,0));emit(&J,2,ins(57,0,0));emit(&J,3,ins(35,1,3));
    for(int i=0;i<4;i++)assert(jrisc_step(&J)>0);
    assert(!(J.errata_risks&JRISC_ERRATA_FLAGS_PIPELINE));
    start(JRISC_GPU);J.reg[0][1]=J.base+0x100u;J.reg[0][2]=0;
    emit(&J,0,ins(47,1,2));emit(&J,1,ins(57,0,0));emit(&J,2,ins(35,1,3));
    for(int i=0;i<3;i++)assert(jrisc_step(&J)>0);
    assert(J.errata_risks&JRISC_ERRATA_FLAGS_PIPELINE);
}
static void op(void)
{
    AvpJaguarOpBitmapHeader h;AvpJaguarOpScaledVerticalStep step;
    memset(&h,0,sizeof h);h.type=1;h.vscale=0xE0;
    assert(avp_jaguar_op_scaled_vertical_step(&h,&step));
    h.vscale=0xE1;step.next_remainder=0xA5u;
    assert(!avp_jaguar_op_scaled_vertical_step(&h,&step));
    assert(step.next_remainder==0xA5u); /* transactional reject */
}
int main(void)
{
    blitter();risc();op();
    puts("Atari JDEV0001/JDEV0002 multi-subsystem errata diagnostics passed");
    return 0;
}
