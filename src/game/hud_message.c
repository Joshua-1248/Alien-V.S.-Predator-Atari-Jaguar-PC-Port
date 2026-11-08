/* Readable C reconstruction of MAZE/HUD_MSG.S.
 *
 * The Jaguar source stores each step as a 32-bit info word followed by three
 * 32-bit text pointers.  This C form makes that record explicit while
 * preserving the original one-frame MSG_NEW display handshake and countdown
 * state machine.  Blitter/font drawing is a backend callback here.
 */
#include "hud_message.h"
#include "collectables.h"
#include <stdio.h>
#include <stddef.h>

#define HUD_CHAR_WIDTH 13u
#define MSG_WIDTH      192u

u32 msg_info;
u8 msg_status;

static const AvpHudMessageStep *new_message;
static const AvpHudMessageStep *msg_ptr;
static const AvpHudMessageStep *msg_source;
static AvpHudMessageDrawLineFn draw_line;
static AvpHudMessageClearFn clear_message;
static char pickup_card[16]="CARD #10";
static AvpHudMessageStep pickup_msg[7];

u32 avp_hudmsg_rgb24(void)
{
    return (msg_info >> 8) & 0x00ffffffu;
}

static unsigned text_pixel_width(const char *s)
{
    unsigned n=0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n*HUD_CHAR_WIDTH;
}

static unsigned centred_x(const char *s)
{
    unsigned w=text_pixel_width(s);
    return (w>=MSG_WIDTH) ? 0u : (MSG_WIDTH-w)/2u;
}

void avp_hudmsg_set_draw_callback(AvpHudMessageDrawLineFn fn) { draw_line=fn; }
void avp_hudmsg_set_clear_callback(AvpHudMessageClearFn fn) { clear_message=fn; }
const AvpHudMessageStep *avp_hudmsg_current(void) { return msg_ptr; }
int avp_hudmsg_critical_active(void)
{
    return new_message==avp_msg_dead || new_message==avp_msg_countdown ||
           (msg_status!=AVP_HUDMSG_STOP &&
            (msg_source==avp_msg_dead || msg_source==avp_msg_countdown));
}

/* The message pointer advances through the countdown's timing/colour
 * records after its opening text step. The source record identity does
 * not change until the sequence is replaced or completes. */
int avp_hudmsg_countdown_active(void)
{
    /* HUD.S:405 CMP.L #cdm_stop,msg_ptr.  Retail msg_ptr points past the
     * current record, so this is true only while the $0000FF00|$7f record
     * is counting; this port's msg_ptr addresses that record itself. */
    return msg_ptr==avp_msg_countdown+2;
}

void InitHUDMsg(void)
{
    new_message=NULL;
    msg_ptr=NULL;
    msg_source=NULL;
    if(clear_message) clear_message();
    msg_info=0;
    msg_status=AVP_HUDMSG_STOP;
}

void avp_hudmsg_queue(const AvpHudMessageStep *steps)
{
    /* Historical new_message is only consumed by UpdtHUDMsg, immediately
     * before the frame in which ShowHUDMsg observes MSG_NEW. */
    new_message=steps;
}

void UpdtHUDMsg(void)
{
    if (new_message) {
        msg_ptr=new_message;
        msg_source=new_message;
        new_message=NULL;
    } else {
        if (msg_status==AVP_HUDMSG_STOP) return;
        --msg_status;
        if (msg_status!=AVP_HUDMSG_STOP) return;
        if (!msg_ptr) return;
        ++msg_ptr;
    }

    if (!msg_ptr) {
        msg_info=0;
        msg_status=AVP_HUDMSG_STOP;
        return;
    }
    msg_info=msg_ptr->info;
    msg_status=(u8)msg_info;
}

void ShowHUDMsg(void)
{
    unsigned i;
    if (msg_status!=AVP_HUDMSG_NEW || !msg_ptr) return;
    /* A new HUD_MSG command clears the full text brush, including lines
       unused by the replacement message. Do not depend on pointer identity:
       countdown and repeated card pickups reuse the same command record. */
    if(clear_message) clear_message();
    if (draw_line) {
        for (i=0;i<AVP_HUDMSG_LINES;++i) {
            const char *s=msg_ptr->line[i];
            if (s) draw_line(i,s,centred_x(s));
        }
    }
    /* In assembly msg_ptr is advanced past three line pointers here, then
     * msg_status=1 so UpdtHUDMsg advances to the next command next frame.
     * Our explicit-record representation advances on that next update. */
    msg_status=1;
}

