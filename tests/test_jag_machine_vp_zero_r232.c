/* RE41 R232: TOM VP stores the LAST vertical half-line count, not a
 * nonzero-only period.  VP=0 is a valid one-half-line test setting:
 * the NTSC reset default is installed by jagm_init, not by treating
 * a guest write of zero as "use NTSC".  Atari TRM Rev.8 VP, p.15.
 * Checks functional register/field boundaries; no physical video timing
 * validity or shared-bus arbitration is inferred from tiny synthetic VP. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x4000];

static void init(unsigned hp)
{
    memset(cart,0,sizeof cart);
    cart[0x404u]=0;cart[0x405u]=0x80;cart[0x406u]=0x20;
    cart[0x2000u]=0x60;cart[0x2001u]=0xfe; /* BRA.S -2 */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    assert(m.vp==523u); /* reset defaults must remain intact */
    jagm_write16(&m,0xF0002Eu,(uint16_t)(hp-1u));
    jagm_write16(&m,0xF0004Eu,0x7ffu); /* no VI until explicitly requested */
}

static void check_vp(unsigned vp,unsigned hp,unsigned fields)
{
    init(hp);
    jagm_write16(&m,0xF0003Eu,(uint16_t)vp);
    assert(m.vp==vp);
    jagm_run_frames(&m,fields);
    if(m.sys_cycles!=(uint64_t)(vp+1u)*hp*fields){
        fprintf(stderr,"R231 VP=0 fallback defect: VP=%u fields=%u cycles=%llu expected=%llu\n",
                vp,fields,(unsigned long long)m.sys_cycles,
                (unsigned long long)((uint64_t)(vp+1u)*hp*fields));
        assert(0);
    }
    assert(m.frames==fields);
    assert(m.vc==0u);
    assert((jagm_read16(&m,0xF00006u)&0x7ffu)==0u);
    assert(((jagm_read16(&m,0xF00006u)>>11)&1u)==(fields&1u));
}

static void check_vi_at_vp_zero(void)
{
    init(32u);
    jagm_write16(&m,0xF0003Eu,0u);
    jagm_write16(&m,0xF0004Eu,0u);
    jagm_write16(&m,0xF000E0u,1u);
    jagm_run_frames(&m,1u);
    assert(m.sys_cycles==32u);
    assert((m.int1_pending&1u)!=0u);
    jagm_write16(&m,0xF000E0u,0x101u);
    assert((m.int1_pending&1u)==0u);
    jagm_run_frames(&m,1u);
    assert(m.sys_cycles==64u);
    /* VP=0 is a one-half-line interlaced field: V10 specifies VI
     * only once per two fields. The old each-field expectation was
     * over-eager; VI=0 still asserts on the FIRST field wrap. */
    assert((m.int1_pending&1u)==0u);
    jagm_run_frames(&m,1u);
    assert(m.sys_cycles==96u);
    assert((m.int1_pending&1u)!=0u);
}

static void check_m68k_write_vp_zero(void)
{
    /* MOVE.W #$0000,($F0003E).L followed by a tight BRA loop.  This
     * proves the real Musashi-MMIO path, not just a host register setter. */
    init(128u);
    cart[0x2000u]=0x33u;cart[0x2001u]=0xfcu;
    cart[0x2002u]=0x00u;cart[0x2003u]=0x00u;
    cart[0x2004u]=0x00u;cart[0x2005u]=0xf0u;
    cart[0x2006u]=0x00u;cart[0x2007u]=0x3eu;
    cart[0x2008u]=0x60u;cart[0x2009u]=0xfeu;
    jagm_run_frames(&m,1u);
    assert(m.vp==0u);
    assert(m.frames==1u && m.sys_cycles==128u);
}

int main(void)
{
    check_vp(0u,32u,1u);
    check_vp(0u,32u,2u);
    check_vp(1u,32u,1u);
    check_vp(2u,32u,2u);
    check_vp(4u,32u,1u);
    check_vp(523u,32u,1u); /* unchanged NTSC programming */
    check_vi_at_vp_zero();
    check_m68k_write_vp_zero();
    puts("R232 TOM VP=0 period, VI wrap and 68000 write regression PASS");
    return 0;
}
