#ifndef AVP_LOCAL_MP_PAUSE_H
#define AVP_LOCAL_MP_PAUSE_H
#include <stdint.h>
/* Co-op host scheduler pause. Rising-edge only; a held button cannot
 * immediately resume a pause. No per-player world is advanced while paused. */
typedef struct AvpMpPause {
    uint32_t prior_held;
    int paused;
} AvpMpPause;
static inline int avp_mp_pause_step(AvpMpPause *s,uint32_t current_held)
{
    const uint32_t rising=current_held & ~s->prior_held;
    s->prior_held=current_held;
    if(rising)s->paused=!s->paused;
    return rising!=0u;
}
#endif
