#ifndef AVP_LOCAL_MP_HUDMSG_NATIVE_H
#define AVP_LOCAL_MP_HUDMSG_NATIVE_H
#include "hud_message.h"
#include <stdint.h>

/* Original HUD_MSG.S message cursor and all mutable message backing text.
 * The pointers address stable source-owned command arrays in the SAME process.
 * Never serialize these snapshots or copy renderer callback pointers.
 * The countdown belongs to a world in the finished multiplayer scheduler;
 * this structure currently isolates its displayed message per actor only. */
#define AVP_MP_HUDMSG_MAGIC UINT32_C(0x484D5347)
typedef struct AvpMpHudMessageState {
    uint32_t valid;
    u32 msg_info;
    u8 msg_status;
    const AvpHudMessageStep *new_message, *msg_ptr, *msg_source;
    AvpHudMessageStep pickup_msg[7];
    char pickup_card[16];
    char access_text[13];
    char newlevel_text[11];
    char newduct_text[11];
    char countdown_text[10];
} AvpMpHudMessageState;
int avp_mp_hudmsg_native_valid(const AvpMpHudMessageState *state);
int avp_mp_hudmsg_native_capture(AvpMpHudMessageState *out);
int avp_mp_hudmsg_native_restore(const AvpMpHudMessageState *in);
#endif
