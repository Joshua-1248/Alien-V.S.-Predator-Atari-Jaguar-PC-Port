/* RE40 R216: 68000 writes before an HDB event must be visible to the
 * Object Processor. Before R216 the OP always ran at the beginning of the
 * half-line, before the CPU could execute earlier horizontal clocks.
 * Synthetic cartridge, no proprietary assets. */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t rom[0x5000];
static void word(uint32_t a, uint16_t v)
{rom[a]=(uint8_t)(v>>8);rom[a+1]=(uint8_t)v;}
static void lng(uint32_t a, uint32_t v)
{word(a,(uint16_t)(v>>16));word(a+2,(uint16_t)v);}
static void phrase(uint32_t a, uint64_t v)
{jagm_write32(&m,a,(uint32_t)(v>>32));jagm_write32(&m,a+4,(uint32_t)v);}

static uint32_t run_case(uint16_t hdb)
{
    const uint16_t old_color=0x001fu;
    uint64_t p0,p1;
    memset(rom,0,sizeof rom);
    lng(0x404,0x00802000u);
    /* MOVE.L #$F800F800,$00003000 ; BRA.S -2 */
    word(0x2000,0x23fcu);lng(0x2002,0xf800f800u);
    lng(0x2006,0x00003000u);word(0x200a,0x60feu);
    assert(jagm_init(&m,rom,sizeof rom)==0);
    jagm_write32(&m,0x3000u,((uint32_t)old_color<<16)|old_color);
    jagm_write32(&m,0x3004u,((uint32_t)old_color<<16)|old_color);
    p0=((uint64_t)1u<<14)|((uint64_t)(0x1010u>>3)<<24)|
       ((uint64_t)(0x3000u>>3)<<43);
    p1=0u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
       ((uint64_t)1u<<28);
    phrase(0x1000u,p0);phrase(0x1008u,p1);phrase(0x1010u,4u);
    jagm_write16(&m,0xf0002eu,127u);
    jagm_write16(&m,0xf0003eu,3u);
    jagm_write16(&m,0xf00046u,0u);
    jagm_write16(&m,0xf00048u,2u);
    jagm_write16(&m,0xf00038u,hdb);
    jagm_write16(&m,0xf0003au,hdb);
    jagm_write16(&m,0xf0003cu,120u);
    jagm_write16(&m,0xf00020u,0x1000u);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf00028u,0x0007u); /* RGB16, video enabled */
    jagm_run_frames(&m,1u);
    assert(m.frame_h==1u);
    assert(m.op_lines==1u);
    assert(m.op_unsupported_mask==0u);
    return m.frame[0u];
}
/* CPU starts modifying the *second* sprite after HDB1 has fired but
 * before HDB2. The old reference ran HDB2 at the start of the second
 * horizontal half (128 clocks), rather than its programmed 192 clocks. */
static void check_second_hdb(void)
{
    /* An eight-byte BRANCH may immediately precede a 16-byte BITOBJ. */
    const uint32_t list=0x1008u, second=0x1040u;
    uint64_t branch,first,other,p1;
    unsigned pos=0x2000u;
    memset(rom,0,sizeof rom);
    lng(0x404,0x00802000u);
    for(unsigned n=0;n<11u;n++,pos+=2u)word(pos,0x4e71u); /* NOP */
    word(pos,0x23fcu);lng(pos+2u,0x07e007e0u);
    lng(pos+6u,0x00003008u);word(pos+10u,0x60feu);
    assert(jagm_init(&m,rom,sizeof rom)==0);
    jagm_write32(&m,0x3000u,0xf800f800u);
    jagm_write32(&m,0x3004u,0xf800f800u);
    jagm_write32(&m,0x3008u,0x001f001fu);
    jagm_write32(&m,0x300cu,0x001f001fu);
    branch=3u|((uint64_t)0x7ffu<<3)|((uint64_t)4u<<14)|
           ((uint64_t)(second>>3)<<24);
    first=((uint64_t)1u<<14)|((uint64_t)(0x1020u>>3)<<24)|
          ((uint64_t)(0x3000u>>3)<<43);
    other=((uint64_t)1u<<3)|((uint64_t)1u<<14)|
          ((uint64_t)(0x1050u>>3)<<24)|
          ((uint64_t)(0x3008u>>3)<<43);
    p1=10u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
       ((uint64_t)1u<<28);
    phrase(list,branch);
    phrase(0x1010u,first);phrase(0x1018u,p1);phrase(0x1020u,4u);
    phrase(second,other);phrase(second+8u,p1);phrase(0x1050u,4u);
    jagm_write16(&m,0xf0002eu,127u); /* each half 128 clock ticks */
    jagm_write16(&m,0xf0003eu,3u);
    jagm_write16(&m,0xf00046u,0u);
    jagm_write16(&m,0xf00048u,2u);
    jagm_write16(&m,0xf00038u,32u);
    jagm_write16(&m,0xf0003au,0x0440u); /* 128+64 = 192 */
    jagm_write16(&m,0xf0003cu,0x0470u);
    jagm_write16(&m,0xf00020u,(uint16_t)list);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf00028u,0x0607u); /* RGB16, PW=4, enabled */
    jagm_run_frames(&m,1u);
    assert(m.frame_w==52u);
    assert(m.op_lines==2u);
    assert(m.frame[10u]==avp_jaguar_vmode_rgb16(0xf800u));
    assert(m.frame[50u]==avp_jaguar_vmode_rgb16(0x07e0u));
}

int main(void)
{
    const uint32_t before=avp_jaguar_vmode_rgb16(0x001fu);
    const uint32_t after=avp_jaguar_vmode_rgb16(0xf800u);
    /* HDB=0: CPU has not yet run a single instruction. */
    uint32_t early=run_case(0u);
    printf("at 0: got %08x want %08x\n",early,before);
    assert(early==before);
    /* By HDB=96 clocks, the initial CPU MOVE.L has completed. */
    uint32_t late=run_case(96u);
    printf("at 96: got %08x want %08x\n",late,after);
    assert(late==after);
    check_second_hdb();
    puts("R216 TOM HDB1/HDB2 honor pre-event 68000 writes PASS");
    return 0;
}
