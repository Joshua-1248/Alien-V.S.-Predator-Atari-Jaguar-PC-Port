/* RE35 R99: the Jaguar FONT.S 3-VBlank typewriter must be paced against
 * VIDEO FIELD deadlines, not VBlank *plus* software renderer time.
 * No real ROM bytes are embedded in these host-clock checks. */
#include "pc/pc_frontend_field_clock.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void check(unsigned hz)
{
    uint64_t deadline=0u;
    const uint64_t start=2000000000ull;
    const uint64_t interval=hz==50u?20000000ull:16666667ull;
    unsigned i;
    /* 3 VBlanks of original FONT.S per glyph, after varying host render
       times. This must not accumulate 1..12ms extra per field. */
    for(i=0u;i<180u;i++){
        const uint64_t actual=start+(uint64_t)i*interval+(i%3u)*1000000ull;
        deadline=avp_pc_computer_next_deadline(deadline,actual,hz);
        assert(deadline==start+(uint64_t)(i+1u)*interval);
    }
    /* A new modal session is anchored anew.  No stale title/gameplay time. */
    assert(avp_pc_computer_next_deadline(0u,987654321u,hz)==987654321u+interval);
    /* A suspended window must reanchor rather than spin through missed fields. */
    assert(avp_pc_computer_next_deadline(deadline,deadline+2000000000ull,hz)==
           deadline+2000000000ull+interval);
    /* Exactly three fields per $FE-flagged character at original cadence. */
    assert(3u*interval==(hz==50u?60000000ull:50000001ull));
}
int main(void)
{
    check(60u);check(50u);
    /* Keep title and story clocks independent; no fallback title path. */
    assert(avp_pc_mjp_field_paced(AVP_MJP_INTRO));
    puts("PASS: COMPUTER.S original 3-VBlank clock, NTSC/PAL and modal resets");
    return 0;
}
