#ifndef AVP_GAME_MICRO_AMP_FREE_LIST_H
#define AVP_GAME_MICRO_AMP_FREE_LIST_H
#include "avp_types.h"
/* Read-only validation of the original AMP allocator free-slot array.
 * Equivalent to checking every entry against all predecessors, but visits
 * each slot once using a 320-bit stack-local bitmap (300 legal slots).
 * No allocator bookkeeping, allocation order, or RNG behavior is changed. */
int avp_game_micro_amp_free_list_valid(const u16 *list,unsigned count);
#endif
