#include "player.h"
#include "joypad.h"
#include "amp.h"
#include "avp_runtime.h"
#include "music.h"
#include "avp_sfx_ids.h"
#include <assert.h>
static unsigned cues,unloops;
static u32 started[32],released[32];
static unsigned char active[8];
volatile u32 joy_edge,joy_cur;
u8 cur_weps;
s16 new_wepno;
static void sound_start(void *user,unsigned id,u32 handle,unsigned voice)
{
    (void)user;
    assert(id==AVP_FX_VSX20);
    assert(handle!=0u && handle!=(u32)-1);
    assert(voice<8u && cues<32u);
    started[cues++]=handle;
    active[voice]=1;
}
static void sound_unloop(void *user,u32 handle)
{
    (void)user;
    assert(unloops<32u && handle==started[unloops]);
    released[unloops++]=handle;
    /* Model the DSP voice finishing its released loop before the next
     * AVPSOUND allocation: no additional loop may remain after release. */
    for(unsigned i=2;i<8u;i++)active[i]=0;
}
static int voice_active(void *user,unsigned voice)
{
    (void)user;
    return voice<8u ? active[voice]!=0 : 0;
}
int main(void)
{
    AvpRuntimeOps ops={0};
    ops.play_sfx_handle=sound_start;
    ops.unloop_sfx=sound_unloop;
    ops.sfx_voice_active=voice_active;
    avp_runtime_bind(&ops);
    InitSynt();
    invis_act=255;invis_stat=0;invisflag=0;
    predo=0;predo_count=0;xtra_c=0x8880;pred_vision_vmode=0;
    joy_edge=0xffffffffu;
    do_predo();
    assert(invis_stat==0 && xtra_c==0x8880 && !invisflag && pred_vision_vmode==0);
    /* The HUD first completes its fade. */
    invis_stat=1;
    do_predo();
    assert(invis_stat==2 && invisflag==(1u<<AMP_INVHIT));
    assert(predo_count==3 && xtra_c==0x8880 && pred_vision_vmode==0x06c7u && cues==1);
    do_predo();assert(predo_count==2 && xtra_c==0x9558 && pred_vision_vmode==0x06c7u);
    do_predo();assert(predo_count==1 && xtra_c==0x0cf8 && pred_vision_vmode==0x0ec3u);
    do_predo();assert(predo_count==0 && xtra_c==0x8880 && pred_vision_vmode==0x0ec3u);
    for(int i=0;i<20;i++){
        do_predo();
        assert(predo_count==0 && xtra_c==0x8880 && pred_vision_vmode==0x0ec3u && cues==1);
    }
    /* Jaguar keypad 6 requests the next three-frame vision transition. */
    joy_edge&=~(1u<<KEY_6);
    do_predo();
    assert(predo_count==2 && xtra_c==0x26f8 && pred_vision_vmode==0x06c7u && cues==2);
    assert(unloops==1u && released[0]==started[0]);
    joy_edge=0xffffffffu;
    do_predo();assert(predo_count==1 && xtra_c==0x8880 && pred_vision_vmode==0x06c7u);
    do_predo();assert(predo_count==0 && xtra_c==0x9558 && pred_vision_vmode==0x06c7u);
    do_predo();assert(xtra_c==0x9558 && cues==2);
    /* Consecutive keypad-6 advances traverse all five original
     * {TOM VMODE, X_FADE} pairs; index 2 and index 4 deliberately
     * share $8880 but do NOT share the same hardware video format. */
    {static const u16 expected_extra[4]={0x26f8u,0x0cf8u,0x8880u,0x8880u};
     static const u16 expected_vmode[4]={0x06c7u,0x0ec3u,0x06c7u,0x0ec3u};
     for(unsigned k=0;k<4u;k++){
         joy_edge=~(1u<<KEY_6);
         do_predo();
         joy_edge=0xffffffffu;
         do_predo();do_predo();
         assert(predo_count==0);
         assert(xtra_c==expected_extra[k]);
         assert(pred_vision_vmode==expected_vmode[k]);
     }
     assert(cues==6 && unloops==5);
    }
    player_type=PT_PREDATOR;
    /* The original Option handler does not clear the AMP_INVHIT bit early.
     * The flag remains until do_predo completes the timed decloak. */
    joy_edge=~(1u<<OPTION);
    PlayerWeapons();
    assert(!invis_act && invis_stat==1 && invisflag && predo_count==2);
    /* Decloaking releases the ongoing loop and starts the SAME VSX20
     * cue for the transition, immediately unlooping that new voice. */
    assert(cues==7u && unloops==7u && released[5]==started[5]);
    assert(released[6]==started[6]); /* decloak one-shot unlooped */
    joy_edge=0xffffffffu;
    do_predo();assert(invisflag && predo_count==1);
    do_predo();assert(invisflag && predo_count==0);
    do_predo();assert(invis_stat==0 && !invisflag && xtra_c==0x8880 && pred_vision_vmode==0);
    for(unsigned i=0;i<10u;i++)do_predo();
    assert(cues==7u && unloops==7u);
    /* A new cloak after a completed decloak must allocate a fresh handle,
     * not try to reuse/release the finished voice. */
    invis_act=255u;invis_stat=1;
    do_predo();
    assert(cues==8u && unloops==7u && started[7]!=started[6]);
    invis_act=0u;
    do_predo();
    assert(cues==9u && unloops==9u && released[7]==started[7] && released[8]==started[8]);
    avp_runtime_reset_ops();
    return 0;
}
