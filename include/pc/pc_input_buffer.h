#ifndef AVP_PC_INPUT_BUFFER_H
#define AVP_PC_INPUT_BUFFER_H
#include <stdint.h>
/* Active-low pad: sample input at the host poll rate and hold new press bits
 * until the next original gameplay tick. This is OPTIONAL and deliberately
 * does not increase Jaguar gameplay frequency or fabricate releases. */
static inline void avp_pc_buffer_poll(uint32_t *previous,uint32_t *pending,
                                      uint32_t pad,int enabled)
{
    if(enabled)*pending|=*previous&~pad;
    else *pending=0u;
    *previous=pad;
}
static inline uint32_t avp_pc_buffer_take(uint32_t *pending,uint32_t pad,int enabled)
{
    uint32_t out=enabled?(pad&~*pending):pad;
    *pending=0u;
    return out;
}
#endif
