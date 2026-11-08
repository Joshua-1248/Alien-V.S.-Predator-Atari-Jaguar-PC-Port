#ifndef AVP_PC_ORIGINAL_CHEATS_H
#define AVP_PC_ORIGINAL_CHEATS_H
#include "avp_types.h"
/* Host-only menu adapter. Acts on the *currently scoped* original actor.
 * Calling this does not alter the retail MAZE/PLAYER/MAZESCRN code. */
void avp_pc_original_cheat_apply(unsigned actions);
u32 avp_pc_original_cheat_alien_pad(u32 pad);
/* Three original coordinate displays; fine position is the fixed-point
 * location in 1/256-cell increments, naturally spanning 16-bit hex. */
void avp_pc_original_cheat_gps_format(char *out,unsigned capacity,
    unsigned level,u32 x,u32 y);
#endif
