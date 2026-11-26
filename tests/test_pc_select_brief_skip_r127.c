/* R127: a NEW Fire during hosted character briefing skips the remaining
 * text and host-only hold, then proceeds directly into the retail FX fade. */
#include "mjp.h"
#include "avp_runtime.h"
#include "joypad.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

volatile u32 joy_edge,joy2_edge,joy_cur,joy2_cur;
volatile s8 joy_special_mode,reset_enabled,pad_flag2,pad_flag3;
volatile u16 reset_wait;
volatile u32 reset_time,frame_counter;
void (*x_read)(void);
u32 one_second_ticks=60u;
static unsigned vblanks,fade_calls,prompts,sounds;
static s32 fade_value;
void VSync(void){++vblanks;}
void readpad(void){joy_cur=vblanks>=2u?~(1u<<FIRE_A):0xffffffffu;}
void FXFadeDo(s32 v){++fade_calls;fade_value=v;}
static void record(void *u,unsigned event,s32 a,s32 b)
{
    (void)u;(void)a;(void)b;
    if(event==0x53454c54u)++prompts;
    if(event==0x53454c53u)++sounds;
}
int main(void)
{
    AvpRuntimeOps ops;
    memset(&ops,0,sizeof ops);
    ops.frontend_event=record;
    avp_runtime_bind(&ops);
    joy_cur=0xffffffffu;
    avp_mjp_select_request_briefing_skip();
    End_Sele();
    assert(vblanks==0u && prompts==0u && sounds==1u);
    assert(fade_calls==1u && fade_value==0xfau);
    /* Skip state must not carry into the next SELECT confirmation. */
    joy_cur=0xffffffffu;
    End_Sele();
    assert(vblanks==2u && prompts==1u && sounds==2u);
    assert(fade_calls==2u && fade_value==0xfau);
    puts("PASS: R127 SELECT fresh-press skip -> original loading fade; normal confirmation retained");
    return 0;
}
