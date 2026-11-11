/* Original MAIN title -> idle attract -> ROM title replay -> menu -> selection.
 * This is the production NewTitle() function, exercised with fake VBlanks. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#define AVP_PC_HOST 1
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/game/main_game.c"

volatile u32 one_second_ticks=60u;
volatile u32 joy_cur=UINT32_MAX;
s16 player_type=PT_ALIEN;
s16 player_dead,launch_flag;
s32 Choice,score;
s8 grunke;
u8 cartcopy[AVP_EE_SIZE];
uintptr_t savegame;
static unsigned vblanks,logos,menus,intros,selects,resources,music_starts;
static unsigned first_menu_vblanks,second_menu_vblanks;
static void test_frontend(void *u,unsigned e,s32 a,s32 b)
{
    (void)u;(void)a;(void)b;
    if(e==0x5449544cu){ ++logos; assert(logos<=2u); }
    if(e==0x5449434bu)assert(logos!=0u);
}
const AvpRuntimeOps *avp_runtime_ops(void)
{
    static AvpRuntimeOps ops;
    ops.frontend_event=test_frontend;
    return &ops;
}
void VSync(void){++vblanks; assert(vblanks<1200u);}
void readpad(void){joy_cur=UINT32_MAX;}
void InitMJP(void){}
void SetScreen(u32 w,u32 h){(void)w;(void)h;}
void StartMus(unsigned id){assert(id==AVP_MUSIC_TITLE3);++music_starts;}
void avp_mjp_prepare_title(void){++resources;}
void MTEST(AvpMjpMode mode)
{
    if(mode==AVP_MJP_TITLE){
        ++menus;
        assert(logos==menus);
        if(menus==1u){first_menu_vblanks=vblanks;Choice=4;}
        else {assert(menus==2u);second_menu_vblanks=vblanks;Choice=0;}
    }else if(mode==AVP_MJP_INTRO){
        assert(menus==1u);++intros;
    }else if(mode==AVP_MJP_SELECT){
        assert(menus==2u&&intros==1u);++selects;Choice=PT_HUMAN;
    }else assert(!"Unexpected MJP mode");
}
void End_Sele(void){}
void InitFile(void){}
void WaitFXFa(void){}
void StopMusi(void){}
void Trash_EE(void){}
int main(void)
{
    NewTitle();
    assert(logos==2u && menus==2u && intros==1u && selects==1u);
    /* Only the original title boot starts the score; the attract returns
     * through native logo replay without restarting the live music. */
    assert(resources==2u && music_starts==1u);
    assert(first_menu_vblanks==AVP_PC_TITLE_SEQUENCE_FIELDS);
    assert(second_menu_vblanks==2u*AVP_PC_TITLE_SEQUENCE_FIELDS);
    assert(in_select==0xffu && player_type==PT_HUMAN);
    puts("R91: attract exit replays original title before menu: PASS");
    return 0;
}
