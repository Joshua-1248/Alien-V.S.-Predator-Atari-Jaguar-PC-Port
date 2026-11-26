/* RE41 R229: the original Flare/Atari TOM netlist OB.NET drives
 * phrase bits [15:0] onto OB0, then [31:16], [47:32], [63:48].
 * Do not confuse little-phrase-word ordering with big-endian bytes in
 * each individual 16-bit register. Actual synthetic 68000 cartridge. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static JagMachine machine;
static uint8_t cart[0x4000];
static unsigned pc;
static void emit16(uint16_t v){cart[pc++]=(uint8_t)(v>>8);cart[pc++]=(uint8_t)v;}
static void emit32(uint32_t v){emit16((uint16_t)(v>>16));emit16((uint16_t)v);}
static void movel(uint32_t v,uint32_t addr){emit16(0x23fcu);emit32(v);emit32(addr);}
static void movew(uint16_t v,uint32_t addr){emit16(0x33fcu);emit16(v);emit32(addr);}
static void check(uint64_t phrase)
{
    const uint32_t list=0x1000u;
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    pc=0x2000u;
    movel((uint32_t)(phrase>>32),list);
    movel((uint32_t)phrase,list+4u);
    movew(28u,0xf00046u);movew(508u,0xf00048u);
    movew(0x00cbu,0xf00038u);movew(0x0680u,0xf0003cu);
    movel(list<<16,0xf00020u); /* word-swapped OLP */
    movew(0x06c7u,0xf00028u);
    emit16(0x60feu);
    assert(jagm_init(&machine,cart,sizeof cart)==0);
    jagm_run_frames(&machine,1u);
    assert(machine.op_objects>=1u);
    assert(machine.ob==phrase);
    for(unsigned word=0u;word<4u;word++){
        uint16_t expect=(uint16_t)(phrase>>(16u*word));
        uint16_t got=jagm_read16(&machine,0xf00010u+word*2u);
        if(got!=expect)
            fprintf(stderr,"R228 OB%u word reversed: got=%04x expected=%04x\n",
                    word,got,expect);
        assert(got==expect);
    }
    assert(jagm_read32(&machine,0xf00010u)==
           (((uint32_t)(uint16_t)phrase<<16)|(uint16_t)(phrase>>16)));
    assert(jagm_read32(&machine,0xf00014u)==
           (((uint32_t)(uint16_t)(phrase>>32)<<16)|(uint16_t)(phrase>>48)));
}
int main(void)
{
    /* STOP = low 3 bits 4, bit 3 controls CPU IRQ independently. */
    check(UINT64_C(0xa56c7319428f1004));
    check(UINT64_C(0x5a938ce6bd70effc));
    puts("R229 TOM OB0..OB3 hardware phrase-word order passed");
    return 0;
}
