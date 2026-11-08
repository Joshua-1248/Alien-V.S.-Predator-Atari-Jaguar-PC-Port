/* R42: the original HUD_MSG records choose when an alert is dark;
 * the PC display chooses its hue based on the character in that viewport. */
#include "jaguar/avp_gpu/popup_message_color.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

int main(void)
{
    const int types[3]={PT_ALIEN,PT_PREDATOR,PT_HUMAN};
    const unsigned rgb[3]={0x00b860ffu,0x00ffff00u,0x0000ff00u};
    const unsigned source_inks[]={0xffff0000u,0x00ff0000u,0xffffff00u,
        0xff000000u,0x00ff00ffu,0x00ffffffu};
    for(unsigned species=0;species<3u;species++){
        assert(avp_pc_popup_ink_for_species(types[species])==rgb[species]);
        for(unsigned j=0;j<sizeof source_inks/sizeof source_inks[0];j++)
            assert(avp_pc_popup_ink_for_step(types[species],source_inks[j])==rgb[species]);
        /* Repeated blank timer records must remain invisible. */
        assert(avp_pc_popup_ink_for_step(types[species],0u)==0u);
    }
    assert(avp_pc_popup_ink_for_species(-1)==0x00ffffffu);
    puts("R42 species-colored pickup/warning popup ink and blink phases: PASS");
    return 0;
}
