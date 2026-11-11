/* RE36/R100: original MUSIC.o signed-D1 override reaches the hosted DSP.
 * Synthetic 98-module RDB fixture: no original cartridge data is embedded. */
#include "music.h"
#include "avp_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define RDB_SIZE 0x2868u
static u8 rdb[RDB_SIZE];
static unsigned regular_calls,override_calls,stops,chosen_voice;
static u32 last_handle;
static s32 last_d1;
static unsigned last_id;

static void play_original(void *u,unsigned id,u32 handle,unsigned v)
{
    (void)u;
    ++regular_calls;last_id=id;last_handle=handle;chosen_voice=v;
}
static void play_overridden(void *u,unsigned id,u32 handle,unsigned v,s32 d1)
{
    (void)u;
    ++override_calls;last_id=id;last_handle=handle;chosen_voice=v;last_d1=d1;
}
static void stop_handle(void*u,u32 handle){(void)u;(void)handle;++stops;}
static void clear(void*u){(void)u;}
static void reset_counts(void)
{
    regular_calls=override_calls=stops=0u;
    last_id=last_handle=chosen_voice=0u;last_d1=-1;
}
static void initialize_modules(void)
{
    unsigned i;
    memset(rdb,0,sizeof(rdb));
    for(i=0;i<98u;i++){
        size_t off=(size_t)(0x20cc8u-0x20300u)+80u*i;
        assert(off+80u<=sizeof(rdb));
        rdb[off+3]=32u; rdb[off+6]=0x2au; /* Type-32, default $2A00 */
    }
    {   /* Original ZERO voice uses a separate, higher $6000 default. */
        size_t off=0x21718u-0x20300u;
        rdb[off+6]=0x60u;
    }
    avp_music_bind_retail_sfx_descriptors(rdb,sizeof(rdb));
}
int main(void)
{
    static const unsigned spoken_five_to_one[]={0x22848u,0x22898u,0x228e8u,0x22938u,0x22988u};
    AvpRuntimeOps ops;
    unsigned i;
    memset(&ops,0,sizeof(ops));
    ops.play_sfx_handle=play_original;
    ops.play_sfx_handle_d1=play_overridden;
    ops.stop_sfx=stop_handle;ops.kill_sounds=clear;
    avp_runtime_bind(&ops);
    initialize_modules();
    InitSynt();reset_counts();

    /* All five spoken commands send explicit D1=$2600 to the *same*
     * CPU-allocated slot and hosted synthesizer. They do not fall back to
     * their $2A00 module defaults. */
    for(i=0;i<5u;i++){
        assert(avp_music_tfx_d1(spoken_five_to_one[i],0x2600)!=(u32)-1);
        assert(override_calls==i+1u && regular_calls==0u);
        assert(last_id==spoken_five_to_one[i]);
        assert(last_d1==0x2600 && chosen_voice==2u+i && last_handle!=0u);
    }

    /* The retail ZERO instruction instead passes D1=-1; module $6000
     * remains its source-authored volume. */
    assert(avp_music_tfx_d1(0x21718u,-1)!=(u32)-1);
    assert(regular_calls==1u && override_calls==5u);
    assert(last_id==0x21718u && chosen_voice==7u);

    /* With six occupied sampler voices at $2A00, an arriving $2600
     * cannot steal any. Original BGT is STRICT, and CPU D1 (not module
     * default) is used to arbitrate priority. */
    InitSynt();reset_counts();
    for(i=0;i<6u;i++)assert(TFX(spoken_five_to_one[0])!=(u32)-1);
    assert(regular_calls==6u && override_calls==0u);
    assert(avp_music_tfx_d1(spoken_five_to_one[1],0x2600)==(u32)-1);
    assert(regular_calls==6u && override_calls==0u);
    assert(avp_music_tfx_d1(spoken_five_to_one[1],0x2a00)==(u32)-1);
    assert(regular_calls==6u && override_calls==0u);
    assert(avp_music_tfx_d1(spoken_five_to_one[1],0x3000)!=(u32)-1);
    assert(override_calls==1u && last_d1==0x3000 && stops==1u);

    /* Ordinary pre-existing APIs must keep their original behaviour. */
    InitSynt();reset_counts();
    assert(TFX(spoken_five_to_one[0])!=(u32)-1);
    assert(DoEffect(spoken_five_to_one[0])!=(u32)-1);
    assert(regular_calls==2u && override_calls==0u);
    avp_music_bind_retail_sfx_descriptors(NULL,0u);
    avp_runtime_reset_ops();
    puts("R100: TFX signed-D1 DSP override, voice arbitration, original ZERO default: PASS");
    return 0;
}
