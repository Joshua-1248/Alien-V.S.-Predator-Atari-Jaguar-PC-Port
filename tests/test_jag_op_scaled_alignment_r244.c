/* RE42 R244: SCBITOBJ requires a 32-byte-aligned header (TRM Rev.8 p.19).
 * Validate the read-only trace and executable TOM reference machine with
 * original-free, DRAM-resident synthetic scaled objects. */
#include "jaguar/object_processor.h"
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void be64(uint8_t *p,uint64_t v)
{
    for(unsigned i=0;i<8u;i++)p[i]=(uint8_t)(v>>(56u-8u*i));
}
static uint64_t scaled0(uint32_t next)
{
    return UINT64_C(1)|((uint64_t)1u<<14)|
        ((uint64_t)(next>>3)<<24)|((uint64_t)(0x1400u>>3)<<43);
}
static uint64_t scaled1(void)
{
    return ((uint64_t)4u<<12)|((uint64_t)1u<<15)|
        ((uint64_t)1u<<18)|((uint64_t)1u<<28);
}
static void trace_case(unsigned offset,int valid)
{
    uint8_t mem[0x500];
    AvpJaguarOpTraceStep steps[4];
    AvpJaguarOpTraceResult r;
    const uint32_t base=0x1000u,object=base+offset;
    memset(mem,0,sizeof mem);memset(steps,0xa5,sizeof steps);
    be64(mem+offset,scaled0(0x1200u));
    be64(mem+offset+8u,scaled1());
    be64(mem+offset+16u,UINT64_C(0x202020));
    be64(mem+0x200u,4u); /* STOP without INT FLAG */
    r=avp_jaguar_op_trace_scanline(mem,sizeof mem,base,object,
                                  2u,0u,steps,4u);
    if(valid){
        assert(r.status==AVP_JAG_OP_TRACE_STOP);
        assert(r.count==2u);
        assert(steps[0].address==object&&steps[0].type==1u);
        assert(steps[0].next_address==0x1200u);
    }else{
        assert(r.status==AVP_JAG_OP_TRACE_INVALID_OBJECT);
        assert(r.address==object);
        assert(r.count==0u);
    }
}

static JagMachine machine;
static uint8_t cart[0x3000];
static void put64(uint32_t address,uint64_t v)
{
    jagm_write32(&machine,address,(uint32_t)(v>>32));
    jagm_write32(&machine,address+4u,(uint32_t)v);
}
static void machine_case(uint32_t object,int valid)
{
    memset(cart,0,sizeof cart);
    cart[0x404]=0u;cart[0x405]=0x80u;cart[0x406]=0x20u;
    cart[0x2000]=0x60u;cart[0x2001]=0xfeu; /* BRA -2 */
    assert(jagm_init(&machine,cart,sizeof cart)==0);
    put64(object,scaled0(0x1200u));
    put64(object+8u,scaled1());
    put64(object+16u,UINT64_C(0x202020));
    put64(0x1200u,4u); /* STOP without INT FLAG */
    jagm_write16(&machine,0xF00020u,(uint16_t)object);
    jagm_write16(&machine,0xF00022u,0u);
    jagm_write16(&machine,0xF0002Eu,31u);
    jagm_write16(&machine,0xF0003Eu,3u);
    jagm_write16(&machine,0xF00046u,0u);
    jagm_write16(&machine,0xF00048u,2u);
    jagm_write16(&machine,0xF00038u,4u);
    jagm_write16(&machine,0xF0003Au,0x7ffu);
    jagm_write16(&machine,0xF0003Cu,0x0020u);
    jagm_write16(&machine,0xF00028u,7u);
    jagm_run_frames(&machine,1u);
    assert(machine.frames==1u);
    assert(machine.op_objects>=1u);
    if(valid){
        assert(machine.op_unsupported_mask==0u);
        assert(machine.op_objects>=2u);
    }else{
        assert((machine.op_unsupported_mask&512u)!=0u);
    }
    assert((machine.int1_pending&4u)==0u);
}
int main(void)
{
    trace_case(0u,1);
    trace_case(8u,0);
    trace_case(16u,0);
    trace_case(24u,0);
    trace_case(32u,1);
    machine_case(0x1000u,1);
    machine_case(0x1008u,0);
    machine_case(0x1010u,0);
    machine_case(0x1018u,0);
    machine_case(0x1020u,1);
    puts("R244 scaled Object Processor 32-byte header alignment PASS");
    return 0;
}