#define INFO_NEW(rgb) ((u32)(rgb) | AVP_HUDMSG_NEW)
#define INFO_WAIT(rgb,n) ((u32)(rgb) | (u8)(n))
#define END_STEP { AVP_HUDMSG_STOP,{NULL,NULL,NULL} }

/* These sequences are direct structural transcriptions of the corresponding
 * HUD_MSG.S command streams.  Null-line timing records preserve the original
 * coloured/blank flash cadence even though host drawing is delegated. */
const AvpHudMessageStep avp_msg_dead[]={
    {INFO_NEW(0x00ff0000u),{NULL,"GAME OVER",NULL}},
    {INFO_WAIT(0x00ff0000u,254),{NULL,NULL,NULL}}, END_STEP
};
const AvpHudMessageStep avp_msg_pressure[]={
    {INFO_NEW(0xffff0000u),{"AIRLOCK","PRESSURISING",NULL}},
    {INFO_WAIT(0xffff0000u,4),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}}, END_STEP
};
const AvpHudMessageStep avp_msg_notsecure[]={
    {INFO_NEW(0xffffff00u),{"CLOSE OTHER","AIRLOCK DOOR","FIRST"}},
    {INFO_WAIT(0xffffff00u,4),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffffff00u,5),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffffff00u,5),{NULL,NULL,NULL}}, END_STEP
};
const AvpHudMessageStep avp_msg_airlocked[]={
    {INFO_NEW(0xffff0000u),{"AIRLOCK","NOT IN USE",NULL}},
    {INFO_WAIT(0xffff0000u,4),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}}, END_STEP
};
const AvpHudMessageStep avp_msg_jlift[]={
    {INFO_NEW(0x00ff0000u),{NULL,"LIFT JAMMED",NULL}},
    {INFO_WAIT(0x00ff0000u,4),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0x00ff0000u,5),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0x00ff0000u,5),{NULL,NULL,NULL}}, END_STEP
};
const AvpHudMessageStep avp_msg_jammed[]={
    {INFO_NEW(0x00ff0000u),{NULL,"DOOR JAMMED",NULL}},
    {INFO_WAIT(0x00ff0000u,4),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0x00ff0000u,5),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0x00ff0000u,5),{NULL,NULL,NULL}}, END_STEP
};
static char avp_access_text[]="SECURITY #00";
void avp_hudmsg_set_access_code(unsigned code){if(code>99u)code=99u;avp_access_text[10]=(char)('0'+(code/10u));avp_access_text[11]=(char)('0'+(code%10u));}
const AvpHudMessageStep avp_msg_access_denied[]={
    {INFO_NEW(0xff000000u),{avp_access_text,"ACCESS","DENIED"}},
    {INFO_WAIT(0xff000000u,4),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffffff00u,5),{NULL,NULL,NULL}},{INFO_WAIT(0xff000000u,5),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffffff00u,5),{NULL,NULL,NULL}},{INFO_WAIT(0xff000000u,5),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffffff00u,5),{NULL,NULL,NULL}},{INFO_WAIT(0xff000000u,5),{NULL,NULL,NULL}}, END_STEP
};


const AvpHudMessageStep avp_msg_usecocoon[]={
    {INFO_NEW(0xffff0000u),{"COCOON","HATCHING",NULL}},
    {INFO_WAIT(0xffff0000u,9),{NULL,NULL,NULL}},
    {INFO_WAIT(0x00000000u,10),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffff0000u,10),{NULL,NULL,NULL}},
    {INFO_WAIT(0x00000000u,10),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffff0000u,10),{NULL,NULL,NULL}}, END_STEP
};

/* HUD_MSG.S: source-authoritative species and Predator-honour notices.
 * The repeated wait/blank/color intervals are game ticks, not host frames. */
