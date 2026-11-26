/* RE38 R145: GPU/DSP SH/SHA signed 32-bit counts at both extremes.
 * Atari Jaguar TRM Rev.8 pp.54-55: negative => left, nonnegative => right;
 * shifts by 32 or more produce zero for SH, sign-fill for SHA right.
 * The signed count INT32_MIN is valid architectural input and must not
 * trigger C signed-overflow when computing its unsigned magnitude. */
#include "jaguar/jrisc.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef struct ShiftCase {
    uint32_t count, source, expect_sh, expect_sha;
    unsigned carry;
} ShiftCase;

static void check_one(int kind, unsigned op, const ShiftCase *tc)
{
    JRisc j;
    JRiscBus bus = {0};
    uint32_t flags;
    jrisc_init(&j,kind,bus);
    /* This historical synthetic fixture stores the opcode high-word-first. */
    jrisc_local_write(&j,j.base+0x10Cu,4u);
    j.reg[0][3]=tc->count;
    j.reg[0][4]=tc->source;
    j.ram[0]=(uint8_t)((op<<2)|(3u>>3));
    j.ram[1]=(uint8_t)(((3u&7u)<<5)|4u);
    j.go=1;
    assert(jrisc_step(&j)!=0u);
    assert(j.fault==0);
    assert(j.reg[0][4]==(op==23u?tc->expect_sh:tc->expect_sha));
    flags=j.flags;
    assert(((flags&JRISC_FLAG_C)!=0u)==(tc->carry!=0u));
    assert(((flags&JRISC_FLAG_Z)!=0u)==(j.reg[0][4]==0u));
    assert(((flags&JRISC_FLAG_N)!=0u)==((j.reg[0][4]&0x80000000u)!=0u));
}

int main(void)
{
    static const ShiftCase cases[]={
      {0x80000000u,0x80000001u,0u,0u,1u},
      {0xFFFFFFE0u,0x80000001u,0u,0u,1u},
      {0xFFFFFFE1u,0x80000001u,0x80000000u,0x80000000u,1u},
      {0xFFFFFFFFu,0x80000001u,0x00000002u,0x00000002u,1u},
      {0x00000000u,0x80000001u,0x80000001u,0x80000001u,1u},
      {0x00000001u,0x80000001u,0x40000000u,0xC0000000u,1u},
      {0x0000001Fu,0x80000001u,0x00000001u,0xFFFFFFFFu,1u},
      {0x00000020u,0x80000001u,0u,0xFFFFFFFFu,1u},
      {0x7FFFFFFFu,0x80000001u,0u,0xFFFFFFFFu,1u},
      {0xFFFFFFFFu,0x7FFFFFFFu,0xFFFFFFFEu,0xFFFFFFFEu,0u},
      {0x00000020u,0x7FFFFFFFu,0u,0u,1u},
    };
    unsigned n=0;
    for(unsigned kind=JRISC_GPU;kind<=JRISC_DSP;kind++)
        for(unsigned op=23u;op<=26u;op+=3u)
            for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
                check_one((int)kind,op,&cases[i]);n++;
            }
    assert(n==44u);
    printf("R145 GPU/DSP SH/SHA signed shift tests: %u PASS\n",n);
    return 0;
}
