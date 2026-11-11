#ifndef AVP_PC_FRONTEND_FIELD_CLOCK_H
#define AVP_PC_FRONTEND_FIELD_CLOCK_H

/* Original Jaguar MJP delays count VIDEO FIELDS, not elapsed host redraw
 * time. The PC must finish a field before the next field's deadline rather
 * than adding a full-field sleep after potentially slow X11 composition.
 * This is independent of the ROM-executed TITLE-logo clock. */
#include "mjp.h"
#include "pc/pc_title_reference.h"
#include <stdint.h>

static inline int avp_pc_mjp_field_paced(unsigned mode)
{
    return mode==AVP_MJP_TITLE || mode==AVP_MJP_INTRO ||
           mode==AVP_MJP_ESCAPE || mode==AVP_MJP_SIM_TERMINATED ||
           mode==AVP_MJP_BASE_EXPLODES;
}

static inline uint64_t avp_pc_mjp_next_deadline(uint64_t previous,
                                                uint64_t now,
                                                unsigned fields_per_second)
{
    if(fields_per_second!=50u)
        return avp_pc_title_next_deadline(previous,now);
    if(previous==0u || now>previous+1000000000ull ||
       previous>now+1000000000ull)
        return now+20000000ull;
    return previous+20000000ull;
}

/* Independent, modal original COMPUTER.S VBlank clock.  The caller owns
 * and resets its separate deadline on CPAL entry/exit; it must never share
 * a deadline with the title, normal gameplay, or another terminal session. */
static inline uint64_t avp_pc_computer_next_deadline(uint64_t previous,
                                                     uint64_t now,
                                                     unsigned fields_per_second)
{
    return avp_pc_mjp_next_deadline(previous,now,fields_per_second);
}
#endif
