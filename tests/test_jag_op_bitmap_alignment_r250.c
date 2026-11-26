/* RE42/R250: TRM Rev.8 p.18: TYPE=0 BITOBJ must start on a 16-byte
 * 64-bit RAM boundary; TYPE=1 SCBITOBJ needs a 32-byte boundary (R244).
 * Check the standalone read-only list tracer and the executable Jaguar
 * reference machine; invalid objects are reported, not silently rendered.
 * Synthetic DRAM-only fixtures, no retail ROM required. */
#include "jaguar/object_processor.h"
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void be64(uint8_t *p,uint64_t v)
{
    for(unsigned i=0u;i<8u;i++)p[i]=(uint8_t)(v>>(56u-8u*i));
}
static uint64_t bitobj0(uint32_t stop)
{
    return ((uint64_t)1u<<14)|((uint64_t)(stop>>3)<<24)|
           ((uint64_t)(0x1800u>>3)<<43);
}
static uint64_t bitobj1(void)
{
    return 8u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
           ((uint64_t)1u<<18)|((uint64_t)1u<<28);
}
static void trace_case(uint32_t address,int valid)
{
    const uint32_t base=0x1000u,stop=0x1200u;
    uint8_t memory[0x300];
    AvpJaguarOpTraceStep steps[4];
    AvpJaguarOpTraceResult r;
    memset(memory,0,sizeof memory);
    memset(steps,0,sizeof steps);
    be64(memory+(address-base),bitobj0(stop));
    be64(memory+(address-base+8u),bitobj1());
    be64(memory+(stop-base),4u);
    r=avp_jaguar_op_trace_scanline(memory,sizeof memory,base,address,
                                  0u,0u,steps,4u);
    if(valid){
        assert(r.status==AVP_JAG_OP_TRACE_STOP&&r.count==2u);
        assert(steps[0].address==address&&steps[0].type==0u);
        assert(steps[0].next_address==stop);
    }else{
        if(r.status!=AVP_JAG_OP_TRACE_INVALID_OBJECT)
            fprintf(stderr,"R249 tracer accepted misaligned BITOBJ at %06x (status=%d)\n",
                    address,(int)r.status);
        assert(r.status==AVP_JAG_OP_TRACE_INVALID_OBJECT);
        assert(r.address==address&&r.count==0u);
    }
}
static JagMachine machine;
static uint8_t rom[0x3000];
static void put64(uint32_t a,uint64_t v)
{
    jagm_write32(&machine,a,(uint32_t)(v>>32));
    jagm_write32(&machine,a+4u,(uint32_t)v);
}
static void machine_case(uint32_t address,int valid)
{
    memset(rom,0,sizeof rom);
    rom[0x404u]=0u;rom[0x405u]=0x80u;rom[0x406u]=0x20u;
    rom[0x2000u]=0x60u;rom[0x2001u]=0xfeu;
    assert(jagm_init(&machine,rom,sizeof rom)==0);
    put64(address,bitobj0(0x1200u));
    put64(address+8u,bitobj1());
    put64(0x1200u,4u);
    put64(0x1800u,UINT64_C(0xf800f800f800f800));
    jagm_write16(&machine,0xF00020u,(uint16_t)address);
    jagm_write16(&machine,0xF00022u,0u);
    jagm_write16(&machine,0xF0002Eu,31u);
    jagm_write16(&machine,0xF0003Eu,31u);
    jagm_write16(&machine,0xF00046u,28u);
    jagm_write16(&machine,0xF00048u,30u);
    jagm_write16(&machine,0xF00038u,4u);
    jagm_write16(&machine,0xF0003Au,0x7ffu);
    jagm_write16(&machine,0xF0003Cu,20u);
    jagm_write16(&machine,0xF00028u,7u);
    jagm_run_frames(&machine,1u);
    assert(machine.frames==1u&&machine.op_lines==1u);
    if(valid){
        assert(machine.op_unsupported_mask==0u);
        assert(machine.op_objects==2u);
        assert(machine.frame[8u]!=0u);
    }else{
        if((machine.op_unsupported_mask&1024u)==0u)
            fprintf(stderr,"R249 machine accepted misaligned BITOBJ at %06x (mask=%x)\n",
                    address,machine.op_unsupported_mask);
        assert((machine.op_unsupported_mask&1024u)!=0u);
        assert(machine.op_objects==1u);
        assert(machine.frame[8u]==0u);
    }
}
int main(void)
{
    trace_case(0x1000u,1);
    trace_case(0x1008u,0);
    trace_case(0x1010u,1);
    trace_case(0x1018u,0);
    machine_case(0x1000u,1);
    machine_case(0x1008u,0);
    machine_case(0x1010u,1);
    machine_case(0x1018u,0);
    puts("R250 OP BITOBJ 16-byte header alignment PASS");
    return 0;
}
