/* R236: Jaguar TRM Rev.8 pp.60/110 BIG_INSTR controls JRISC fetch
 * lane order, with MOVEI's low then high immediate order unchanged.
 * Test both GPU and DSP, both settings, and external instruction fetch. */
#include "jaguar/jrisc.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t external[32];
static uint16_t op(unsigned code,unsigned src,unsigned dst)
{ return (uint16_t)((code<<10)|(src<<5)|dst); }
static void word(uint8_t *p,unsigned n,uint16_t value)
{ p[2*n]=(uint8_t)(value>>8);p[2*n+1]=(uint8_t)value; }
static uint32_t read_external(void *ctx,uint32_t address,int width)
{ (void)ctx;assert(width==2);assert(address>=0x1000u&&address<0x101Eu);
  unsigned off=address-0x1000u;
  return (uint16_t)((external[off]<<8)|external[off+1u]); }

static void test_word_order(int kind,int high_first)
{
    JRisc j;JRiscBus bus={0};
    jrisc_init(&j,kind,bus);
    if(high_first)assert(jrisc_local_write(&j,j.base+0x10Cu,4u));
    else assert(j.bigend==0u);
    /* physical memory: high word has MOVEQ 7, low word MOVEQ 9 */
    word(j.ram,0,op(35,7,1));word(j.ram,1,op(35,9,1));
    j.go=1u;
    assert(jrisc_step(&j)>0u);
    assert(j.reg[0][1]==(high_first?7u:9u));
    assert(jrisc_step(&j)>0u);
    assert(j.reg[0][1]==(high_first?9u:7u));
    assert(j.pc==j.ram_base+4u);
}
static void test_movei(int kind,int high_first)
{
    JRisc j;JRiscBus bus={0};jrisc_init(&j,kind,bus);
    jrisc_local_write(&j,j.base+0x10Cu,high_first?4u:0u);
    if(high_first){
        word(j.ram,0,op(38,0,2)); /* MOVEI */
        word(j.ram,1,0x5678u); /* low immediate */
        word(j.ram,2,0x1234u); /* high immediate */
    }else{
        word(j.ram,1,op(38,0,2)); /* PC+0 selects low physical word */
        word(j.ram,0,0x5678u); /* PC+2 selects high physical word */
        word(j.ram,3,0x1234u); /* PC+4 selects next low physical word */
    }
    j.go=1u;assert(jrisc_step(&j)>0u);
    assert(j.reg[0][2]==0x12345678u);
    assert(j.pc==j.ram_base+6u);
}
static void test_external(int kind,int high_first)
{
    JRisc j;JRiscBus bus={0,read_external,0};jrisc_init(&j,kind,bus);
    memset(external,0,sizeof external);
    word(external,0,op(35,3,1));
    word(external,1,op(35,5,1));
    jrisc_local_write(&j,j.base+0x10Cu,high_first?4u:0u);
    j.pc=0x1000u;j.go=1u;
    assert(jrisc_step(&j)>0u);
    assert(j.reg[0][1]==(high_first?3u:5u));
    assert(jrisc_step(&j)>0u);
    assert(j.reg[0][1]==(high_first?5u:3u));
}
int main(void)
{
    for(int kind=JRISC_GPU;kind<=JRISC_DSP;kind++)
        for(int big_instr=0;big_instr<=1;big_instr++){
            test_word_order(kind,big_instr);
            test_movei(kind,big_instr);
            test_external(kind,big_instr);
        }
    puts("R236 GPU/DSP BIG_INSTR lane-order and MOVEI tests PASS");
    return 0;
}
