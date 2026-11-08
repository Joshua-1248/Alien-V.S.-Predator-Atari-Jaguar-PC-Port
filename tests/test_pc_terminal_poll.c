/* PC-only regression: COMPUTER.S polls the pad in tight A/B release loops. */
#include "pc/pc_platform.h"
#include "joypad.h"
#include <assert.h>
#include <stdio.h>

static unsigned samples;
static int held;
static void pump(void)
{
    ++samples;
    if(samples==3u){held=0;avp_pc_pad_set(0xffffffffu);}
}

int main(void)
{
    const u32 held_a=0xffffffffu&~(1u<<FIRE_A);
    initpad();
    avp_pc_pad_set(held_a);
    held=1;samples=0u;
    avp_pc_pad_set_poll_callback(pump);
    readpad();
    assert(held && !(joy_cur&(1u<<FIRE_A)));
    assert((joy_edge&(1u<<FIRE_A))==0u);
    /* The original terminal repeatedly calls readpad() without VSync.
     * A host that only polls once per VBL would never observe this release. */
    for(unsigned n=0;n<10u && !(joy_cur&(1u<<FIRE_A));n++)readpad();
    assert(samples>=3u && !held);
    assert((joy_cur&(1u<<FIRE_A))!=0u);
    avp_pc_pad_set_poll_callback(NULL);
    avp_pc_pad_set(0xffffffffu&~(1u<<FIRE_C));
    readpad();
    assert((joy_cur&(1u<<FIRE_C))==0u);
    assert((joy_edge&(1u<<FIRE_C))==0u);
    puts("PC terminal continuous-pad polling passed.");
    return 0;
}
