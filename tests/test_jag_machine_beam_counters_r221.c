/* RE41 R221 — TOM HC / VC documented counter readback.
 * Atari Rev.8 pp.11-12: HC low ten bits advance on pixel clocks and
 * bit 10 marks the second horizontal half; VC bit 11 marks field parity.
 * A synthetic, independent 68000 cartridge captures two HC samples.
 * This is not a bus-accurate arbitration/timing regression. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t rom[0x5000];
static void put16(unsigned addr,uint16_t val){rom[addr]=(uint8_t)(val>>8);rom[addr+1]=(uint8_t)val;}
static void put32(unsigned addr,uint32_t val){put16(addr,(uint16_t)(val>>16));put16(addr+2u,(uint16_t)val);}

static void make_cartridge(void)
{
    unsigned pc=0x2000u;
    memset(rom,0,sizeof rom);
    put32(0x404u,0x00802000u);
    /* Delay first read about 32 system clocks at 68000/2 rate. */
    for(unsigned i=0;i<4u;i++){put16(pc,0x4e71u);pc+=2u;} /* NOP */
    /* MOVE.W $00F00004,$00003000 — first HC read, still in first half. */
    put16(pc,0x33f9u);put32(pc+2u,0x00f00004u);put32(pc+6u,0x00003000u);pc+=10u;
    /* Delay second sample into the second horizontal half. */
    for(unsigned i=0;i<4u;i++){put16(pc,0x4e71u);pc+=2u;}
    put16(pc,0x33f9u);put32(pc+2u,0x00f00004u);put32(pc+6u,0x00003002u);pc+=10u;
    put16(pc,0x60feu); /* BRA.S -2 */
}

static void check(unsigned vp)
{
    uint16_t early,late;
    make_cartridge();
    assert(jagm_init(&m,rom,sizeof rom)==0);
    jagm_write16(&m,0xf0002eu,127u); /* 128 pixel clocks / half line */
    jagm_write16(&m,0xf0003eu,(uint16_t)vp);
    jagm_run_frames(&m,1u);
    assert(m.frames==1u);
    early=jagm_read16(&m,0x3000u);
    late=jagm_read16(&m,0x3002u);
    if(!(early>0u && early<128u && (late&0x400u) &&
         (late&0x3ffu)<128u)){
        fprintf(stderr,"R221 baseline HC defect: VP=%u early=%04x late=%04x\n",
                vp,early,late);
        assert(0);
    }
    /* First field has completed; bit 11 must now select the next field. */
    assert(jagm_read16(&m,0xf00006u)==0x800u);
    assert(jagm_read16(&m,0xf00004u)==0x000u);
    jagm_run_frames(&m,1u);
    assert(m.frames==2u);
    assert(jagm_read16(&m,0xf00006u)==0x000u);
    jagm_run_frames(&m,1u);
    assert(m.frames==3u);
    assert(jagm_read16(&m,0xf00006u)==0x800u);
}
int main(void)
{
    /* VP+1 even = noninterlaced, odd = interlaced: field identification
     * must remain visible to software in both programmed configurations. */
    check(3u);
    check(4u);
    puts("R221 TOM HC/VC exposed beam counters PASS");
    return 0;
}
