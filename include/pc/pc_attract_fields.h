#ifndef AVP_PC_ATTRACT_FIELDS_H
#define AVP_PC_ATTRACT_FIELDS_H

#include "mjp.h"

/* The Jaguar MJP text blitter and OP list are observed at VBlank. During
 * INTRO/ESCAPE, an MJPA sound, MJPT resource identifier or MJPX CPU/GPU
 * progress write is NOT a separate video field. Publishing each of those
 * host callbacks exposed half-updated text/picture states and added a full
 * X11 repaint between the actual One_Tick displays. Keep their data/audio
 * callbacks; defer their presentation to the original wait_vblank path.
 * Do not change TITLE's native Jaguar presentation or other MJP modes. */
static inline int avp_pc_frontend_present_on_event(unsigned mode,unsigned event)
{
    if(mode==AVP_MJP_INTRO || mode==AVP_MJP_ESCAPE)return 0;
    return event!=0x5449434bu; /* One_Tick's TICK was already displayed. */
}

#endif
