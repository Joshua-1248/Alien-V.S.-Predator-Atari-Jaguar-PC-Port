/* R249: GPUOBJ suspended without an OBF write must resume at the following
 * phrase when OBF is later written.  No claims of cycle-accurate resume. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t rom[0x3000];
static void put64(uint32_t a,uint64_t v)
{
    jagm_write32(&m,a,(uint32_t)(v>>32));
    jagm_write32(&m,a+4u,(uint32_t)v);
}
static void test_deferred(unsigned flag)
{
    const uint64_t gpu=UINT64_C(0x123456789abcde02);
    uint64_t branch=3u|((uint64_t)3u<<14)|((uint64_t)(0x1018u>>3)<<24);
    memset(rom,0,sizeof rom);
    rom[0x404u]=0u;rom[0x405u]=0x80u;rom[0x406u]=0x20u;
    rom[0x2000u]=0x60u;rom[0x2001u]=0xfeu;
    assert(jagm_init(&m,rom,sizeof rom)==0);
    put64(0x1000u,gpu);   /* First object interrupts idle GPU. */
    put64(0x1008u,branch);/* Select a STOP by OBF bit 0. */
    put64(0x1010u,4u);   /* No IRQ STOP */
    put64(0x1018u,12u);  /* IRQ STOP */
    jagm_write16(&m,0xf00020u,0x1000u);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf0002eu,31u);
    jagm_write16(&m,0xf0003eu,31u);
    jagm_write16(&m,0xf00046u,28u);
    jagm_write16(&m,0xf00048u,30u);
    jagm_write16(&m,0xf00038u,4u);
    jagm_write16(&m,0xf0003au,127u);
    jagm_write16(&m,0xf0003cu,20u);
    jagm_write16(&m,0xf00028u,7u);
    m.gpu.go=0u;
    jagm_run_frames(&m,1u);
    assert(m.op_lines==1u&&m.op_gpu_objects==1u&&m.op_objects==1u);
    assert(m.op_waiting_for_obf==1u);
    assert(m.ob==gpu&&(m.int1_pending&4u)==0u);
    /* Waiting persists across frame boundaries without re-running GPUOBJ. */
    jagm_run_frames(&m,1u);
    assert(m.op_lines==1u&&m.op_objects==1u);
    jagm_write16(&m,0xf00026u,(uint16_t)flag);
    if(m.op_objects!=3u)
        fprintf(stderr,"R248 lost deferred cursor: objects=%llu after OBF=%x\n",
                (unsigned long long)m.op_objects,flag);
    assert(m.op_objects==3u&&m.op_gpu_objects==1u);
    assert(m.op_waiting_for_obf==0u);
    assert(m.obf==(uint16_t)flag);
    assert((m.int1_pending&4u)!=0u ? (flag&1u)!=0u : (flag&1u)==0u);
    /* A second unrelated OBF write must not replay a completed list. */
    jagm_write16(&m,0xf00026u,0x8000u);
    assert(m.op_objects==3u);
}
/* A deferred CC=4 BRANCH must test the HC10 bit latched at the original
 * OP start, even when OBF is written in another field. */
static void test_deferred_hc10(void)
{
    memset(rom,0,sizeof rom);
    rom[0x404u]=0u;rom[0x405u]=0x80u;rom[0x406u]=0x20u;
    rom[0x2000u]=0x60u;rom[0x2001u]=0xfeu;
    assert(jagm_init(&m,rom,sizeof rom)==0);
    put64(0x1000u,2u);
    put64(0x1008u,3u|((uint64_t)4u<<14)|((uint64_t)(0x1018u>>3)<<24));
    put64(0x1010u,4u);
    put64(0x1018u,12u);
    jagm_write16(&m,0xf00020u,0x1000u);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf0002eu,31u);
    jagm_write16(&m,0xf0003eu,31u);
    jagm_write16(&m,0xf00046u,28u);
    jagm_write16(&m,0xf00048u,30u);
    jagm_write16(&m,0xf00038u,0x404u); /* HDB1 in second half */
    jagm_write16(&m,0xf0003au,0x47fu); /* HDB2 out of range */
    jagm_write16(&m,0xf0003cu,0x414u);
    jagm_write16(&m,0xf00028u,7u);
    m.gpu.go=0u;
    jagm_run_frames(&m,1u);
    assert(m.op_objects==1u&&m.op_waiting_for_obf==1u);
    /* m.hc_half now starts a new field at HC10=0. */
    assert((m.hc_half&0x400u)==0u);
    jagm_write16(&m,0xf00026u,0u);
    assert(m.op_objects==3u);
    assert((m.int1_pending&4u)!=0u);
}

/* A second GPUOBJ in a deferred continuation must start a fresh wait,
 * keeping its own next-phrase address until the second release. */
static void test_two_delayed_gpu_objects(void)
{
    memset(rom,0,sizeof rom);
    rom[0x404u]=0u;rom[0x405u]=0x80u;rom[0x406u]=0x20u;
    rom[0x2000u]=0x60u;rom[0x2001u]=0xfeu;
    assert(jagm_init(&m,rom,sizeof rom)==0);
    put64(0x1000u,UINT64_C(0x0102030405060702));
    put64(0x1008u,UINT64_C(0x08090a0b0c0d0e02));
    put64(0x1010u,12u);
    jagm_write16(&m,0xf00020u,0x1000u);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf0002eu,31u);
    jagm_write16(&m,0xf0003eu,31u);
    jagm_write16(&m,0xf00046u,28u);
    jagm_write16(&m,0xf00048u,30u);
    jagm_write16(&m,0xf00038u,4u);
    jagm_write16(&m,0xf0003au,127u);
    jagm_write16(&m,0xf0003cu,20u);
    jagm_write16(&m,0xf00028u,7u);
    m.gpu.go=0u;
    jagm_run_frames(&m,1u);
    assert(m.op_objects==1u&&m.op_gpu_objects==1u);
    assert(m.op_waiting_for_obf==1u);
    jagm_write16(&m,0xf00026u,0u);
    assert(m.op_objects==2u&&m.op_gpu_objects==2u);
    assert(m.op_waiting_for_obf==1u);
    assert((m.int1_pending&4u)==0u);
    jagm_write16(&m,0xf00026u,0u);
    assert(m.op_objects==3u&&m.op_gpu_objects==2u);
    assert(m.op_waiting_for_obf==0u&&(m.int1_pending&4u)!=0u);
}

int main(void)
{
    test_deferred(0x0000u);
    test_deferred(0x0001u);
    test_deferred(0x8000u);
    test_deferred(0x8001u);
    test_deferred_hc10();
    test_two_delayed_gpu_objects();
    puts("R249 deferred GPUOBJ OBF list cursor PASS");
    return 0;
}
