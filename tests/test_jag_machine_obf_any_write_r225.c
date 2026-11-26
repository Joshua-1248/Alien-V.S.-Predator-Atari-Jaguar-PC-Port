/* RE41 R225: An OP GPU interrupt object resumes on any write to OBF.
 * The GPU may write the high bit: it belongs to the write value, not to
 * an emulator-private busy sentinel.  These are synthetic cartridge tests;
 * they do not assert bus-cycle-accurate GPU/OP arbitration. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x4000];
static unsigned gp;
static uint16_t ins(unsigned op,unsigned r1,unsigned r2)
{return (uint16_t)((op<<10)|((r1&31u)<<5)|(r2&31u));}
static void emit(uint16_t value)
{
    unsigned off=gp-0xF03000u;
    assert(off+2u<=m.gpu.ram_size);
    m.gpu.ram[off]=(uint8_t)(value>>8);
    m.gpu.ram[off+1u]=(uint8_t)value;
    gp+=2u;
}
static void movei(uint32_t value,unsigned reg)
{emit(ins(38u,0u,reg));emit((uint16_t)value);emit((uint16_t)(value>>16));}
static void p64(uint32_t a,uint64_t phrase)
{jagm_write32(&m,a,(uint32_t)(phrase>>32));jagm_write32(&m,a+4u,(uint32_t)phrase);}
static void check(uint16_t obf_value)
{
    uint64_t branch;
    memset(cart,0,sizeof cart);
    cart[0x404u]=0u;cart[0x405u]=0x80u;cart[0x406u]=0x20u;
    cart[0x2000u]=0x60u;cart[0x2001u]=0xFEu;
    assert(jagm_init(&m,cart,sizeof cart)==0);
    /* Synthetic GPU program is encoded high-word-first (BIG_INSTR=1). */
    jrisc_local_write(&m.gpu,m.gpu.base+0x10Cu,4u);
    gp=0xF03000u;
    movei(0xF00026u,3u);
    movei(obf_value,4u);
    emit(ins(46u,3u,4u)); /* STOREW [OBF], r4: ANY value must release OP. */
    movei(0xF02114u,3u);
    emit(ins(35u,0u,4u)); /* MOVEQ #0,r4 */
    emit(ins(47u,3u,4u)); /* STORE [G_CTRL], r4: halt GPU */
    m.gpu.pc=0xF03000u;
    m.gpu.go=1u;

    p64(0x1000u,2u); /* GPU interrupt object */
    /* A CC=3 branch tests OBF bit0 after the GPU write. */
    branch=3u|((uint64_t)3u<<14)|((uint64_t)(0x1018u>>3)<<24);
    p64(0x1008u,branch);
    p64(0x1010u,4u);  /* STOP without object interrupt */
    p64(0x1018u,12u); /* STOP with object interrupt */
    jagm_write16(&m,0xF00020u,0x1000u);
    jagm_write16(&m,0xF00022u,0u);
    jagm_write16(&m,0xF0002Eu,31u); /* 32 clocks per half-line */
    jagm_write16(&m,0xF0003Eu,31u);
    jagm_write16(&m,0xF00046u,0u);
    jagm_write16(&m,0xF00048u,2u);
    jagm_write16(&m,0xF00038u,0u);  /* OP at beginning of first half */
    jagm_write16(&m,0xF0003Au,127u); /* no second OP start */
    jagm_write16(&m,0xF0003Cu,20u);
    jagm_write16(&m,0xF00028u,7u);
    jagm_run_frames(&m,1u);
    assert(m.op_lines==1u&&m.op_gpu_objects==1u);
    if(m.op_unsupported_mask&32u){
        fprintf(stderr,"R224 defect: OBF write %04x did not release OP (mask=%08x)\n",
                obf_value,m.op_unsupported_mask);
    }
    assert((m.op_unsupported_mask&32u)==0u);
    assert(m.obf==obf_value);
    assert(((m.int1_pending&4u)!=0u)==((obf_value&1u)!=0u));
}
int main(void)
{
    check(0x0000u);
    check(0x0001u);
    check(0x8000u);
    check(0x8001u);
    puts("R225 OP GPU-object OBF any-write resume PASS");
    return 0;
}
