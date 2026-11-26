/* RE41 R224: TOM VI comparator must also match on a VC wrap to zero.
 * The TRM specifies VI as a half-line count; only noninterlaced
 * configurations impose the odd-VI restriction. Deliberately choose an
 * interlaced field (VP+1 odd). R245 refines this historical boundary test:
 * the V10 hardware reference states that interlaced VI is once per FRAME,
 * not per field. This is not a bus-cycle-accuracy claim. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t rom[0x4000];

static void check(unsigned vi, unsigned first, unsigned second)
{
    memset(rom,0,sizeof rom);
    /* NOP / BRA.S -2, and reset PC at 0x802000. */
    rom[0x404u]=0;rom[0x405u]=0x80;rom[0x406u]=0x20;
    rom[0x2000u]=0x4e;rom[0x2001u]=0x71;
    rom[0x2002u]=0x60;rom[0x2003u]=0xfe;
    assert(jagm_init(&m,rom,sizeof rom)==0);
    jagm_write16(&m,0xF0002Eu,31u);
    jagm_write16(&m,0xF0003Eu,4u); /* VP+1 = 5, interlaced */
    jagm_write16(&m,0xF0004Eu,(uint16_t)vi);
    jagm_write16(&m,0xF000E0u,1u); /* enable video IRQ */
    assert(m.vc==0u && m.int1_pending==0u);
    jagm_run_frames(&m,1u);
    assert(m.frames==1u && m.vc==0u);
    if (((m.int1_pending & 1u)!=0u)!=first) {
        fprintf(stderr,"R224 baseline VI match missing on wrap: VI=%u pending=%04x\n",
                vi,m.int1_pending);
        assert(0);
    }
    /* Acknowledge then prove the interlaced match alternates fields. */
    jagm_write16(&m,0xF000E0u,0x0101u);
    assert((m.int1_pending&1u)==0u);
    jagm_run_frames(&m,1u);
    assert(m.frames==2u && m.vc==0u);
    assert(((m.int1_pending&1u)!=0u)==second);
}

int main(void)
{
    check(0u,1u,0u);     /* VI=0 wrap matches parity-1 field only */
    check(1u,0u,1u);     /* VI>0 match during next parity-1 field */
    check(4u,0u,1u);     /* last programmed half-line */
    check(5u,0u,0u);     /* off-line comparison does not alias wrap */
    check(0x7FFu,0u,0u); /* disabled/out-of-range comparator */
    puts("R224 TOM VI=0 wrap comparison regression PASS");
    return 0;
}
