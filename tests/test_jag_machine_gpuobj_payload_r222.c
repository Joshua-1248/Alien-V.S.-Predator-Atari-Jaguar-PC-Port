/* RE41 R222: TOM GPUOBJ is an unconditional one-phrase interrupt object.
 * Atari programming appendix: no YPOS in GPUOBJ; gate with BRANCH.  This
 * test covers both the executable machine and the read-only OP trace.
 * The GPU is stopped intentionally: this tests object selection only,
 * NOT GPU interrupt latency, its ISR or OBF resume bus timing. */
#include "jaguar/jag_machine.h"
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine machine;
static uint8_t rom[0x3000];
static uint8_t objects[0x40];
static void be64(uint8_t *p,uint64_t v)
{
    for(unsigned i=0;i<8u;i++)p[i]=(uint8_t)(v>>(56u-8u*i));
}
static void put64(JagMachine *m,uint32_t a,uint64_t v)
{
    jagm_write32(m,a,(uint32_t)(v>>32));
    jagm_write32(m,a+4u,(uint32_t)v);
}
static void check(unsigned payload_y,unsigned gated)
{
    const uint32_t list=0x1000u;
    const uint64_t gpu=((uint64_t)0x5a72b9u<<32)|
                       ((uint64_t)payload_y<<3)|2u;
    const unsigned scanline=28u;
    unsigned want_gpu;
    AvpJaguarOpTraceStep steps[4];
    AvpJaguarOpTraceResult trace;
    unsigned count;
    uint64_t branch=0u;
    memset(rom,0,sizeof rom);
    memset(objects,0,sizeof objects);
    /* Synthetic cartridge entrypoint: idle in a tight 68000 loop. */
    rom[0x404]=0x00;rom[0x405]=0x80;rom[0x406]=0x20;
    rom[0x2000]=0x60;rom[0x2001]=0xfe;
    assert(jagm_init(&machine,rom,sizeof rom)==0);
    if(gated){
        /* CC=1 branches to stop when YPOS>VC, skipping GPUOBJ. */
        const unsigned gate_y=gated==1u?29u:27u;
        branch=3u|((uint64_t)gate_y<<3)|((uint64_t)1u<<14)|
               ((uint64_t)((list+0x18u)>>3)<<24);
        put64(&machine,list,branch);
        put64(&machine,list+8u,gpu);
        put64(&machine,list+0x10u,4u);
        put64(&machine,list+0x18u,4u);
        be64(objects,branch);
        be64(objects+8u,gpu);
        be64(objects+0x10u,4u);
        be64(objects+0x18u,4u);
    }else{
        put64(&machine,list,gpu);
        put64(&machine,list+8u,4u);
        be64(objects,gpu);
        be64(objects+8u,4u);
    }
    want_gpu=(gated==1u)?0u:1u;
    /* The GPU is deliberately stopped: without an OBF write, hardware
     * halts here rather than executing the subsequent STOP object. */
    count=gated?2u:1u;
    trace=avp_jaguar_op_trace_scanline(objects,sizeof objects,list,list,
                                      scanline,0u,steps,4u);
    if(want_gpu){
        if(trace.status!=AVP_JAG_OP_TRACE_GPU){
            fprintf(stderr,"R221 defect: GPUOBJ payload bits 3..13 = %03x "
                    "mistaken for YPOS: trace status=%d\n",payload_y,
                    (int)trace.status);
        }
        assert(trace.status==AVP_JAG_OP_TRACE_GPU);
        assert(trace.count==(gated?2u:1u));
        assert(steps[trace.count-1u].type==2u);
        assert(steps[trace.count-1u].active==1u);
    }else{
        assert(trace.status==AVP_JAG_OP_TRACE_STOP);
        assert(trace.count==2u);
        assert(steps[0].type==3u&&steps[0].branch_taken);
    }
    jagm_write16(&machine,0xf00020u,(uint16_t)list);
    jagm_write16(&machine,0xf00022u,0u);
    jagm_write16(&machine,0xf0002eu,31u);
    jagm_write16(&machine,0xf0003eu,31u);
    jagm_write16(&machine,0xf00046u,28u);
    jagm_write16(&machine,0xf00048u,30u);
    jagm_write16(&machine,0xf00038u,4u);
    jagm_write16(&machine,0xf0003au,127u);
    jagm_write16(&machine,0xf0003cu,20u);
    jagm_write16(&machine,0xf00028u,7u);
    jagm_run_frames(&machine,1u);
    assert(machine.frames==1u);
    assert(machine.op_lines==1u);
    assert(machine.op_objects==count);
    if(machine.op_gpu_objects!=want_gpu){
        fprintf(stderr,"R221 defect: GPUOBJ payload bits 3..13=%03x "
                "count actual=%llu expected=%u\n",payload_y,
                (unsigned long long)machine.op_gpu_objects,want_gpu);
    }
    assert(machine.op_gpu_objects==want_gpu);
    assert((machine.op_waiting_for_obf!=0u)==(want_gpu!=0u));
    assert(((machine.op_unsupported_mask&32u)!=0u)==(want_gpu!=0u));
    /* Deferred GPUOBJ list continuation remains unimplemented; this test
     * must never silently treat missing OBF as successful release. */
}
int main(void)
{
    /* Unmatched payload values 0, 1, 0x155, and 0x7fe all interrupt;
     * previous R221 behavior incorrectly skipped each one. */
    check(0u,0u);
    check(1u,0u);
    check(0x155u,0u);
    check(0x7feu,0u);
    check(28u,0u); /* matching coincidence remains supported */
    check(0x7ffu,0u); /* old sentinel coincidence remains supported */
    check(0x155u,1u); /* BRANCH skips GPU object */
    check(0x155u,2u); /* BRANCH falls through: interrupt fires */
    puts("R222 TOM GPUOBJ opaque payload + explicit BRANCH gating PASS");
    return 0;
}
