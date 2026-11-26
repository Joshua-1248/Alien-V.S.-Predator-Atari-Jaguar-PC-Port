/* RE39 R162: independently assert the visible Jaguar filter-to-enhancement
 * wiring. R161's table-based test assumed labels and passed the wrong modes.
 * The supplied same-scene screenshots resolve the original RDB.DTA outputs:
 * Electromagnetic=RGB16/$9558, Thermal=RGB16/$26F8,
 * Night Vision=RGB24/$8880, Infrared=RGB16/$8880.
 * This test must FAIL on R161's thermal_vision.c and PASS on R162. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "jaguar/cry.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* These are the exact Jaguar retail framebuffer color modifiers when
     * fade_c=$8880 and fade_level=0, not estimated on-screen RGB values. */
    assert(avp_jaguar_game_clut(0x8880u,0x9558u,0)==0x1dd8u);
    assert(avp_jaguar_game_clut(0x8880u,0x26f8u,0)==0xae78u);
    assert(avp_jaguar_game_clut(0x8880u,0x8880u,0)==0u);

    /* The source-mode IDs come from *rendered output*, not the mistaken
     * mode-name assumptions that R161 attached to the RDB.DTA array. */
    assert(avp_predator_vision_style(1,0,PT_PREDATOR,2,0x06c7u,0x26f8u)
           ==AVP_VISION_ENHANCED_THERMAL);
    assert(avp_predator_vision_style(0,1,PT_PREDATOR,2,0x06c7u,0x9558u)
           ==AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,0x06c7u,0x26f8u)
           ==AVP_VISION_ENHANCED_THERMAL);
    assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,0x06c7u,0x9558u)
           ==AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    /* The formerly modified green Night Vision and pink/yellow Infrared
     * images MUST be pristine, even when BOTH toggles are enabled. */
    assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,0x0ec3u,0x8880u)
           ==AVP_VISION_ORIGINAL);
    assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,0x06c7u,0x8880u)
           ==AVP_VISION_ORIGINAL);
    assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,0x0ec3u,0x0cf8u)
           ==AVP_VISION_ORIGINAL);
    for(unsigned state=0;state<2;state++){
        for(unsigned i=0;i<2;i++){
            const u16 extra=i?0x9558u:0x26f8u;
            assert(avp_predator_vision_style(1,1,PT_PREDATOR,state,0x06c7u,extra)
                   ==AVP_VISION_ORIGINAL);
        }
    }
    puts("RE39 R162 screenshot-calibrated Thermal / Electromagnetic selector PASS");
    return 0;
}
