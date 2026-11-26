#ifndef AVP_NEW_PAUSE_STATE_H
#define AVP_NEW_PAUSE_STATE_H
/* Last presented gameplay pose and frozen pause dimensions. */
typedef struct AvpNewPauseView {
    const u8 *maze;
    u16 mw,mh;
    u32 x,y,angle;
    u64 vbl;
    unsigned level,width,height;
    int rom_ok,smooth,debug,valid,hires;
    s8 brightness;
} AvpNewPauseView;
#endif
