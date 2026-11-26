/* R399: exercise actual PC logical allocator and host DSP mirror together. */
#include "music.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include "../sound_channels.h"
#include "../../jaguar/host/fullsynth_host.c"
#include <assert.h>
static u8 fixture[0x3000], samples[4096];
u32 x_pos,y_pos,centre_angle;
int avp_resources_decode_rdb(const AvpRomImage *rom,u8 **data,size_t *n)
{(void)rom;*data=malloc(sizeof fixture);if(!*data)return 0;memcpy(*data,fixture,sizeof fixture);*n=sizeof fixture;return 1;}
int avp_resources_decode_rdb_text(const AvpRomImage *rom,u8 **data,size_t *n)
{(void)rom;(void)data;(void)n;return 0;}
int avp_resources_rom_descriptor(const AvpRomImage *rom,u32 addr,const u8 **data,size_t *n)
{(void)rom;(void)addr;(void)data;(void)n;return 0;}
static void play(void *u,unsigned id,u32 h,unsigned v)
{(void)u;avp_jaguar_fullsynth_play_handle(id,h,v,0,0);}
static void stop(void *u,u32 h){(void)u;avp_jaguar_fullsynth_stop_handle(h);}
static void kill(void *u){(void)u;avp_jaguar_fullsynth_stop_all();}
static int active(void *u,u32 h){(void)u;return avp_jaguar_fullsynth_handle_active(h);}
static int voice_active(void *u,unsigned v){(void)u;return avp_jaguar_fullsynth_voice_active(v);}
static void unloop(void *u,u32 h){(void)u;avp_jaguar_fullsynth_unloop_handle(h);}
static void mod(void *u,u32 h,s32 n){(void)u;avp_jaguar_fullsynth_mod_handle(h,n);}
static void resize(unsigned n){avp_music_set_sound_channels(n);avp_jaguar_fullsynth_set_sound_channels(n);}
int main(void)
{
    const unsigned id=0x20cc8u;
    AvpRomImage rom={0}; AvpRuntimeOps ops={0}; u32 handles[256]; int16_t pcm[16];
    rom.data=samples;rom.size=sizeof samples;memset(samples,50,sizeof samples);
    for(unsigned i=0;i<98u;i++){
        u8 *p=fixture+id-0x20300u+80u*i;
        put_be32(p,32u);put_be32(p+4u,0x7fffu);put_be32(p+8u,0x800000u);
        put_be32(p+12u,256u);put_be32(p+16u,0x3ffffu);put_be32(p+76u,0x4000u);
    }
    ops.play_sfx_handle=play;ops.stop_sfx=stop;ops.kill_sounds=kill;
    ops.sfx_handle_active=active;ops.sfx_voice_active=voice_active;ops.unloop_sfx=unloop;ops.mod_sfx=mod;
    avp_runtime_bind(&ops);avp_music_bind_retail_sfx_descriptors(fixture,sizeof fixture);
    assert(avp_jaguar_fullsynth_init(&rom));InitSynt();
    assert(F.active_slots==AVP_HOST_SFX_SLOTS);
    assert(avp_sound_channels_valid(0)==8u && avp_sound_channels_valid(255)==8u);
    assert(avp_sound_channels_adjust(256u,1)==256u && avp_sound_channels_adjust(8u,-1)==8u);
    for(unsigned count=8u;count<=256u;count+=8u){
        resize(count);InitSynt();
        for(unsigned i=2u;i<count;i++){
            handles[i]=TFX(id);assert(handles[i]!=(u32)-1);
            assert(avp_jaguar_fullsynth_handle_active(handles[i]));
            assert(F.v[host_voice_slot(i)].game_voice==i);
        }
        assert(TFX(id)==(u32)-1);
        assert(F.active_slots==AVP_HOST_SFX_SLOTS+count-8u);
        avp_jaguar_fullsynth_mix(pcm,8u);
        assert(pcm[0]>0 && pcm[1]>0);
        if(count==256u)assert(pcm[0]==32767 && pcm[1]==32767);
    }
    ModSfx(handles[255],0);assert(F.v[host_voice_slot(255)].gain_l==0);
    KillSfx(handles[255]);assert(!active(NULL,handles[255]));
    handles[255]=TFX(id);assert(active(NULL,handles[255]));
    u32 weapon=avp_music_placed_voice(id,1u);assert(active(NULL,weapon));
    resize(16u);assert(active(NULL,weapon)&&active(NULL,handles[15])&&!active(NULL,handles[16])&&!active(NULL,handles[255]));
    resize(256u);assert(TFX(id)!=(u32)-1);
    resize(8u);assert(active(NULL,weapon)&&F.active_slots==AVP_HOST_SFX_SLOTS);
    KillAll();assert(!active(NULL,weapon));
    resize(24u);InitSynt();
    for(unsigned i=0;i<20u;i++)avp_jaguar_fullsynth_play(id,0,0);
    unsigned playing=0;for(unsigned i=0;i<F.active_slots;i++)playing+=!!F.v[i].synth.active;
    assert(playing==20u);
    KillAll();avp_jaguar_fullsynth_shutdown();
    puts("R399 logical channels 8..256, handle lifecycle, wide mix, raw overlap: PASS");return 0;
}
