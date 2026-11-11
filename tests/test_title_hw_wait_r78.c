/* PC-only title loop: source-derived 30/260 VBlank waits allow Fire skips,
 * while standalone original-C library still uses its 141-tick contract. */
#define AVP_PC_HOST 1
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/game/main_game.c"
volatile u32 one_second_ticks=60u;
volatile u32 joy_cur=0xffffffffu;
s16 player_type=PT_ALIEN;
s16 player_dead,launch_flag;
s32 Choice,score;
s8 grunke;
u8 cartcopy[AVP_EE_SIZE];
uintptr_t savegame;
static unsigned vblanks,menu_calls,events,skips,read_count,select_calls;
static int press_during_wait;
static void fe(void *u,unsigned e,s32 a,s32 b)
{
    (void)u;(void)b;
    if(e==0x5449544cu)events++;
    if(e==AVP_PC_TITLE_SKIP_EVENT){
        assert(a==225 || a==485); ++skips;
    }
}
const AvpRuntimeOps *avp_runtime_ops(void)
{
    static AvpRuntimeOps ops;
    ops.frontend_event=fe;
    return &ops;
}
void VSync(void){++vblanks;}
void readpad(void)
{
    ++read_count;
    joy_cur=0xffffffffu;
    if(press_during_wait && vblanks>=195u)
        joy_cur&=~(1u<<FIRE_A);
}
void InitMJP(void){}
void SetScreen(u32 w,u32 h){(void)w;(void)h;}
void StartMus(unsigned id){assert(id==AVP_MUSIC_TITLE3);}
void avp_mjp_prepare_title(void){}
void MTEST(AvpMjpMode mode)
{
    if(mode==AVP_MJP_TITLE){
        ++menu_calls;
        if(!press_during_wait)assert(vblanks==AVP_PC_TITLE_SEQUENCE_FIELDS);
        else assert(vblanks<AVP_PC_TITLE_SEQUENCE_FIELDS);
        Choice=0;
    }else if(mode==AVP_MJP_SELECT){++select_calls;Choice=PT_HUMAN;}
    else assert(!"unexpected MJP mode");
}
void End_Sele(void){}
void InitFile(void){}
void WaitFXFa(void){}
void StopMusi(void){}
void Trash_EE(void){}
int main(void)
{
    NewTitle();
    assert(menu_calls==1u && select_calls==1u && skips==0u);
    assert(vblanks==AVP_PC_TITLE_SEQUENCE_FIELDS+1u);
    assert(read_count>1u); /* two real source waits, no menu Fire gate */
    vblanks=menu_calls=events=skips=read_count=select_calls=0u;
    press_during_wait=1;
    NewTitle();
    assert(menu_calls==1u && select_calls==1u && skips==2u);
    assert(vblanks<250u); /* both 30- and 260-field waits skip */
    puts("R78 original title wait counts, interruptible Fire and menu handoff PASS");
    return 0;
}
