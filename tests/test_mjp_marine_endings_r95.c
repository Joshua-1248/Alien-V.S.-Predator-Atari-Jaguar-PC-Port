/* RE35 R95: instruction-backed Marine-specific ending source regression.
 * No proprietary ROM contents are compiled into or redistributed with tests.
 * Verified against the user-owned retail AVP.jag RAM 68000 routines:
 * Do_Lose $01DA20..$01DACC and Do_Escap $01DB92..$01E236. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/game/mjp.c"
volatile u32 joy_cur=UINT32_MAX;
volatile u32 one_second_ticks=60u;
static unsigned ticks,test_phase,first_cue_count,voice_notes,typing_notes;
static jmp_buf done;
static unsigned cue_id[16],text_id[16],cue_tick[16],cue_count,text_count;
static s32 cue_d1[16];
static unsigned cursor_line[32],cursor_count,draw_line[32],draw_x[32],draw_count;
static unsigned seq_type[64],seq_id[64],seq_count;
static void event(void *u,unsigned e,s32 a,s32 b){
    (void)u;
    if(e==0x4d4a5041u){
        if((u32)a==0x221b8u){++typing_notes;return;}
        if(seq_count<64u){seq_type[seq_count]=1u;seq_id[seq_count++]=(u32)a;}
        if(cue_count<16u){cue_id[cue_count]=(u32)a;cue_tick[cue_count]=ticks;cue_d1[cue_count++]=b;}
        ++voice_notes;
        if(test_phase==1u&&(u32)a==0x22848u&&++first_cue_count==2u)
            longjmp(done,1);
    }else if(e==0x4d4a5054u){
        if(seq_count<64u){seq_type[seq_count]=2u;seq_id[seq_count++]=(u32)a;}
        if(text_count<16u)text_id[text_count++]=(u32)a;
    }else if(e==0x4d4a5043u){
        if(cursor_count<32u)cursor_line[cursor_count++]=(unsigned)b;
    }
}
static const char *text_resource(void *u,u32 address){
    (void)u;
    static const char short_text[]="A\0\0";
    static const char base_report[]="A\0B\0C\0D\0\0";
    return address==0x2b036u?base_report:short_text;
}
static void draw(void *u,unsigned line,const char *s,unsigned x){
    (void)u;
    if(s[0]!='_'&&draw_count<32u){
        draw_line[draw_count]=line;
        draw_x[draw_count++]=x;
    }
}
static const AvpRuntimeOps ops={.frontend_event=event,.frontend_text=text_resource,.draw_text=draw};
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}
void VSync(void){++ticks;}
void readpad(void){
    joy_cur=(test_phase==2u&&ticks>=750u)?~(1u<<FIRE_A):UINT32_MAX;
}
int main(void){
    const unsigned cues[6]={0x22848u,0x22898u,0x228e8u,0x22938u,0x22988u,0x21718u};
    const unsigned ids[6]={0x2addau,0x2ae01u,0x2ae28u,0x2ae4fu,0x2ae76u,0x2ae9du};
    const unsigned rows[6]={0u,1u,2u,0u,1u,0u};
    /* Use the retail-style padded word to catch the R95 defect: silence
     * also selects instantaneous Show_Tex, including all 32 spaces. */
    current_mjp_mode=AVP_MJP_ESCAPE;
    mjp_suppress_typing_sound=1u;
    New_Text("                Five                \0\0");
    Show_Tex();
    assert(ticks==0u && cursor_count==0u && typing_notes==0u);
    test_phase=1u;
    for(volatile unsigned pass=0u;pass<2u;pass++){
        one_second_ticks=(pass==0u)?60u:50u;
        ticks=first_cue_count=cue_count=text_count=cursor_count=draw_count=0u;
        voice_notes=typing_notes=seq_count=0u;
        if(setjmp(done)==0){Do_Escap();assert(0&&"first escape pass should repeat");}
        assert(cue_count>=6u && text_count>=8u);
        for(unsigned i=1u;i<6u;i++){
            assert(cue_tick[i]-cue_tick[i-1u]==one_second_ticks);
        }
        for(unsigned i=0u;i<6u;i++){
            assert(cue_id[i]==cues[i] && text_id[i]==ids[i]);
            assert(cue_d1[i]==(i<5u?0x2600:-1));
            assert(draw_line[i]==rows[i]);
        }
        /* Retail $01D3FE skips cursor and VBlank while $4BEC8=1. */
        assert(cursor_count==2u); /* only trailing nonspoken reports */
        unsigned next=0u;
        for(unsigned i=0;i<seq_count&&next<6u;i++){
            if(seq_type[i]==1u && seq_id[i]==cues[next]){
                assert(i+1u<seq_count && seq_type[i+1u]==2u && seq_id[i+1u]==ids[next]);
                ++next;
            }
        }
        assert(next==6u);
        assert(typing_notes==2u);
    }
    one_second_ticks=60u;
    /* Bad ending uses a separate report and a distinct six-hundred-tick hold. */
    test_phase=2u;ticks=0u;draw_count=0u;cursor_count=0u;typing_notes=0u;
    current_mjp_mode=AVP_MJP_BASE_EXPLODES;
    mjp_suppress_typing_sound=0u;mjp_escape_text_line=0u;
    Do_Lose();
    assert(text_id[text_count-1u]==0x2b036u);
    assert(draw_count>=4u);
    for(unsigned i=0;i<4u;i++)assert(draw_line[i]==i && draw_x[i]==24u);
    assert(typing_notes==0u);
    assert(ticks>=750u);
    puts("PASS: 60/50-field spoken countdown cue cadence, original-row placement, silent instantaneous words, report TYP1B, separate bad ending");
    return 0;
}
