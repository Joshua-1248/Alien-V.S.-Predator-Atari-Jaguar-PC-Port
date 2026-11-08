/* Included at end of original hud_message.c under the OPTIONAL PC flag. */
#include "../../new/multiplayer/local_mp_hudmsg_native.h"
#include <string.h>

int avp_mp_hudmsg_native_valid(const AvpMpHudMessageState *s)
{
    return s && s->valid==AVP_MP_HUDMSG_MAGIC &&
           s->pickup_card[sizeof(s->pickup_card)-1]=='\0' &&
           s->access_text[sizeof(s->access_text)-1]=='\0' &&
           s->newlevel_text[sizeof(s->newlevel_text)-1]=='\0' &&
           s->newduct_text[sizeof(s->newduct_text)-1]=='\0' &&
           s->countdown_text[sizeof(s->countdown_text)-1]=='\0';
}

int avp_mp_hudmsg_native_capture(AvpMpHudMessageState *out)
{
    AvpMpHudMessageState s={0};
    if(!out)return 0;
    s.valid=AVP_MP_HUDMSG_MAGIC;
    s.msg_info=msg_info;
    s.msg_status=msg_status;
    s.new_message=new_message;
    s.msg_ptr=msg_ptr;
    s.msg_source=msg_source;
    memcpy(s.pickup_msg,pickup_msg,sizeof(pickup_msg));
    memcpy(s.pickup_card,pickup_card,sizeof(pickup_card));
    memcpy(s.access_text,avp_access_text,sizeof(avp_access_text));
    memcpy(s.newlevel_text,avp_newlevel_text,sizeof(avp_newlevel_text));
    memcpy(s.newduct_text,avp_newduct_text,sizeof(avp_newduct_text));
    memcpy(s.countdown_text,avp_countdown_text,sizeof(avp_countdown_text));
    *out=s;
    return 1;
}

int avp_mp_hudmsg_native_restore(const AvpMpHudMessageState *s)
{
    if(!avp_mp_hudmsg_native_valid(s))return 0;
    msg_info=s->msg_info;
    msg_status=s->msg_status;
    new_message=s->new_message;
    msg_ptr=s->msg_ptr;
    msg_source=s->msg_source;
    memcpy(pickup_msg,s->pickup_msg,sizeof(pickup_msg));
    memcpy(pickup_card,s->pickup_card,sizeof(pickup_card));
    memcpy(avp_access_text,s->access_text,sizeof(avp_access_text));
    memcpy(avp_newlevel_text,s->newlevel_text,sizeof(avp_newlevel_text));
    memcpy(avp_newduct_text,s->newduct_text,sizeof(avp_newduct_text));
    memcpy(avp_countdown_text,s->countdown_text,sizeof(avp_countdown_text));
    /* draw_line / clear_message are host renderer callbacks, never rebound. */
    return 1;
}
