/* R272: retail DoEffect ($9CD4-$9CDA) compares a new effect's priority with
 * the occupied type-4 voice's STORED word, which TFX wrote as D1 LSR.W #2
 * ($9D3C-$9D42).  A sampler cue therefore steals a busy FM voice whenever
 * its priority exceeds a quarter of the FM module word.  Synthetic RDB only. */
#include "music.h"
#include "avp_sfx_ids.h"
#include "avp_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned plays;
static void play_handle(void*u,unsigned id,u32 h,unsigned v){(void)u;(void)id;(void)h;(void)v;++plays;}
static int busy(void*u,unsigned v){(void)u;(void)v;return 1;}
static void nop_stop(void*u,u32 h){(void)u;(void)h;}

int main(void)
{
    static u8 retail[0x2868u];
    AvpRuntimeOps o;unsigned i;
    memset(&o,0,sizeof(o));
    o.play_sfx_handle=play_handle;o.sfx_voice_active=busy;o.stop_sfx=nop_stop;o.unloop_sfx=nop_stop;
    avp_runtime_bind(&o);
    for(i=0;i<98u;i++){
        size_t off=(size_t)(AVP_FX_TRAC1-0x20300u)+(size_t)i*80u;
        retail[off+3u]=32u;retail[off+6u]=0x30u;     /* sampler, priority $3000 */
    }
    {   /* FM module word $4100 at +$18: stored in the voice as $1040. */
        size_t off=AVP_FX_TRAC1-0x20300u;
        retail[off+3u]=4u;retail[off+6u]=0u;
        retail[off+26u]=0x41u;
    }
    avp_music_bind_retail_sfx_descriptors(retail,sizeof(retail));
    InitSynt();ProtectV(2u);
    for(i=0;i<6u;i++)assert(TFX(AVP_FX_TRAC1)!=(u32)-1);
    /* $3000 > $1040 (quartered FM word): retail steals the busy FM voice.
     * Comparing against the unshifted $4100 would wrongly refuse it. */
    assert(TFX(AVP_FX_SHOTG)!=(u32)-1);
    /* A cue at or below the stored $1040 still cannot steal (BGT is strict). */
    {   size_t off=AVP_FX_TRAC2-0x20300u;retail[off+6u]=0x10u;retail[off+7u]=0x40u; }
    avp_music_bind_retail_sfx_descriptors(retail,sizeof(retail));
    InitSynt();ProtectV(2u);
    for(i=0;i<6u;i++)assert(TFX(AVP_FX_TRAC1)!=(u32)-1);
    assert(TFX(AVP_FX_TRAC2)==(u32)-1);
    avp_music_bind_retail_sfx_descriptors(NULL,0u);
    puts("R272 DoEffect type-4 stored-priority steal PASS");
    return 0;
}
