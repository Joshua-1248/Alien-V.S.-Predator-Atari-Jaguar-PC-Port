#include "player.h"
#include "music.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include <assert.h>
#include <string.h>

/* The 1994 BGSounds calls the ordinary AVPSOUND effect allocator: random
 * ambient one-shots are not free-running host mixer samples. */
static unsigned handled, direct;
static unsigned last_id, last_voice;
static u32 last_handle;
static void handled_play(void *user,unsigned id,u32 handle,unsigned voice)
{
    (void)user;
    handled++;last_id=id;last_handle=handle;last_voice=voice;
}
static void direct_play(void *user,unsigned id)
{
    (void)user;(void)id;direct++;
}
int main(void)
{
    AvpRuntimeOps ops={0};
    ops.play_sfx=direct_play;
    ops.play_sfx_handle=handled_play;
    avp_runtime_bind(&ops);
    avp_music_bind_sfx_meta(NULL,0u);
    InitSynt();
    /* The 68k random routine returns the old seed1.  Force the four
       original ambient branches without modifying the generator itself. */
    for(unsigned r=0;r<4u;r++){
        seed0=0;seed1=(u16)r;
        BGSounds();
        assert(handled==r+1u && direct==0u);
        assert(last_id==(r<2u?AVP_FX_AMB2:AVP_FX_AMB3));
        assert(last_handle!=0u && last_handle!=(u32)-1);
        assert(last_voice>=2u && last_voice<8u);
    }
    seed0=0;seed1=4u;
    BGSounds();
    assert(handled==4u && direct==0u);
    /* The retail bg_fxs table's first entry is NULL: it cannot schedule a
       repeating tone, even on the 1-in-512 random selection. */
    bg_fx=0;
    seed0=0;seed1=0;
    BGSounds();
    assert(bg_fx==0u && bg_repeat==4 && bg_delay==10 && bg_count==1);
    avp_runtime_reset_ops();
    return 0;
}
