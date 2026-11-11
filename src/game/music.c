/* Readable reconstruction of the ordinary-68000 AVPSOUND/music.o control
 * layer.  The retail DSP/FullSynth program is a separate processor domain,
 * but voice reservation, effect handles, loop release, priority stealing and
 * transfer-file caching are CPU responsibilities and are kept here. */
#include "music.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include <stdint.h>
#include <string.h>

#define AVP_SOUND_VOICES 8u
#define AVP_TNF_CACHE 9u
#define AVP_DEFAULT_PROTECTED 2u
#define AVP_ALARM_VOICE 2u
#define AVP_AMBIENT_VOICE 0u
#define AVP_SOUND_TYPE_STOPPED (-4)

typedef struct AvpVoiceState {
    s32 type;                 /* retail voice long +0; -4 means stop/free */
    s16 priority;             /* low word of retail priority/control long */
    u8 looping;
    u8 reserved;
    u32 handle;               /* VIdent[] entry */
    unsigned sound_id;
    s32 mod_value;
} AvpVoiceState;

typedef struct AvpTnfCacheEntry {
    const AvpTransferFile *file;
    const void *copy;
} AvpTnfCacheEntry;

s16 fx_fade_level=0x7fff,fx_fade_target=0x7fff;
u32 current_music,alarm_id,ambient_id;

static AvpVoiceState voice[AVP_SOUND_VOICES];
static u32 ident;
static unsigned protected_voices=AVP_DEFAULT_PROTECTED;
#ifdef AVP_PC_HOST
/* The PC currently hosts the music sequencer separately from general SFX,
 * so Intro's six nominally unprotected effect lanes can overlap one TYP1B
 * each glyph. The correctly sounding Marine escape-report path has only
 * one effective lane. Until shared JERRY music/effect voice ownership can
 * be reconstructed completely, match that user-verified single-lane
 * TYP1B behavior in INTRO *without* changing the original 68000
 * ProtectV(2) CPU state or any other sound effect. */
static int host_intro_typ1b_single_lane;
void avp_music_set_intro_typ1b_single_lane(int enabled)
{
    host_intro_typ1b_single_lane=!!enabled;
}
#endif
static const AvpSoundMeta *sound_meta;
static unsigned sound_meta_count;
static AvpTnfCacheEntry tnf_cache[AVP_TNF_CACHE];
static u8 *tnf_arena;
static size_t tnf_arena_size,tnf_used;
static s32 fade_step;

/* The retail 98 effects are eighty-byte FullSynth modules at these RAM
 * addresses.  DoEffect reads word 1 for sampler types, word 6 for FM, then
 * compares their low 16 bits when choosing a voice (MUSIC.o 0x9C6C).
 * The high bit is the synthesizer's loop flag.  These are *not* all equal
 * priority: applying the generic fallback to retail cues lets quieter
 * ambient sounds steal important high-priority weapon/alert voices. */
