/* RE40 R208: Jaguar console VDE>VP Object Processor start quirk.
 * Synthetic cartridge executes actual 68000 register writes, then checks
 * OP traversal across one NTSC field with independent STOP lists.
 * Atari Software Reference V10: VDE=$FFFF enables processing every line;
 * the usual VDB gate applies while VDE does not overrun VP.
 */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t rom[0x4000];
static JagMachine machine;
static unsigned off;
static void w16(uint16_t v) { assert(off+2u<=sizeof rom);rom[off++]=(uint8_t)(v>>8);rom[off++]=(uint8_t)v; }
static void w32(uint32_t v) { w16((uint16_t)(v>>16));w16((uint16_t)v); }
static void mw(uint16_t v,uint32_t a) { w16(0x33fcu);w16(v);w32(a); }
static void ml(uint32_t v,uint32_t a) { w16(0x23fcu);w32(v);w32(a); }

static uint64_t run(unsigned vdb,unsigned vde,unsigned vp)
{
    memset(rom,0,sizeof rom);
    rom[0x404]=0;rom[0x405]=0x80;rom[0x406]=0x20;rom[0x407]=0;
    off=0x2000u;
    ml(4u,0x1004u); /* STOP without CPU IRQ at list 0x1000 */
    mw((uint16_t)vp,0xf0003eu); /* horizontal and vertical controls */
    mw((uint16_t)vdb,0xf00046u);
    mw((uint16_t)vde,0xf00048u);
    mw(0x00cbu,0xf00038u);
    mw(0x0680u,0xf0003cu);
    ml(0x10000000u,0xf00020u); /* word-swapped OLP $1000 */
    mw(0x06c7u,0xf00028u);
    w16(0x60feu);
    assert(jagm_init(&machine,rom,sizeof rom)==0);
    jagm_run_frames(&machine,1u);
    assert(machine.frames==1u);
    assert(machine.op_unsupported_mask==0u);
    assert(machine.op_lines==machine.op_objects);
    assert((machine.int1_pending&4u)==0u);
    return machine.op_lines;
}

int main(void)
{
    const uint64_t whole=run(0u,0x07ffu,523u);
    const uint64_t over=run(28u,0x07ffu,523u);
    const uint64_t bounded=run(28u,508u,523u);
    const uint64_t disabled=run(600u,508u,523u);
    /* VDE past VP makes the OP ignore VDB, but it does not rewrite the
     * VDB register or change the ordinary active-range rule otherwise. */
    assert(whole>200u);
    assert(over==whole);
    assert(bounded>0u && bounded<whole);
    assert(disabled==0u);
    printf("R208 VDE>VP console quirk PASS (whole=%llu over=%llu bounded=%llu)\n",
          (unsigned long long)whole,(unsigned long long)over,
          (unsigned long long)bounded);
    return 0;
}
