/* R98: Model TYP1B's original 68000 priority arbitration with the
 * user-verified Marine escape-report (one free voice) as the reference.
 * All fixtures are synthetic; no cartridge PCM is embedded. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "music.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define RDB_SIZE 0x2868u
static u8 rdb[RDB_SIZE];
static u32 live[8];
static unsigned until[8],frame;
static unsigned accepted[32],accepted_voice[32],accepted_n;
static void clear_audio(void *u){(void)u;memset(live,0,sizeof(live));}
static void stop(void *u,u32 handle)
{
    (void)u;
    for(unsigned v=0;v<8;v++)if(live[v]==handle)live[v]=0;
}
static void play(void *u,unsigned id,u32 handle,unsigned v)
{
    (void)u;
    assert(id==AVP_FX_TYP1B && v<8 && accepted_n<32u);
    live[v]=handle;until[v]=frame+5u;
    accepted[accepted_n]=frame;
    accepted_voice[accepted_n++]=v;
}
static int active(void *u,u32 handle)
{
    (void)u;
    for(unsigned v=0;v<8;v++)if(live[v]==handle){
        if(frame>=until[v]){live[v]=0;return 0;}
        return 1;
    }
    return 0;
}
static unsigned run(unsigned count,unsigned intro)
{
    memset(live,0,sizeof(live));
    InitSynt();ProtectV(count);
    avp_music_set_intro_typ1b_single_lane((int)intro);
    accepted_n=frame=0;
    for(frame=0;frame<24u;frame++)(void)TFX(AVP_FX_TYP1B);
    avp_music_set_intro_typ1b_single_lane(0);
    return accepted_n;
}
int main(void)
{
    AvpRuntimeOps ops;
    memset(&ops,0,sizeof(ops));
    ops.play_sfx_handle=play;ops.sfx_handle_active=active;
    ops.stop_sfx=stop;ops.kill_sounds=clear_audio;
    avp_runtime_bind(&ops);
    for(unsigned i=0;i<98u;i++){
        const unsigned off=(AVP_FX_TRAC1-0x20300u)+i*80u;
        assert(off+80u<=RDB_SIZE);
        rdb[off+3]=32; /* original sampler class */
        rdb[off+6]=0x10; /* priority $1000, all TYP1B starts equal */
    }
    avp_music_bind_retail_sfx_descriptors(rdb,sizeof(rdb));
    /* Marine good ending: original postgame ProtectV(7), one free voice. */
    assert(run(7u,0u)==5u);
    for(unsigned k=0;k<accepted_n;k++){
        assert(accepted[k]==k*5u);
        assert(accepted_voice[k]==7u);
    }
    /* Normal original title reservation: six free CPU effect voices. */
    assert(run(2u,0u)==24u);
    /* PC-only intro correction: exact same audible-start cadence and voice
     * as the user-confirmed good ending, without changing 68000 ProtectV. */
    assert(run(2u,1u)==5u);
    for(unsigned k=0;k<accepted_n;k++){
        assert(accepted[k]==k*5u);
        assert(accepted_voice[k]==7u);
    }
    /* Flag must be scoped: after Intro, default host 6-voice allocation
     * must still work for normal effects and future gameplay. */
    assert(run(2u,0u)==24u);
    avp_runtime_reset_ops();
    puts("R98: escape-report and intro TYP1B accepted starts match: PASS");
    return 0;
}