#define RETAIL_FX_FIRST 0x20cc8u
#define RETAIL_FX_LAST  0x22b18u
#define RETAIL_FX_STRIDE 80u
#define RETAIL_FX_COUNT (((RETAIL_FX_LAST-RETAIL_FX_FIRST)/RETAIL_FX_STRIDE)+1u)
static AvpSoundMeta retail_meta[RETAIL_FX_COUNT];
static int retail_meta_valid;
static u32 module_long(const u8 *p)
{
    return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];
}
void avp_music_bind_retail_sfx_descriptors(const u8 *rdb,size_t size)
{
    unsigned i;
    retail_meta_valid=0;
    if(!rdb||size<(size_t)(RETAIL_FX_LAST-0x20300u)+80u)return;
    for(i=0;i<RETAIL_FX_COUNT;i++){
        size_t off=(size_t)(RETAIL_FX_FIRST-0x20300u)+i*RETAIL_FX_STRIDE;
        u32 type=module_long(rdb+off),control;
        if(type!=4u&&type!=32u&&type!=44u)return;
        control=module_long(rdb+off+(type==4u?24u:4u));
        retail_meta[i].priority=(s16)(control&0xffffu);
        retail_meta[i].synth_type=(u8)type;
        retail_meta[i].looping=(u8)(control>>31);
    }
    retail_meta_valid=1;
}
static const AvpSoundMeta default_meta={0,1,0};
static const AvpSoundMeta *meta_for(unsigned id)
{
    if(sound_meta&&id<sound_meta_count)return &sound_meta[id];
    if(retail_meta_valid&&id>=RETAIL_FX_FIRST&&id<=RETAIL_FX_LAST&&
       (id-RETAIL_FX_FIRST)%RETAIL_FX_STRIDE==0u)
        return &retail_meta[(id-RETAIL_FX_FIRST)/RETAIL_FX_STRIDE];
    return &default_meta;
}
static void backend_stop(u32 handle)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    if(handle&&o->stop_sfx)o->stop_sfx(o->user,handle);
}
static void backend_mod(u32 handle,s32 value)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    if(handle&&o->mod_sfx)o->mod_sfx(o->user,handle,value);
}
static void request_stop(unsigned i,int clear_handle)
{
    if(i>=AVP_SOUND_VOICES)return;
    backend_stop(voice[i].handle);
    voice[i].type=AVP_SOUND_TYPE_STOPPED;
    voice[i].looping=0;
    if(clear_handle)voice[i].handle=0;
}
static void unloop_voice(unsigned i,int clear_handle)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    u32 h;
    if(i>=AVP_SOUND_VOICES)return;
    h=voice[i].handle;
    if(h&&o->unloop_sfx)o->unloop_sfx(o->user,h);
    if(clear_handle)voice[i].handle=0;
    voice[i].looping=0;
}
static u32 begin_voice(unsigned i,unsigned id,s16 priority,s32 mod_value)
{
    const AvpSoundMeta *m=meta_for(id);
    const AvpRuntimeOps *o=avp_runtime_ops();
    u32 h;
    if(i>=AVP_SOUND_VOICES)return (u32)-1;
    h=++ident;
    if(!h)h=++ident; /* retail Ident naturally wraps; keep zero as no-handle */
    voice[i].type=(s32)m->synth_type;
    voice[i].priority=priority;
    voice[i].looping=m->looping?1u:0u;
    voice[i].handle=h;
    voice[i].sound_id=id;
    voice[i].mod_value=mod_value;
    /* TFX's positive D1 overrides BOTH the occupied-voice comparator and
     * the new DSP module's low volume word. Send it atomically with start;
     * a post-start ModSfx would change a different host gain and race audio. */
    if(mod_value>=0 && o->play_sfx_handle_d1)
        o->play_sfx_handle_d1(o->user,id,h,i,mod_value);
    else if(o->play_sfx_handle)o->play_sfx_handle(o->user,id,h,i);
    else if(o->play_sfx)o->play_sfx(o->user,id);
    return h;
}

#ifdef AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE
/* Swap only the original reserved WEAPON voice.  Original 8-voice generic
 * SFX allocator and ident counter continue to be global and shared. */
static int mp_weapon_voice_busy;
static void mp_weapon_read(AvpMpWeaponAudioState *out)
{
    out->type=voice[1].type;out->priority=voice[1].priority;
    out->looping=voice[1].looping;out->reserved=voice[1].reserved;
    out->handle=voice[1].handle;out->sound_id=voice[1].sound_id;
    out->mod_value=voice[1].mod_value;
}
static void mp_weapon_write(const AvpMpWeaponAudioState *in)
{
    voice[1].type=in->type;voice[1].priority=in->priority;
    voice[1].looping=in->looping;voice[1].reserved=in->reserved;
    voice[1].handle=in->handle;voice[1].sound_id=in->sound_id;
    voice[1].mod_value=in->mod_value;
}
int avp_music_mp_weapon_capture(AvpMpWeaponAudioState *out)
{
    if(!out||mp_weapon_voice_busy)return 0;
    mp_weapon_read(out);return 1;
}
int avp_music_mp_weapon_enter(AvpMpWeaponAudioState *old_host,
                              const AvpMpWeaponAudioState *player)
{
    if(!old_host||!player||mp_weapon_voice_busy)return 0;
    mp_weapon_read(old_host);mp_weapon_write(player);
    mp_weapon_voice_busy=1;return 1;
}
int avp_music_mp_weapon_leave(const AvpMpWeaponAudioState *old_host,
                              AvpMpWeaponAudioState *player)
{
    if(!old_host||!player||!mp_weapon_voice_busy)return 0;
    mp_weapon_read(player);mp_weapon_write(old_host);
    mp_weapon_voice_busy=0;return 1;
}
#endif