#define COCOON_FLASH(text) \
    {INFO_NEW(0xffff0000u),{"COCOON",text,NULL}}, \
    {INFO_WAIT(0xffff0000u,4),{NULL,NULL,NULL}}, \
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}}, \
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}}, END_STEP
const AvpHudMessageStep avp_msg_stun[]={COCOON_FLASH("THE ENEMY")};
const AvpHudMessageStep avp_msg_cocooned[]={COCOON_FLASH("COMPLETED")};
const AvpHudMessageStep avp_msg_ccnfail[]={COCOON_FLASH("FAILED")};
#undef COCOON_FLASH
const AvpHudMessageStep avp_msg_cheat_on[]={
    {INFO_NEW(0x00ff0000u),{NULL,"CHEAT ON",NULL}},
    {INFO_WAIT(0x00ff0000u,5),{NULL,NULL,NULL}},END_STEP
};
const AvpHudMessageStep avp_msg_cheat_off[]={
    {INFO_NEW(0x00ff0000u),{NULL,"CHEAT OFF",NULL}},
    {INFO_WAIT(0x00ff0000u,5),{NULL,NULL,NULL}},END_STEP
};
#define PRED_FLASH(color,first,second,third) \
    {INFO_NEW(color),{first,second,third}}, \
    {INFO_WAIT(color,6),{NULL,NULL,NULL}}, \
    {7,{NULL,NULL,NULL}},{INFO_WAIT(color,7),{NULL,NULL,NULL}}, \
    {7,{NULL,NULL,NULL}},{INFO_WAIT(color,7),{NULL,NULL,NULL}},END_STEP
const AvpHudMessageStep avp_msg_gain_combi[]={PRED_FLASH(0xffff0000u,"COMBI STICK","ATTAINED",NULL)};
const AvpHudMessageStep avp_msg_lose_combi[]={PRED_FLASH(0xffff0000u,"COMBI STICK","LOST",NULL)};
const AvpHudMessageStep avp_msg_gain_disc[]={PRED_FLASH(0xffff0000u,"SMART DISC","ATTAINED",NULL)};
const AvpHudMessageStep avp_msg_lose_disc[]={PRED_FLASH(0xffff0000u,"SMART DISC","LOST",NULL)};
const AvpHudMessageStep avp_msg_gain_cannon[]={PRED_FLASH(0xffffff00u,"SHOULDER","CANNON","ATTAINED")};
const AvpHudMessageStep avp_msg_lose_cannon[]={PRED_FLASH(0xffffff00u,"SHOULDER","CANNON","LOST")};
#undef PRED_FLASH

static char avp_newlevel_text[]="SUBLEVEL ?";
static char avp_newduct_text[]="AIRDUCT ??";
AvpHudMessageStep avp_msg_newlevel[]={
    {INFO_NEW(0xffff0000u),{"ENTERING",avp_newlevel_text,NULL}},
    {INFO_WAIT(0xffff0000u,4),{NULL,NULL,NULL}},
    {INFO_WAIT(0x00000000u,5),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}},
    {INFO_WAIT(0x00000000u,5),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}}, END_STEP
};
AvpHudMessageStep avp_msg_newduct[]={
    {INFO_NEW(0xffff0000u),{"ENTERING",avp_newduct_text,NULL}},
    {INFO_WAIT(0xffff0000u,4),{NULL,NULL,NULL}},
    {INFO_WAIT(0x00000000u,5),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}},
    {INFO_WAIT(0x00000000u,5),{NULL,NULL,NULL}},
    {INFO_WAIT(0xffff0000u,5),{NULL,NULL,NULL}}, END_STEP
};
void avp_hudmsg_set_destination_level(unsigned level)
{
    static const char duct_ids[8][2]={{'1','B'},{'2','B'},{'3','B'},{'3','C'},{'4','B'},{'4','C'},{'5','B'},{'5','C'}};
    if(level>=6u && level<=13u){
        avp_newduct_text[8]=duct_ids[level-6u][0];
        avp_newduct_text[9]=duct_ids[level-6u][1];
    }else{
        unsigned n=level>9u?9u:level;
        avp_newlevel_text[9]=(char)('0'+n);
    }
}

const AvpHudMessageStep avp_msg_escape[]={
    {INFO_NEW(0xffffff00u),{"ESCAPE","CAPSULE",NULL}},
    {INFO_WAIT(0xffffff00u,4),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffffff00u,5),{NULL,NULL,NULL}},
    {5,{NULL,NULL,NULL}},{INFO_WAIT(0xffffff00u,5),{NULL,NULL,NULL}}, END_STEP
};

