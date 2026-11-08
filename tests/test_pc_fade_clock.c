#include "pc/pc_fade_clock.h"
#include <assert.h>
#include <stdint.h>

int main(void)
{
    const uint64_t t=AVP_PC_NTSC_VBL_NS;
    assert(AVP_PC_FADE_TICKS==64u);
    assert(avp_pc_fade_ticks_due(0u)==0u);
    assert(avp_pc_fade_ticks_due(t-1u)==0u);
    assert(avp_pc_fade_ticks_due(t)==1u);
    assert(avp_pc_fade_ticks_due(10u*t)==10u);
    assert(avp_pc_fade_ticks_due(32u*t)==32u);
    assert(avp_pc_fade_ticks_due(64u*t-1u)==63u);
    assert(avp_pc_fade_ticks_due(64u*t)==64u);
    assert(avp_pc_fade_ticks_due(300u*t)==64u);
    /* A rendered frame taking ten nominal VBlanks must allow the game
     * fade clock to advance by ten, not add ten new frame sleeps. */
    assert(avp_pc_fade_ticks_due(11u*t)-avp_pc_fade_ticks_due(t)==10u);
    return 0;
}