void avp_music_bind_sfx_meta(const AvpSoundMeta *meta,unsigned count)
{
    sound_meta=meta;sound_meta_count=count;
}
void avp_music_bind_transfer_arena(void *arena,size_t size)
{
    tnf_arena=(u8*)arena;tnf_arena_size=size;InitTnF();
}

void KillSynt(void)
{
    /* Retail KillSynt stops the DSP/DAC engines.  Host-side effect state is
     * killed explicitly so a backend sees the same terminal condition. */
    KillAll();StopMusi();
}
void InitSynt(void)
{
    unsigned i;
    KillSynt();
    fx_fade_level=fx_fade_target=0x7fff;
    {const AvpRuntimeOps *o=avp_runtime_ops();
     if(o->set_master_volume)o->set_master_volume(o->user,0x7fffu);}
    fade_step=0;
    current_music=alarm_id=ambient_id=0;
    ident=0;
    for(i=0;i<AVP_SOUND_VOICES;i++){
        memset(&voice[i],0,sizeof(voice[i]));
        voice[i].type=AVP_SOUND_TYPE_STOPPED;
    }
    ProtectV(AVP_DEFAULT_PROTECTED);
#ifdef AVP_PC_HOST
    host_intro_typ1b_single_lane=0;
#endif
    InitTnF();
}
void StopMusi(void)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    if(o->stop_music)o->stop_music(o->user);
    current_music=0;
}

void InitTnF(void)
{
    memset(tnf_cache,0,sizeof(tnf_cache));
    tnf_used=0;
}
const void *TransnFi(const AvpTransferFile *file)
{
    unsigned i,empty=AVP_TNF_CACHE;
    size_t aligned,need;
    if(!file)return NULL;
    if(!tnf_arena)return file->data; /* retail fix_loc==0 path */
    for(i=0;i<AVP_TNF_CACHE;i++){
        if(tnf_cache[i].file==file)return tnf_cache[i].copy;
        if(!tnf_cache[i].file&&empty==AVP_TNF_CACHE)empty=i;
    }
    if(empty==AVP_TNF_CACHE)return NULL;
    aligned=(tnf_used+7u)&~(size_t)7u;
    need=(file->size+7u)&~(size_t)7u;
    if(aligned>tnf_arena_size||need>tnf_arena_size-aligned)return NULL;
    if(file->size)memcpy(tnf_arena+aligned,file->data,file->size);
    tnf_cache[empty].file=file;
    tnf_cache[empty].copy=tnf_arena+aligned;
    tnf_used=aligned+need;
    return tnf_cache[empty].copy;
}
void StartMus(unsigned id)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    InitTnF();
    StopMusi();
    current_music=id;
    if(o->start_music)o->start_music(o->user,id);
}
void ProtectV(unsigned count)
{
    /* Retail ProtectVoices stores free_ide=VIdent+count and
     * free_voi=voice_table+count*0x50. */
    protected_voices=count>AVP_SOUND_VOICES?AVP_SOUND_VOICES:count;
}

/* FXFadeDown/FXFadeUp install a temporary vblank callback in retail.  The
 * hosted representation keeps the same signed step/target state and advances
 * it when WaitFXFade pumps vblanks. */
