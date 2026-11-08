#include "hud.h"
#include "hud_message.h"
#include "avp_runtime.h"
#include "player.h"
#include "hud_score.h"
#include <assert.h>
#include <string.h>

/* Isolated original-HUD CPU checks; only the original code paths are run. */
static unsigned sound_ticks,alarm_on,scope_events;
static unsigned event_flags;
static void event(void *ctx,unsigned e,u32 a,u32 b,u32 c)
{
    (void)ctx;(void)a;(void)b;
    if(e==0x201u){scope_events++;event_flags=c;}
}
const AvpRuntimeOps *avp_runtime_ops(void)
{
    static AvpRuntimeOps ops;
    ops.object_event=event;
    return &ops;
}
void PREDScore(void){}
void avp_hudmsg_queue(const AvpHudMessageStep *steps){(void)steps;}
const AvpHudMessageStep *avp_hudmsg_current(void){return NULL;}
int avp_hudmsg_countdown_active(void){return 0;}
AvpHudMessageStep avp_msg_countdown[]={{0,{NULL,NULL,NULL}}};
char avp_countdown_text[10]="TIME 1:00";
u8 msg_status;
u16 xtra_c;
s16 game_over;
static void count_sound(AvpCountdownCue cue){(void)cue;sound_ticks++;}
static void alarm(int start){alarm_on=(unsigned)start;}

int main(void)
{
    s16 a,b;
    avp_hud_set_pred_meter_samples(8192,0); /* (sample << 6) >> 16 = 8 */
    HP2();
    avp_hud_pred_meter_span(0,&a,&b);
    assert(a==0&&b==4);
    avp_hud_pred_meter_span(1,&a,&b);
    assert(a==4&&b==8);
    avp_hud_set_pred_meter_samples(0,0);
    HP2();
    avp_hud_pred_meter_span(0,&a,&b);
    assert(a==8&&b==4);
    avp_hud_pred_meter_span(1,&a,&b);
    assert(a==4&&b==0);

    scope_events=0u;
    avp_hud_set_tracker_distance(10u<<16);
    TC();
    assert(scope_events==1u&&event_flags==3u);
    /* HUD.S::TC: CLR.W/SWAP leaves the integer click range; one
     * ADD.L #$2F supplies the envelope-rate baseline. */
    assert(tracker_audio_state.env_rate==0x2fu+5u);
    assert(tracker_audio_state.pitch==0xa7000u+(5u<<14));
    assert(tracker_audio_state.volume==0x200u+(5u<<8));
    avp_hud_set_tracker_distance(15u<<16);
    TC();
    assert(scope_events==2u&&event_flags==3u);
    TC();
    assert(scope_events==3u&&event_flags==2u);

    avp_hud_set_clock(0u,10u);
    avp_hud_set_countdown_callbacks(count_sound,alarm);
    InitCountdown();
    assert(counttime==121&&ticktime==20u&&alarm_on==1u);
    assert(xtra_c==0xf180u);
    Countdown();
    assert(xtra_c==0xf194u&&flash_dir==20);
    Countdown();
    assert(xtra_c==0xf1a8u&&flash_dir==-20);
    Countdown();
    assert(xtra_c==0xf194u&&flash_dir==-20);
    assert(counttime==121&&game_over==0&&sound_ticks==0);
    return 0;
}
