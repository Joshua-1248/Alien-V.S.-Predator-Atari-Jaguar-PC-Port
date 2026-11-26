#ifndef AVP_NEW_ATOMIC_OVERLAY_H
#define AVP_NEW_ATOMIC_OVERLAY_H
/* Overlay and software frame must be in a single drawable transaction. */
static inline int avp_new_atomic_overlay_required(int fps,int scoreboard)
{return fps || scoreboard;}
#endif
