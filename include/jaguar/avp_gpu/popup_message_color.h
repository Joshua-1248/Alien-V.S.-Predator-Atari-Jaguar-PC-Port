#ifndef AVP_PC_POPUP_MESSAGE_COLOR_H
#define AVP_PC_POPUP_MESSAGE_COLOR_H

#include "player.h"
#include <stdint.h>

/* PC presentation only. Original HUD_MSG command RGB still controls whether
 * its glyphs are illuminated at a given 10-Hz tick. The viewing character
 * determines the illuminated ink, independently in every multiplayer view.
 * Do not change the Jaguar message commands, pickup logic, or timing. */
static inline uint32_t avp_pc_popup_ink_for_species(int viewing_type)
{
    switch(viewing_type){
    case PT_ALIEN:    return 0x00b860ffu; /* purple */
    case PT_PREDATOR: return 0x00ffff00u; /* yellow */
    case PT_HUMAN:    return 0x0000ff00u; /* green */
    default:          return 0x00ffffffu; /* invalid type: readable fallback */
    }
}

/* Zero-ink original HUD_MSG steps are genuine blink/blank intervals; keep
 * them invisible even though the species's ink would otherwise be nonzero. */
static inline uint32_t avp_pc_popup_ink_for_step(
    int viewing_type,uint32_t source_ink)
{
    return source_ink?avp_pc_popup_ink_for_species(viewing_type):0u;
}

#endif
