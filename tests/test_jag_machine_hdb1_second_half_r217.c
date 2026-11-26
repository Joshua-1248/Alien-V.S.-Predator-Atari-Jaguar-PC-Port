/* RE40 R217: HDB1 is an eleven-bit horizontal counter comparator.  When
 * its HC10 bit is set and HDB2 is not a separate display start, the one
 * Object Processor start must happen in the second horizontal half, not
 * at the beginning of the first half.  Synthetic Musashi 68000 cartridge. */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t rom[0x5000];
static void w16(unsigned a,uint16_t v)
{rom[a]=(uint8_t)(v>>8);rom[a+1]=(uint8_t)v;}
static void w32(unsigned a,uint32_t v)
{w16(a,(uint16_t)(v>>16));w16(a+2u,(uint16_t)v);}
static void phrase(uint32_t a,uint64_t p)
{jagm_write32(&m,a,(uint32_t)(p>>32));jagm_write32(&m,a+4u,(uint32_t)p);}

static void check(unsigned hdb1,unsigned hdb2,unsigned ypos,unsigned expected_new)
{
    const uint16_t old=0x001fu,newc=0xf800u;
    uint64_t branch,bitmap,desc;
    memset(rom,0,sizeof rom);
    w32(0x404u,0x00802000u);
    /* CPU writes a different color and then spins. */
    w16(0x2000u,0x23fcu);w32(0x2002u,((uint32_t)newc<<16)|newc);
    w32(0x2006u,0x00003000u);w16(0x200au,0x60feu);
    assert(jagm_init(&m,rom,sizeof rom)==0);
    jagm_write32(&m,0x3000u,((uint32_t)old<<16)|old);
    jagm_write32(&m,0x3004u,((uint32_t)old<<16)|old);
    /* CC4 branches to bitmap only if the start is in HC10=1; the
     * fall-through is a STOP, which must not produce pixels. */
    branch=3u|((uint64_t)0x7ffu<<3)|((uint64_t)4u<<14)|
           ((uint64_t)(0x1040u>>3)<<24);
    bitmap=((uint64_t)ypos<<3)|((uint64_t)1u<<14)|
           ((uint64_t)(0x1050u>>3)<<24)|((uint64_t)(0x3000u>>3)<<43);
    desc=((uint64_t)4u<<12)|((uint64_t)1u<<15)|((uint64_t)1u<<28);
    phrase(0x1000u,branch);phrase(0x1008u,4u);
    phrase(0x1040u,bitmap);phrase(0x1048u,desc);phrase(0x1050u,4u);
    jagm_write16(&m,0xf0002eu,127u); /* two 128-clock halves */
    jagm_write16(&m,0xf0003eu,3u); /* 4 half-lines/field */
    jagm_write16(&m,0xf00046u,0u);jagm_write16(&m,0xf00048u,2u);
    jagm_write16(&m,0xf00038u,(uint16_t)hdb1);
    jagm_write16(&m,0xf0003au,(uint16_t)hdb2);
    jagm_write16(&m,0xf0003cu,0x0470u); /* display ends at clock 240 */
    jagm_write16(&m,0xf00020u,0x1000u);jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf00028u,0x0607u); /* RGB16, PW=4, OP on */
    jagm_run_frames(&m,1u);
    assert(m.frame_h==1u);
    assert(m.op_unsupported_mask==0u);
    assert(m.op_lines==1u);
    assert(m.op_objects==(hdb1&0x400u?3u:2u)); /* CC4 taken only in HC10=1 */
    assert(m.frame_w==(240u-((hdb1&0x3ffu)+((hdb1&0x400u)?128u:0u)))/4u);
    if(expected_new){
        assert(m.frame[0]==avp_jaguar_vmode_rgb16(newc));
    }else{
        assert(m.frame[0]==0u);
    }
}

int main(void)
{
    /* HDB1 is the only OP start.  HDB2 is duplicated or beyond HP. */
    check(0x0440u,0x0440u,1u,1u);
    check(0x0440u,0x0600u,1u,1u);
    /* CC4 is false in the first horizontal half: STOP fall-through. */
    check(0x0020u,0x0020u,0u,0u);
    puts("R217 TOM single HDB1 second-half clock/CC4 PASS");
    return 0;
}
