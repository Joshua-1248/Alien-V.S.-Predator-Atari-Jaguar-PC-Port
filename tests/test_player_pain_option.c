#include "player.h"
#include "avp_runtime.h"
#include "joypad.h"
#include <assert.h>

/* The PC preference must suppress only the playable character's pain voice.
 * Source-order RNG consumption, visual pain and enemy HumanPain stay original. */
volatile u32 frame_counter;
s16 hugkill;
static unsigned voices;
static void heard(void *user,unsigned id){(void)user;(void)id;voices++;}

static void hit(int species,int enabled,u16 *rng_out,u16 *fade_out)
{
    player_type=(s16)species;
    seed0=0x1234u;seed1=0x5678u;
    player_energy=700;old_energy=800;
    pain=0;fade_c=0x8880u;destruct_flag=0;
    frame_counter+=25u;
    avp_player_set_pain_sounds(enabled);
    voices=0;
    Pain();
    assert(voices==(enabled?1u:0u));
    assert(pain==99);
    *rng_out=seed1;
    *fade_out=fade_c;
}
int main(void)
{
    AvpRuntimeOps o={0};o.play_sfx=heard;
    avp_runtime_bind(&o);
    for(int s=PT_HUMAN;s<=PT_PREDATOR;s+=4){
        u16 rng_on,rng_off,fade_on,fade_off;
        hit(s,1,&rng_on,&fade_on);
        hit(s,0,&rng_off,&fade_off);
        assert(rng_on==rng_off);
        assert(fade_on==fade_off);
        assert(fade_on!=0x8880u);
    }
    /* HumanPain is also called for non-player human actors by AMP.S. */
    voices=0;
    seed0=0x1234;seed1=0x5678;
    avp_player_set_pain_sounds(0);
    HumanPain();
    assert(voices==1u);
    avp_player_set_pain_sounds(1);
    /* Facehugged pain voice can be muted independently of ordinary damage.
     * Suppression must retain original RNG consumption and pain presentation. */
    {
        u16 on_rng,off_rng,on_fade,off_fade;
        hugkill=1;
        avp_player_set_facehug_pain_sounds(1);
        hit(PT_HUMAN,1,&on_rng,&on_fade);
        avp_player_set_facehug_pain_sounds(0);
        seed0=0x1234;seed1=0x5678;
        player_energy=700;old_energy=800;pain=0;fade_c=0x8880u;
        frame_counter+=25u;voices=0;Pain();
        assert(voices==0u);
        off_rng=seed1;off_fade=fade_c;
        assert(on_rng==off_rng && on_fade==off_fade);
        avp_player_set_facehug_pain_sounds(1);hugkill=0;
    }
    avp_runtime_reset_ops();
    return 0;
}
