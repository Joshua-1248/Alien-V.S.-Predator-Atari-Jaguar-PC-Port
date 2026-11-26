/* RE41 R230: a GPU interrupt object halts the TOM Object Processor until
 * software writes OBF.  An idle GPU must NOT implicitly release the object
 * and run a subsequent STOP or bitmap.  This validates the effect of the
 * interrupt object, not an estimate of in-flight bus/line-buffer timing. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x3000];
static void put64(uint32_t a, uint64_t v)
{
    jagm_write32(&m,a,(uint32_t)(v>>32));
    jagm_write32(&m,a+4u,(uint32_t)v);
}
int main(void)
{
    const uint64_t gpu=UINT64_C(0xcafebabe12345002);
    const uint64_t stop=UINT64_C(0x56c4a9e31dffe00c); /* STOP + interrupt */
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xfe;
    assert(jagm_init(&m,cart,sizeof cart)==0);
    put64(0x1000u,gpu);
    put64(0x1008u,stop);
    jagm_write16(&m,0xF00020u,0x1000u);
    jagm_write16(&m,0xF00022u,0u);
    jagm_write16(&m,0xF0002Eu,31u);
    jagm_write16(&m,0xF0003Eu,31u);
    jagm_write16(&m,0xF00046u,28u);
    jagm_write16(&m,0xF00048u,30u);
    jagm_write16(&m,0xF00038u,4u);
    jagm_write16(&m,0xF0003Au,127u);
    jagm_write16(&m,0xF0003Cu,20u);
    jagm_write16(&m,0xF00028u,7u);
    m.gpu.go=0u; /* interrupt is raised; no handler writes OBF */
    jagm_run_frames(&m,1u);
    assert(m.op_gpu_objects==1u);
    if(m.op_objects!=1u || m.ob!=gpu || (m.int1_pending&4u)!=0u)
        fprintf(stderr,"R229 defect: unreleased GPUOBJ advanced to STOP: objects=%llu OB=%016llx int1=%04x\n",
            (unsigned long long)m.op_objects,(unsigned long long)m.ob,m.int1_pending);
    assert(m.op_objects==1u); /* STOP never read */
    assert(m.ob==gpu); /* GPU phrase still latched */
    assert((m.int1_pending&4u)==0u); /* STOP interrupt not raised */
    assert(m.op_waiting_for_obf==1u);
    assert((m.op_unsupported_mask&32u)!=0u); /* no timed resume modeled */
    /* A new HDB comparator in the next field cannot restart the OP while
     * its previous GPU interrupt object remains unreleased. */
    jagm_run_frames(&m,1u);
    assert(m.op_lines==1u && m.op_objects==1u);
    assert(m.op_waiting_for_obf==1u);
    jagm_write16(&m,0xF00026u,0x8000u); /* any value releases the wait */
    assert(m.op_waiting_for_obf==0u && m.obf==0x8000u);
    puts("R230 GPUOBJ stays halted until OBF write PASS");
    return 0;
}
