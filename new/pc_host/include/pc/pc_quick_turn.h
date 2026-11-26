#ifndef AVP_PC_QUICK_TURN_H
#define AVP_PC_QUICK_TURN_H
#include <stdint.h>

/* PC-only extra action: an interpolated clockwise half turn. The original
 * Jaguar simulation still advances at 10 Hz; host display samples this same
 * monotonic heading at its existing presentation rate. 200 ms = twelve 60-Hz
 * frames, with no invented Jaguar gameplay timing or speed changes. */
#define AVP_PC_QUICK_TURN_HALF_NS UINT64_C(200000000)
#define AVP_PC_QUICK_TURN_HALF UINT32_C(0x80000000)

typedef struct AvpPcQuickTurn {
    uint32_t start_angle;
    int64_t delta;
    uint64_t start_ns, duration_ns;
    int active;
} AvpPcQuickTurn;

/* Exact wraparound target arithmetic (not an instantaneous camera update). */
static inline uint32_t avp_pc_quick_turn_right(uint32_t angle,unsigned presses)
{
    return angle-(uint32_t)((uint64_t)presses*AVP_PC_QUICK_TURN_HALF);
}
static inline uint64_t avp_pc_quick_turn_elapsed(const AvpPcQuickTurn *t,uint64_t now)
{
    if(!t||!t->active||now<=t->start_ns)return 0;
    if(now-t->start_ns>=t->duration_ns)return t->duration_ns;
    return now-t->start_ns;
}
static inline int64_t avp_pc_quick_turn_travelled(const AvpPcQuickTurn *t,uint64_t now)
{
    uint64_t elapsed=avp_pc_quick_turn_elapsed(t,now);
    if(!t||!t->active||!t->duration_ns)return 0;
    if(elapsed==t->duration_ns)return t->delta;
    /* Evaluate the same continuous path on each poll and render. The final
     * sample is handled separately so there is never fractional drift. */
    return (int64_t)((double)t->delta*((double)elapsed/(double)t->duration_ns));
}
static inline uint32_t avp_pc_quick_turn_angle(const AvpPcQuickTurn *t,uint64_t now)
{
    if(!t||!t->active)return 0;
    return t->start_angle+(uint32_t)avp_pc_quick_turn_travelled(t,now);
}
static inline int avp_pc_quick_turn_complete(const AvpPcQuickTurn *t,uint64_t now)
{
    return t&&t->active&&avp_pc_quick_turn_elapsed(t,now)==t->duration_ns;
}
static inline void avp_pc_quick_turn_cancel(AvpPcQuickTurn *t)
{
    if(t)t->active=0;
}
static inline void avp_pc_quick_turn_enqueue(AvpPcQuickTurn *t,
                                              uint32_t current_angle,
                                              uint64_t now,unsigned presses)
{
    int64_t remaining=0;
    uint64_t remaining_ns=0;
    if(!t||!presses)return;
    if(presses>4)presses=4; /* Maximum pending edges per host input batch. */
    if(t->active){
        int64_t traveled=avp_pc_quick_turn_travelled(t,now);
        current_angle=t->start_angle+(uint32_t)traveled;
        remaining=t->delta-traveled;
        remaining_ns=t->duration_ns-avp_pc_quick_turn_elapsed(t,now);
    }
    /* Extend the pending rotation without restarting or skipping the current
     * view. Repeated presses therefore queue successive half turns. */
    t->start_angle=current_angle;
    t->delta=remaining-(int64_t)presses*AVP_PC_QUICK_TURN_HALF;
    t->start_ns=now;
    t->duration_ns=remaining_ns+(uint64_t)presses*AVP_PC_QUICK_TURN_HALF_NS;
    t->active=1;
}
#endif