char avp_countdown_text[]="TIME 1:00";
AvpHudMessageStep avp_msg_countdown[]={
    {INFO_NEW(0xffffff00u),{"EVACUATE","BASE",avp_countdown_text}},
    {INFO_WAIT(0xffffff00u,4),{NULL,NULL,NULL}},
    {INFO_WAIT(0x0000ff00u,0x7f),{NULL,NULL,NULL}}, END_STEP
};

/* Pickup HUD_MSG records are reconstructed from the cartridge's compressed
   RDB.DTA, loaded at $20300.  The 12 distinct command streams occupy
   $25086..$25265; their length-prefixed strings are at $25266..$252D7.
   Their three-line layouts and 4/5-tick colour/blank sequence are part of the
   original presentation, not a generic host pickup banner. */
void avp_hudmsg_pickup(enum AvpCollectNotice notice,unsigned detail)
{
    const char *line0=NULL,*line1=NULL,*line2=NULL;
    u32 ink=0xffffff00u,hold=0xffffff00u;
    switch(notice){
    case AVP_CNOTICE_CARD:
        if(detail<1u||detail>10u)return;
        (void)snprintf(pickup_card,sizeof(pickup_card),"CARD #%02u",detail);
        line0="SECURITY";line1=pickup_card;line2="COLLECTED";ink=hold=0xffffff00u;break;
    case AVP_CNOTICE_SHOTGUN:
        line0="SHOTGUN";line1="COLLECTED";ink=hold=0xffff0000u;break;
    case AVP_CNOTICE_SHOT_AMMO:
        line0="SHOTGUN";line1="ROUNDS";line2="COLLECTED";break;
    case AVP_CNOTICE_PULSE:
        line0="PULSE RIFLE";line1="COLLECTED";ink=hold=0xffff0000u;break;
    case AVP_CNOTICE_PULSE_AMMO:
        line0="PULSE RIFLE";line1="ROUNDS";line2="COLLECTED";break;
    case AVP_CNOTICE_FLAME:
        line0="FLAME";line1="THROWER";line2="COLLECTED";break;
    case AVP_CNOTICE_FLAME_AMMO:
        line0="GAS";line1="COLLECTED";ink=hold=0xffff0000u;break;
    case AVP_CNOTICE_SMART:
        line0="SMARTGUN";line1="COLLECTED";ink=hold=0xffff0000u;break;
    case AVP_CNOTICE_SMART_AMMO:
        line0="SMARTGUN";line1="AMMO";line2="COLLECTED";break;
    case AVP_CNOTICE_MTRACKER:
        line0="MOTION";line1="TRACKER";line2="COLLECTED";break;
    case AVP_CNOTICE_FOOD:
        line0="FOOD";line1="COLLECTED";ink=0xffff0000u;break;
    case AVP_CNOTICE_MEDAID:
        line0="MEDKIT";line1="COLLECTED";ink=hold=0xffff0000u;break;
    default:return;
    }
    /* PLAYER.S give_energy/give_wep/give_ammo and MAZESCRN.S PredAvail write
     * new_message unconditionally: a pickup replaces even the countdown/dead
     * message (HUD.S Countdown re-queues only once the pickup finishes). */
    pickup_msg[0].info=ink|AVP_HUDMSG_NEW;
    pickup_msg[0].line[0]=line0;
    pickup_msg[0].line[1]=line1;
    pickup_msg[0].line[2]=line2;
    /* Cartridge HUD_MSG: initial bright text, four hold ticks, alternating
       five dark/bright ticks twice, and one terminating zero command. */
    pickup_msg[1].info=hold|4u;
    pickup_msg[2].info=5u;
    pickup_msg[3].info=hold|5u;
    pickup_msg[4].info=5u;
    pickup_msg[5].info=hold|5u;
    pickup_msg[6].info=AVP_HUDMSG_STOP;
    for(unsigned i=1u;i<7u;i++)
        for(unsigned j=0u;j<AVP_HUDMSG_LINES;j++)pickup_msg[i].line[j]=NULL;
    avp_hudmsg_queue(pickup_msg);
}

#ifdef AVP_LOCAL_MULTIPLAYER_HUDMSG_BRIDGE
#include "../../new/multiplayer/local_mp_hudmsg_native.inc"
#endif