static void fade_tick(void)
{
    s32 v=fx_fade_level;
    if(!fade_step)return;
    if(fx_fade_target==0){
        v-=fade_step;
        if(v<=0){v=0;fade_step=0;}
    }else{
        v+=fade_step;
        if(v>=0x7fff){v=0x7fff;fade_step=0;}
    }
    fx_fade_level=(s16)v;
    {const AvpRuntimeOps *o=avp_runtime_ops();
     if(o->set_master_volume)o->set_master_volume(o->user,(u32)v);}
}
void FXFadeDo(s32 step)
{
    WaitFXFa();
    fade_step=step;
    fx_fade_target=0;
    avp_runtime_set_fx_vblank(fade_tick);
}
void FXFadeUp(s32 step)
{
    WaitFXFa();
    fade_step=step;
    fx_fade_target=0x7fff;
    avp_runtime_set_fx_vblank(fade_tick);
}
void WaitFXFa(void)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    while(fade_step){if(o->wait_vblank)o->wait_vblank(o->user);fade_tick();}
}

void KillAlar(void)
{
    if(!alarm_id)return;
    alarm_id=0;
    unloop_voice(AVP_ALARM_VOICE,0);
    ProtectV(AVP_DEFAULT_PROTECTED);
}
void StartAla(unsigned id)
{
    /* Retail forces the alarm into voice 2, first releasing any loop already
     * there, then raises the protected range from 2 to 3 voices. */
    if(voice[AVP_ALARM_VOICE].type>=0)request_stop(AVP_ALARM_VOICE,1);
    ProtectV(3);
    alarm_id=begin_voice(AVP_ALARM_VOICE,id,-1,-1);
}
void KillAmbi(void)
{
    unloop_voice(AVP_AMBIENT_VOICE,0);
    ambient_id=0;
    {const AvpRuntimeOps *o=avp_runtime_ops();if(o->kill_ambient)o->kill_ambient(o->user);}
}
void Ambient(void)
{
    /* The exact level->ambient descriptor table is authored retail data.  The
     * CPU semantics are to release voice 0 before asking the resource/backend
     * layer to start the level's selected ambient sound in that reserved slot. */
    if(voice[AVP_AMBIENT_VOICE].type>=0)request_stop(AVP_AMBIENT_VOICE,1);
    ambient_id=++ident;if(!ambient_id)ambient_id=++ident;
    voice[AVP_AMBIENT_VOICE].type=1;
    voice[AVP_AMBIENT_VOICE].priority=-1;
    voice[AVP_AMBIENT_VOICE].looping=1;
    voice[AVP_AMBIENT_VOICE].handle=ambient_id;
    {const AvpRuntimeOps *o=avp_runtime_ops();if(o->ambient)o->ambient(o->user);}
}

