#include "amp_free_list.h"
#include "amp.h"
#include <stdint.h>

int avp_game_micro_amp_free_list_valid(const u16 *list,unsigned count)
{
    uint64_t seen[(AVP_NUM_AMPS + 63u) / 64u] = {0};
    if(count>AVP_NUM_AMPS || (count && !list))return 0;
    for(unsigned i=0;i<count;i++){
        unsigned slot=list[i];
        uint64_t bit;
        if(slot>=AVP_NUM_AMPS)return 0;
        bit=UINT64_C(1) << (slot & 63u);
        if(seen[slot>>6] & bit)return 0;
        seen[slot>>6] |= bit;
    }
    return 1;
}
