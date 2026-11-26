/* RE41 R233: Atari TRM Rev.8 p.51 MTOI signed IEEE mantissa.
 * The bit-23 implied one is not the sign extension; the source sign selects
 * a two's-complement negation.  Both the GPU and DSP implement MTOI. */
#include "jaguar/jrisc.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void check(int kind, uint32_t ieee, uint32_t expected)
{
    JRisc j;
    uint16_t opcode=(uint16_t)((55u<<10)|(1u<<5)|2u); /* MTOI R1,R2 */
    JRiscBus bus;
    memset(&bus,0,sizeof bus);
    jrisc_init(&j,kind,bus);
    /* This historical synthetic fixture stores the opcode high-word-first. */
    jrisc_local_write(&j,j.base+0x10Cu,4u);
    j.ram[0]=(uint8_t)(opcode>>8);j.ram[1]=(uint8_t)opcode;
    j.pc=j.ram_base;
    j.reg[0][1]=ieee;
    j.reg[0][2]=0xdeadbeefu;
    j.go=1u;
    j.flags=JRISC_FLAG_C; /* MTOI does not define carry: preserve it */
    assert(jrisc_step(&j)>0u);
    if(j.reg[0][2]!=expected){
        fprintf(stderr,"baseline MTOI defect: kind=%d IEEE=%08x got=%08x expected=%08x\n",
                kind,ieee,j.reg[0][2],expected);
        assert(0);
    }
    assert(j.pc==j.ram_base+2u);
    assert(j.reg[0][1]==ieee);
    assert((j.flags&JRISC_FLAG_N)==((expected&0x80000000u)?JRISC_FLAG_N:0u));
    assert((j.flags&JRISC_FLAG_Z)==(expected?0u:JRISC_FLAG_Z));
    assert((j.flags&JRISC_FLAG_C)==JRISC_FLAG_C);
}

int main(void)
{
    for(int kind=JRISC_GPU; kind<=JRISC_DSP; ++kind){
        check(kind,0x3f800000u,0x00800000u); /* +1.0: implied leading 1 */
        check(kind,0x3fc00000u,0x00c00000u); /* +1.5 */
        check(kind,0xbf800000u,0xff800000u); /* -1.0 */
        check(kind,0xbfc00000u,0xff400000u); /* -1.5: negate, not sign extend */
        check(kind,0x3f923456u,0x00923456u); /* arbitrary mantissa */
        check(kind,0xbf923456u,0xff6dcbaau); /* signed arbitrary mantissa */
    }
    puts("R233 JRISC GPU/DSP MTOI mantissa/sign regression PASS");
    return 0;
}
