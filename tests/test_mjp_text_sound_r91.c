/* Reconstructed Show_Tex TFX selection: original 68000 at $01D2FE..$01D376. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/game/mjp.c"
volatile u32 joy_cur=UINT32_MAX;
static unsigned ticks,sounds,glyphs,cursor_events,cursor_end_events,cursor_active;
static unsigned cursor_x[32];
static unsigned press_at_tick;
static void event(void *u,unsigned e,s32 a,s32 b)
{
    (void)u;
    if(e==0x4d4a5041u){assert(a==0x221b8 && b==-1);++sounds;}
    if(e==0x4d4a5043u){assert(b==0 && cursor_events<32u);cursor_x[cursor_events++]=(unsigned)a;cursor_active=1u;}
    if(e==0x4d4a505au){++cursor_end_events;cursor_active=0u;}
}
static void draw(void *u,unsigned line,const char *s,unsigned x)
{
    (void)u;(void)line;(void)s;(void)x;++glyphs;
}
static const AvpRuntimeOps ops={.frontend_event=event,.draw_text=draw};
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}
void VSync(void){++ticks;}
void readpad(void){joy_cur=(press_at_tick && ticks>=press_at_tick)?~(1u<<FIRE_A):UINT32_MAX;}
int main(void)
{
    current_mjp_mode=AVP_MJP_INTRO;
    /* Blank ASCII space and literal '_' are silent, '~' and '@' are
     * converted only AFTER the original sound test. */
    New_Text("A _~@\0\0");Show_Tex();
    assert(ticks==5u && sounds==3u && glyphs==6u);
    assert(cursor_events==5u);
    assert(cursor_end_events==2u && cursor_active==0u);
    assert(cursor_x[0]==7u && cursor_x[1]==14u && cursor_x[2]==21u &&
           cursor_x[3]==28u && cursor_x[4]==31u);
    current_mjp_mode=AVP_MJP_FAME;
    New_Text("ABC\0\0");Show_Tex();
    assert(ticks==5u && sounds==3u);
    assert(cursor_end_events==2u && cursor_active==0u);
    /* The first printable character of a new intro message activates the
     * cursor again; a skipped message MUST retire it immediately. */
    press_at_tick=ticks+1u;
    current_mjp_mode=AVP_MJP_INTRO;
    New_Text("ABCD\0\0");Show_Tex();
    assert(text_abort==1);
    assert(cursor_active==0u && cursor_end_events==4u);
    assert(cursor_events==6u && cursor_x[5]==7u);
    assert(sounds==4u && ticks==6u);
    press_at_tick=0;
    /* A completed message can be followed by a blank or missing text record
     * without restoring the stale square from the previous message. */
    New_Text(NULL);Show_Tex();
    assert(cursor_active==0u && cursor_end_events==5u);
    current_mjp_mode=AVP_MJP_ESCAPE;
    New_Text("ABCDE\0\0");Show_Tex();
    assert(cursor_active==0u && cursor_end_events==7u);
    assert(sounds==9u && ticks==11u);
    puts("R94: retail text TFX dispatch and live-cursor completion/skip: PASS");
    return 0;
}
