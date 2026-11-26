/* ROM-free synthesis integration test: same generated FM effect, different
 * multiplayer spatial source tags.  No changes to single-player DSP code. */
#define _DEFAULT_SOURCE 1
#ifndef AVP_LOCAL_MULTIPLAYER
#define AVP_LOCAL_MULTIPLAYER 1
#endif
#include "../new/jaguar/host/fullsynth_host.c"
#include <assert.h>
#include <stdio.h>
static u8 fake_rdb[0x5000],fake_env[0x4000];
u32 x_pos,y_pos,centre_angle;
int avp_resources_decode_rdb(const AvpRomImage *rom,u8 **data,size_t *n)
{
    (void)rom;*data=malloc(sizeof fake_rdb);if(!*data)return 0;
    memcpy(*data,fake_rdb,sizeof fake_rdb);*n=sizeof fake_rdb;return 1;
}
int avp_resources_decode_rdb_text(const AvpRomImage *rom,u8 **data,size_t *n)
{
    (void)rom;*data=malloc(sizeof fake_env);if(!*data)return 0;
    memcpy(*data,fake_env,sizeof fake_env);*n=sizeof fake_env;return 1;
}
int avp_resources_rom_descriptor(const AvpRomImage *rom,u32 addr,const u8 **data,size_t *n)
{
    (void)rom;(void)addr;(void)data;(void)n;return 0;
}
static unsigned long long energy(const int16_t *pcm,size_t count)
{
    unsigned long long sum=0;
    for(size_t i=0;i<count;i++)sum+=pcm[i]<0?-(int)pcm[i]:pcm[i];
    return sum;
}
static unsigned long long playback(const AvpHostedVoice *original,
    const AvpMpSpatialListener l[4])
{
    int16_t pcm[4096];
    for(unsigned i=0;i<AVP_HOST_SFX_SLOTS;i++)if(i!=9u)
        memset(&F.v[i],0,sizeof F.v[i]);
    F.v[9]=*original;
    avp_jaguar_fullsynth_mp_set_listeners(l,0u);
    avp_jaguar_fullsynth_mix(pcm,2048u);
    return energy(pcm,4096u);
}
int main(void)
{
    AvpRomImage rom={0};
    const u32 addr=AVP_FX_NEWAL;
    const unsigned env_off=0x9e98u-0x8000u+0xf1cc50u-0xf1bc00u;
    AvpMpSpatialListener l[4]={{1u,3u,0,0,0u,1u},
                               {1u,4u,0,0,0u,2u},{0},{0}};
    AvpHostedVoice original;
    unsigned long long near,far,wrong,personal_other,personal_spy,human,global;
    u8 *p=fake_rdb+(addr-0x20300u);
    put_be32(p,4u);put_be32(p+4u,0xf1de00u);
    put_be32(p+8u,0xf1de00u);put_be32(p+16u,0x28f00u);
    put_be32(p+24u,0x80004000u);put_be32(p+36u,0x1164u);
    put_be32(p+40u,0xf1cc50u);put_be32(p+44u,0x60u);
    put_be32(p+48u,0x130000u);put_be32(p+52u,0x130000u);
    put_be32(p+60u,0x130000u);put_be32(p+76u,0x3fffu);
    put_be32(fake_env+env_off,0x7fffu);put_be32(fake_env+env_off+4u,0x7fffu);
    assert(avp_jaguar_fullsynth_init(&rom));
    /* A real enemy has neither player-1 identity nor a self-owner shortcut. */
    avp_jaguar_fullsynth_mp_set_world_sound_source(3u,0,0,17u);
    avp_jaguar_fullsynth_play(addr,0,0);
    original=F.v[9];
    assert(original.mp_source_active &&
           original.mp_source_player==AVP_LOCAL_MP_MAX_ACTORS+17u);
    /* Only the original emitting creature updates its loop source. */
    avp_jaguar_fullsynth_mp_set_world_sound_source(3u,2<<16,0,17u);
    assert(F.v[9].mp_source_x==(2<<16));
    avp_jaguar_fullsynth_mp_set_world_sound_source(3u,4<<16,0,18u);
    assert(F.v[9].mp_source_x==(2<<16));
    avp_jaguar_fullsynth_mp_set_world_sound_source(4u,8<<16,0,17u);
    assert(F.v[9].mp_source_x==(2<<16));
    avp_jaguar_fullsynth_mp_set_world_sound_source(3u,0,0,17u);
    assert(F.v[9].mp_source_x==0);
    near=playback(&original,l);
    assert(near>0u);
    l[0].level=4u;
    wrong=playback(&original,l);
    assert(wrong==0u); /* Sublevel 3 cannot bleed into Sublevel 4. */
    l[0].level=3u;l[0].x=16<<16;
    far=playback(&original,l);
    assert(far==0u);
    l[0].x=0;
    /* Human effects must be positional even when NO bots are enabled. */
    avp_jaguar_fullsynth_mp_set_sound_source(0u,3u,0,0);
    avp_jaguar_fullsynth_play(addr,0,0);
    original=F.v[10];
    assert(original.mp_source_player==0u);
    human=playback(&original,l);
    assert(human>0u);
    /* A contact cue must not be heard merely for being on the same map. */
    avp_jaguar_fullsynth_mp_set_personal_cue(1);
    avp_jaguar_fullsynth_mp_set_sound_source(1u,3u,0,0);
    avp_jaguar_fullsynth_play(addr,0,0);
    original=F.v[11];
    assert(original.mp_source_player==1u && original.mp_personal_cue);
    personal_other=playback(&original,l);
    assert(personal_other==0u);
    l[0].subject_plus_one=2u; /* P1 spies on P2 */
    personal_spy=playback(&original,l);
    assert(personal_spy>0u);
    /* An unsourced interface effect is still global; music/UI untouched. */
    original.mp_personal_cue=0u;original.mp_source_active=0u;
    l[0].level=15u;l[1].level=15u;
    global=playback(&original,l);
    assert(global>0u);
    avp_jaguar_fullsynth_shutdown();
    puts("PASS fullsynth R126: PCM world source level filtering, human SFX, personal spycam acid, original unsourced UI");
    return 0;
}