static u32 do_effect_d1(unsigned id,s32 d1)
{
    const AvpSoundMeta *m=meta_for(id);
    unsigned i,best=AVP_SOUND_VOICES;
    unsigned search_start=protected_voices;
    /* MUSIC.o $9C6E TST.W D1 / $9C70 BPL: nonnegative D1 is
     * the caller's explicit DSP volume and allocation priority, not the
     * cartridge module's default. MJP Do_Escap supplies $2600. */
    s16 p=((d1&0x8000)==0)?(s16)(d1&0xffff):m->priority;
#ifdef AVP_PC_HOST
    /* The intro matches the one-voice escape text reference for TYP1B
     * only. Normal voice counts and retail priority/effect identity remain
     * unchanged; an occupied equal-priority lane rejects the request. */
    if(host_intro_typ1b_single_lane && id==AVP_FX_TYP1B)
        search_start=AVP_SOUND_VOICES-1u;
#endif
    /* The Jaguar DSP marks a completed voice stopped in shared DSP RAM.
       The native mixer keeps its own voice state; synchronize completed
       one-shots here, at the next allocation, on the game thread.  Without
       this, six short effects permanently exhaust the generic voice pool. */
    {const AvpRuntimeOps *o=avp_runtime_ops();
     if(o->sfx_handle_active){
        for(i=protected_voices;i<AVP_SOUND_VOICES;i++){
            if(voice[i].type>=0 &&
               (voice[i].handle
                ? !o->sfx_handle_active(o->user,voice[i].handle)
                : (o->sfx_voice_active && !o->sfx_voice_active(o->user,i)))){
                voice[i].type=AVP_SOUND_TYPE_STOPPED;
                voice[i].handle=0;
                voice[i].looping=0;
            }
        }
     }
    }
    /* First pass mirrors the retail search for a -4/stopped slot. */
    for(i=search_start;i<AVP_SOUND_VOICES;i++){
        if(voice[i].type<0){best=i;break;}
    }
    /* The retail MC68000 at $9CC4-$9CC6 executes CMP.W D4,D1; BGT.W
     * $9CE8, where D1 is the incoming priority and D4 the occupied slot's.
     * Therefore only a STRICTLY higher signed priority can steal an occupied
     * retail voice.  Equal priority is rejected.  Keep the longstanding PC
     * fallback for explicitly supplied non-retail/test metadata so existing
     * host control enhancements retain their established behavior. */
    if(best==AVP_SOUND_VOICES){
        const int original_retail=retail_meta_valid && !sound_meta;
        for(i=search_start;i<AVP_SOUND_VOICES;i++){
            if(original_retail ? (p>voice[i].priority) : (p>=voice[i].priority)){
                best=i;break;
            }
        }
    }
    /* Retail code has no third "same sound ID" exception.  Leave the
     * existing same-ID recovery strictly on the non-retail host/test path. */
    if(best==AVP_SOUND_VOICES && (!retail_meta_valid || sound_meta)){
        for(i=search_start;i<AVP_SOUND_VOICES;i++){
            if(voice[i].sound_id==id&&voice[i].type>=0){best=i;break;}
        }
    }
    if(best==AVP_SOUND_VOICES)return (u32)-1;
    if(voice[best].type>=0)request_stop(best,0);
    /* $9D3C-$9D42: a type-4 voice keeps its priority word LSR.W #2 in the
     * DSP voice long that the next steal scan reads at $9CD4-$9CDA, so later
     * effects compare against the quartered word, not the module word. */
    if(retail_meta_valid && !sound_meta && m->synth_type==4u)
        return begin_voice(best,id,(s16)((u16)p>>2),d1);
    return begin_voice(best,id,p,d1);
}
u32 DoEffect(unsigned id){return do_effect_d1(id,-1);}
u32 TFX(unsigned id){return do_effect_d1(id,-1);}
u32 avp_music_tfx_d1(unsigned id,s32 d1){return do_effect_d1(id,d1);}
/* Retail MUSIC.o PlacedEf $9C3A: start an effect directly in reserved DSP
 * voice D3 (no free/steal search), then share DoEffect's store path $9CE8.
 * MAZESCRN.S `wep_sfx` uses voice 1 (weapon), `pause_sfx` voice 0. */
