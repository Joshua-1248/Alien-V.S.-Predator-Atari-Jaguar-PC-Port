/* Full MAIN title -> menu handoff with no input, using the production
 * NewTitle() control flow. Test-only stubs stand in for platform services. */
#include <assert.h>
#include <stdint.h>
#include "../src/game/main_game.c"

volatile u32 one_second_ticks=60u;
volatile u32 joy_cur=0xffffffffu;
s16 player_type=PT_ALIEN;
s16 player_dead,launch_flag;
s32 Choice,score;
s8 grunke;
u8 cartcopy[AVP_EE_SIZE];
uintptr_t savegame;
static unsigned vblanks,read_count,menu_calls,select_calls,initial_events,menu_vblanks;
static unsigned title_resources,screen_320x280;
static unsigned seen_title;
static void test_frontend(void *u,unsigned e,s32 a,s32 b)
{
    (void)u;(void)a;(void)b;
    if(e==0x5449544cu){assert(!seen_title);seen_title=1u;++initial_events;}
    if(e==0x5449434bu){assert(seen_title);}
}
const AvpRuntimeOps *avp_runtime_ops(void)
{
    static AvpRuntimeOps ops;
    ops.frontend_event=test_frontend;
    return &ops;
}
void VSync(void){++vblanks;}
void readpad(void){++read_count;joy_cur=0xffffffffu;}
void InitMJP(void){}
void SetScreen(u32 w,u32 h){if(w==320u&&h==280u)++screen_320x280;}
void StartMus(unsigned id){assert(id==AVP_MUSIC_TITLE3);}
void avp_mjp_prepare_title(void){++title_resources;}
void MTEST(AvpMjpMode mode)
{
    if(mode==AVP_MJP_TITLE){
        assert(seen_title && vblanks==141u && read_count==1u);
        ++menu_calls;menu_vblanks=vblanks;Choice=0;
    }else if(mode==AVP_MJP_SELECT){
        assert(menu_calls==1u);++select_calls;Choice=PT_HUMAN;
    }else assert(!"unexpected front-end mode");
}
void End_Sele(void){}
void InitFile(void){}
void WaitFXFa(void){}
void StopMusi(void){}
void Trash_EE(void){}
int main(void)
{
    NewTitle();
    assert(title_resources==1u&&initial_events==1u);
    assert(menu_calls==1u&&select_calls==1u);
    assert(menu_vblanks==141u&&vblanks==142u);
    assert(read_count==1u); /* OPTION probe, no extra Fire gate */
    assert(screen_320x280==0u); /* NTSC 320x240 menu */
    assert(player_type==PT_HUMAN && in_select==0xffu);
    return 0;
}
