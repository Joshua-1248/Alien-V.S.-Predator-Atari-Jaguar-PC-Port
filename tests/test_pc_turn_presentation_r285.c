/* R285: PC visual yaw respects Smooth Presentation for both turn backends.
 * Keyboard rendering predicts Jaguar's NEXT signed velocity without running
 * or mutating a 68000 gameplay tick. */
#include "pc/pc_turn_presentation.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    uint32_t result=0,first=0,half=0,full=0;
    /* A held keyboard right key must show intermediate heading at 60 Hz. */
    assert(avp_pc_digital_turn_display(0x20000000u,0,0x300,0x600,1,0.0,&first));
    assert(avp_pc_digital_turn_display(0x20000000u,0,0x300,0x600,1,0.5,&half));
    assert(avp_pc_digital_turn_display(0x20000000u,0,0x300,0x600,1,1.0,&full));
    assert(first==0x20000000u);
    assert(half==0x1e800000u);
    assert(full==0x1d000000u);
    assert(avp_pc_digital_turn_display(full,-0x300,0x300,0x600,1,1.0,&result));
    assert(result==0x17000000u); /* accelerated signed turn */
    assert(avp_pc_digital_turn_display(result,-0x600,0x300,0x600,-1,1.0,&result));
    assert(result==0x1a000000u); /* direction change resets acceleration */
    assert(avp_pc_digital_turn_display(0xfffd0000u,0,0x300,0x600,-1,1.0,&result));
    assert(result==0x00000000u); /* retail cardinal-angle snap */
    assert(!avp_pc_digital_turn_display(0,0,0x300,0x600,0,.5,&result));
    assert(!avp_pc_digital_turn_display(0,0,0x300,0x600,1,1.5,&result));
    assert(!avp_pc_digital_turn_display(0,0,0x300,0x600,1,-.1,&result));
    /* With Smooth OFF, neither fractional stick nor live host heading may
     * override the completed game-tick angle in the renderer. */
    assert(!avp_pc_turn_visual_override_allowed(0,1));
    assert(avp_pc_turn_visual_override_allowed(1,1));
    assert(!avp_pc_turn_visual_override_allowed(1,0));
    puts("R285 keyboard display yaw and Smooth OFF analog isolation PASS");
    return 0;
}
