#ifndef AVP_HUD_MESSAGE_H
#define AVP_HUD_MESSAGE_H
#include "avp_types.h"

#define AVP_HUDMSG_LINES 3
#define AVP_HUDMSG_STOP 0u
#define AVP_HUDMSG_NEW  0xffu

typedef struct AvpHudMessageStep {
    u32 info;                    /* low byte is command/countdown, upper bytes are colour/control */
    const char *line[AVP_HUDMSG_LINES];
} AvpHudMessageStep;

typedef void (*AvpHudMessageDrawLineFn)(unsigned line, const char *text, unsigned x_pixels);
typedef void (*AvpHudMessageClearFn)(void);
enum AvpCollectNotice;

void InitHUDMsg(void);
void UpdtHUDMsg(void);
void ShowHUDMsg(void);
void avp_hudmsg_queue(const AvpHudMessageStep *steps);
void avp_hudmsg_set_draw_callback(AvpHudMessageDrawLineFn fn);
void avp_hudmsg_set_clear_callback(AvpHudMessageClearFn fn);
void avp_hudmsg_pickup(enum AvpCollectNotice notice,unsigned detail);
const AvpHudMessageStep *avp_hudmsg_current(void);
int avp_hudmsg_critical_active(void);
int avp_hudmsg_countdown_active(void);

extern const AvpHudMessageStep avp_msg_dead[];
extern const AvpHudMessageStep avp_msg_pressure[];
extern const AvpHudMessageStep avp_msg_notsecure[];
extern const AvpHudMessageStep avp_msg_airlocked[];
extern const AvpHudMessageStep avp_msg_jammed[];
extern const AvpHudMessageStep avp_msg_jlift[];
extern const AvpHudMessageStep avp_msg_escape[];
extern const AvpHudMessageStep avp_msg_usecocoon[];
extern const AvpHudMessageStep avp_msg_stun[],avp_msg_cocooned[],avp_msg_ccnfail[];
extern const AvpHudMessageStep avp_msg_cheat_on[],avp_msg_cheat_off[];
extern const AvpHudMessageStep avp_msg_gain_combi[],avp_msg_lose_combi[];
extern const AvpHudMessageStep avp_msg_gain_disc[],avp_msg_lose_disc[];
extern const AvpHudMessageStep avp_msg_gain_cannon[],avp_msg_lose_cannon[];
extern AvpHudMessageStep avp_msg_newlevel[];
extern AvpHudMessageStep avp_msg_newduct[];
void avp_hudmsg_set_destination_level(unsigned level);
extern const AvpHudMessageStep avp_msg_access_denied[];
extern AvpHudMessageStep avp_msg_countdown[];
extern char avp_countdown_text[];
void avp_hudmsg_set_access_code(unsigned code);

/* Original HUD_MSG.S message commands pack RGB24 into bits 31..8;
 * the low byte is the message timing opcode. */
u32 avp_hudmsg_rgb24(void);

extern u32 msg_info;
extern u8 msg_status;

#endif
