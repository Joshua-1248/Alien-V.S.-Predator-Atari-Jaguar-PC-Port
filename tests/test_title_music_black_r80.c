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
static unsigned music_starts;
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
void StartMus(unsigned id){assert(id==AVP_MUSIC_TITLE3);++music_starts;}
void avp_mjp_prepare_title(void){}
void MTEST(AvpMjpMode mode)
{
    if(mode==AVP_MJP_TITLE){
        ++menu_calls;
        if(!press_during_wait)assert(vblanks==menu_calls*(AVP_PC_TITLE_SEQUENCE_FIELDS+1u)-1u);
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
    assert(AVP_PC_TITLE_MUSIC_START_FIELD == 43u);
    assert(AVP_PC_TITLE_MUSIC_START_FIELD < AVP_PC_TITLE_COLD_BOOT_FIELDS);
    assert(music_starts == 0u);
    /* Host plays the retail score against black at model field 43. */
    avp_pc_title_music_prestart();
    assert(music_starts == 1u);
    /* NewTitle must consume the one-shot marker, not restart the tune. */
    NewTitle();
    assert(music_starts == 1u);
    assert(menu_calls == 1u && select_calls == 1u);
    assert(vblanks == AVP_PC_TITLE_SEQUENCE_FIELDS + 1u);
    /* Future visits to title must follow retail MAIN.S StartMus normally. */
    NewTitle();
    assert(music_starts == 2u);
    assert(menu_calls == 2u && select_calls == 2u);
    puts("R80 title soundtrack starts during black, without restart, and resets on subsequent titles PASS");
    return 0;
}