u32 avp_music_placed_voice(unsigned id,unsigned v)
{
    const AvpSoundMeta *m=meta_for(id);
    s16 p=m->priority;
    if(v>=AVP_SOUND_VOICES)return (u32)-1;
    if(retail_meta_valid && !sound_meta && m->synth_type==4u)p=(s16)((u16)p>>2);
    if(voice[v].type>=0)request_stop(v,0);
    return begin_voice(v,id,p,-1);
}
u32 PlacedEf(unsigned id,s32 x,s32 y)
{
    u32 h=DoEffect(id);
    const AvpRuntimeOps *o=avp_runtime_ops();
    /* DoEffect already created the retail logical voice/handle.  PlacedEf
       modifies that same voice; it must not start a second host sample. */
    if(h!=(u32)-1&&o->position_sfx)o->position_sfx(o->user,h,x,y);
    return h;
}
void KillAll(void)
{
    unsigned i;const AvpRuntimeOps *o=avp_runtime_ops();
    for(i=0;i<AVP_SOUND_VOICES;i++)if(voice[i].type!=0)voice[i].type=AVP_SOUND_TYPE_STOPPED;
    if(o->kill_sounds)o->kill_sounds(o->user);
}
void KillSfx(u32 h)
{
    unsigned i;
    for(i=0;i<AVP_SOUND_VOICES;i++)if(voice[i].handle==h){request_stop(i,1);return;}
}
void UnLoopAl(void)
{
    unsigned i;
    /* Retail starts at voice 1, deliberately leaving ambient voice 0 alone,
     * and preserves voice 2 while an alarm handle is active. */
    for(i=1;i<AVP_SOUND_VOICES;i++){
        if(alarm_id&&i==AVP_ALARM_VOICE)continue;
        unloop_voice(i,1);
    }
}
void StopLoop(u32 h)
{
    unsigned i;
    for(i=0;i<AVP_SOUND_VOICES;i++)if(voice[i].handle==h){unloop_voice(i,1);return;}
}
/* MAZESCRN.S LoseSounds/RestoreSounds CPU bookkeeping.  Retail snapshots the
 * complete second voice plus the first long of the third voice, preserves the
 * weapon-effect identifier, silences all but the first two voices, and restores
 * that state after pause.  The DSP table itself is represented by AvpVoiceState
 * here; mixer execution remains backend-owned. */
static AvpVoiceState pause_voice1;
static s32 pause_voice2_type;
static u32 pause_weapon_handle;
static int pause_audio_valid;

void avp_pause_audio_save(void)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    fx_fade_level=0; /* UEBERVOLUME=0 before the table transfer */
    if(o->set_master_volume)o->set_master_volume(o->user,0u);
    pause_voice1=voice[1];
    pause_voice2_type=voice[2].type;
    pause_weapon_handle=voice[1].handle; /* VIdent+4 */
    pause_audio_valid=1;
    if(o->pause_sfx_save)o->pause_sfx_save(o->user);
    voice[2].type=AVP_SOUND_TYPE_STOPPED;
    voice[0].type=AVP_SOUND_TYPE_STOPPED;
    voice[1].type=AVP_SOUND_TYPE_STOPPED;
    if(o->kill_sounds)o->kill_sounds(o->user);
    fx_fade_level=0x7fff; /* in-pause sounds run at full master volume */
    if(o->set_master_volume)o->set_master_volume(o->user,0x7fffu);
}

void avp_pause_audio_restore(void)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    if(!pause_audio_valid)return;
    /* Hardware version busy-waits for the two pause voices to become free.
     * A host mixer owns completion; stopping them provides the same boundary. */
    if(voice[0].type>=0)request_stop(0,0);
    if(voice[1].type>=0)request_stop(1,0);
    fx_fade_level=0;
    if(o->set_master_volume)o->set_master_volume(o->user,0u);
    Ambient();
    voice[1]=pause_voice1;
    voice[2].type=pause_voice2_type;
    voice[1].handle=pause_weapon_handle;
    fx_fade_level=0x7fff;
    if(o->set_master_volume)o->set_master_volume(o->user,0x7fffu);
    /* Restore the actual held synthesizer phase of the weapon and alarm
       when the host supports it.  The fallback is retained for non-PC
       backends that only implement individual effect launches. */
    if(o->pause_sfx_restore)
        o->pause_sfx_restore(o->user,
            voice[1].type>=0?voice[1].handle:0u,
            voice[2].type>=0?voice[2].handle:0u);
    else if(voice[1].type>=0&&voice[1].handle&&o->play_sfx_handle)
        o->play_sfx_handle(o->user,voice[1].sound_id,voice[1].handle,1u);
    pause_audio_valid=0;
}

void ModSfx(u32 h,s32 value)
{
    unsigned i;
    for(i=0;i<AVP_SOUND_VOICES;i++)if(voice[i].handle==h){voice[i].mod_value=value;backend_mod(h,value);return;}
}
