#include "jaguar/host/fullsynth_host.h"
#include "maze.h"
#include "avp_resources.h"
#include "avp_sfx_ids.h"
#include "jaguar/host/dsp_arithmetic.h"
#ifdef AVP_LOCAL_MULTIPLAYER
#include "jaguar/host/mp_waveform_proximity.h"
#include "jaguar/host/mp_spatial_audio.h"
#include "local_mp.h"
#endif
/* Keep the FullSynth mixer synchronization native on each host.  The
 * reconstructed synthesis, voice allocation, and PCM sample path are shared;
 * only the lock implementation differs. */
#ifdef _WIN32
#define ScreenOff avp_winnt_ScreenOff /* winnt.h enumerator vs game routine */
#include <windows.h>
#undef ScreenOff
typedef CRITICAL_SECTION AvpSynthLock;
static int synth_lock_init(AvpSynthLock *lock)
{
    InitializeCriticalSection(lock);
    return 0;
}
static void synth_lock_destroy(AvpSynthLock *lock) { DeleteCriticalSection(lock); }
static void synth_lock_enter(AvpSynthLock *lock) { EnterCriticalSection(lock); }
static void synth_lock_leave(AvpSynthLock *lock) { LeaveCriticalSection(lock); }
#else
#include <pthread.h>
typedef pthread_mutex_t AvpSynthLock;
static int synth_lock_init(AvpSynthLock *lock) { return pthread_mutex_init(lock,NULL); }
static void synth_lock_destroy(AvpSynthLock *lock) { (void)pthread_mutex_destroy(lock); }
static void synth_lock_enter(AvpSynthLock *lock) { (void)pthread_mutex_lock(lock); }
static void synth_lock_leave(AvpSynthLock *lock) { (void)pthread_mutex_unlock(lock); }
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct AvpSynthVoice {
    u32 p[20];
    const u8 *sample;
    size_t sample_len;
    u32 orig_pitch,orig_volume;
    int active;
} AvpSynthVoice;

typedef AvpSynthVoice AvpMusicVoice;

typedef struct AvpHostedMusic {
    u8 *score;
    size_t score_len,score_pos;
    u32 patch_addr;
    u32 timer;
    uint64_t pit_accum,pit_den;
    unsigned descriptor;
    int active;
    AvpMusicVoice voice[8];
} AvpHostedMusic;

typedef struct AvpHostedVoice {
    AvpSynthVoice synth;
    int gain_l,gain_r;
    int loop_override;
    u32 logical_handle;
    unsigned game_voice;
    unsigned sound_id;
#ifdef AVP_LOCAL_MULTIPLAYER
    unsigned mp_source_active,mp_source_player,mp_source_level;
    s32 mp_source_x,mp_source_y,mp_meter_l,mp_meter_r;
#endif
} AvpHostedVoice;

static struct {
    const AvpRomImage *rom;
    u8 *rdb,*rdbt;
    size_t rdbn,rdbtn;
    AvpSynthLock lock;
    int lock_init;
#ifdef AVP_LOCAL_MULTIPLAYER
    AvpHostedVoice v[15u+AVP_LOCAL_MP_MAX_ACTORS];
    /* Co-op weapon voice 1 for each additional actor: host PCM lanes, not Jaguar voices. */
    int mp_session_paused;
    unsigned mp_weapon_owner;
    uint64_t mp_weapon_starts[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned mp_source_active,mp_source_player,mp_source_level;
    s32 mp_source_x,mp_source_y;
    AvpMpSpatialListener mp_listeners[4];
    unsigned mp_bot_mask,mp_spatial_active;
#else
    AvpHostedVoice v[16]; /* original hosted single-player layout */
#endif
    /* Only Jaguar CPU voices 1 and 2 survive LoseSounds. Store the entire
       hosted DSP state so paused looping effects resume at the same phase. */
    AvpHostedVoice paused[2];
    int paused_valid;
    unsigned rr;
    /* AMP.S modifies one global FX_xp1b15 PCM module descriptor before
     * starting a voice.  The rate stays set until the next explicit write. */
    u32 explosion_rate;
    int explosion_rate_written;
    u32 master_volume;
    unsigned mix_gain[5]; /* 0 music, 1 SFX, 2 ambience, 3 voice, 4 random hissing */
    int mix_music_on,mix_ambient_on,mix_hissing_on;
    s32 meter_l,meter_r; /* protected by the same lock as PCM mixing */
    AvpHostedMusic music;
} F={0};

#ifdef AVP_LOCAL_MULTIPLAYER
#define AVP_HOST_SFX_SLOTS (15u+AVP_LOCAL_MP_MAX_ACTORS)
#else
#define AVP_HOST_SFX_SLOTS 16u
#endif

static unsigned host_sound_mix_group(unsigned slot,unsigned sound_id);
static int dec8(u8 x);
static int load_rdb(void);

static u32 be32(const u8 *p)
{
    return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];
}

static void put_be32(u8 *p,u32 v)
{
    p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;
}

static int load_rdb_text(void)
{
    if(F.rdbt)return 1;
    return avp_resources_decode_rdb_text(F.rom,&F.rdbt,&F.rdbtn);
}

static int dsp_table_long(u32 addr,s32 *out)
{
    enum { SRC_BASE=0x9e98u,DST_BASE=0xf1bc00u,DST_END=0xf1cdf0u,TEXT_BASE=0x8000u };
    size_t off;
    if(!out||addr<DST_BASE||addr+4u>DST_END||!load_rdb_text())return 0;
    off=(size_t)(SRC_BASE-TEXT_BASE)+(size_t)(addr-DST_BASE);
    if(off+4u>F.rdbtn)return 0;
    *out=(s32)be32(F.rdbt+off);
    return 1;
}

/* The original JERRY DSP contains eight 128-entry signed-16-bit waveforms.
 * They are hardware data, not AVP cartridge content.  Only mathematical,
 * deterministic wave approximations are generated here: no separate ROM is
 * loaded, manufactured, cached, or distributed.  The original FM modules'
 * p[1] and p[2] addresses independently select their oscillator tables.
 * Hardware-exact table values and DSP interpolation remain unverified. */
static s16 host_wave_table[8][128];
/* JERRY's 4 KiB wave-table ROM is NOT stored in AVP.jag.  If the user
 * supplies their own verified Jaguar wave-ROM dump, use its actual 16-bit
 * samples rather than generating mathematically similar instruments.
 * This is optional: no hardware ROM bytes are shipped with the port. */
static int host_wave_rom_exact;
static u32 wave_crc32(const u8 *data,size_t len)
{
    u32 crc=0xffffffffu;
    for(size_t i=0;i<len;i++){
        crc^=data[i];
        for(unsigned k=0;k<8u;k++)crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);
    }
    return ~crc;
}
int avp_jaguar_fullsynth_load_wave_rom(const char *path)
{
    u8 data[4096];
    FILE *f;
    size_t n;
    if(!path||!path[0])return 0;
    f=fopen(path,"rb");
    if(!f)return 0;
    n=fread(data,1,sizeof(data),f);
    if(n!=sizeof(data)||fgetc(f)!=EOF){fclose(f);return 0;}
    fclose(f);
    /* MAME's verified ROM hash: CRC32 7a25ee5b, SHA1 58117e11...
     * Accept only the original Jaguar ROM's exact byte content. */
    if(wave_crc32(data,sizeof(data))!=0x7a25ee5bu)return 0;
    if(F.lock_init)synth_lock_enter(&F.lock);
    for(unsigned wave=0;wave<8u;wave++)for(unsigned i=0;i<128u;i++){
        size_t at=wave*512u+i*4u;
        u16 raw=(u16)((u16)data[at+2u]<<8|data[at+3u]);
        host_wave_table[wave][i]=(s16)raw;
    }
    host_wave_rom_exact=1;
    if(F.lock_init)synth_lock_leave(&F.lock);
    return 1;
}
int avp_jaguar_fullsynth_uses_exact_waves(void){return host_wave_rom_exact;}

static void init_host_wave_tables(void)
{
    u32 noise_state=0x5a17c309u;
    unsigned i;
    for(i=0;i<128u;i++){
        double t=(double)i/128.0, angle=2.0*M_PI*t;
        double sine=sin(angle);
        double tri=(2.0/M_PI)*asin(sine);
        double am=sine*(0.5+0.5*sin(2.0*angle));
        double harmonic=(sine+0.5*sin(2.0*angle))/1.5;
        double chirp=sin(2.0*M_PI*8.0*t*t);
        double n, noisytri;
        noise_state^=noise_state<<13;
        noise_state^=noise_state>>17;
        noise_state^=noise_state<<5;
        n=(double)((noise_state>>16)&0xffffu)/32767.5-1.0;
        noisytri=tri*0.75+n*0.25;
        host_wave_table[0][i]=(s16)lrint(tri*32767.0); /* F1D000 TRI */
        host_wave_table[1][i]=(s16)lrint(sine*32767.0); /* F1D200 SINE */
        host_wave_table[2][i]=(s16)lrint(am*32767.0); /* F1D400 AMSINE */
        host_wave_table[3][i]=(s16)lrint(harmonic*32767.0); /* F1D600 SINE12W */
        host_wave_table[4][i]=(s16)lrint(chirp*32767.0); /* F1D800 CHIRP16 */
        host_wave_table[5][i]=(s16)lrint(noisytri*32767.0); /* F1DA00 NTRI */
        host_wave_table[6][i]=(s16)(i==0u?32767:0); /* F1DC00 DELTA */
        host_wave_table[7][i]=(s16)lrint(n*32767.0); /* F1DE00 NOISE */
    }
}

static s32 host_wave_sample(u32 wavetable,u32 phase)
{
    unsigned wave,ix,nx,fr;
    s32 a,b;
    if(wavetable<0xf1d000u||wavetable>=0xf1e000u ||
       ((wavetable-0xf1d000u)&0x1ffu)!=0u)return 0;
    wave=(wavetable-0xf1d000u)>>9;
    ix=(phase>>16)&127u;
    nx=(ix+1u)&127u;
    fr=(phase>>8)&255u;
    a=host_wave_table[wave][ix];b=host_wave_table[wave][nx];
    /* Original FullSynth FM interpolation uses r10=0xff-fraction and
     * r12=fraction before IMULTN/IMACN/RESMAC, then SHARQ #8.
     * See original DSP F1B494..F1B49A and the identical modulation path.
     * A 0x100 first coefficient incorrectly overstates even exact table
     * samples when the phase has zero fractional bits. */
    /* F1B494/496/498: IMULTN/IMACN/RESMAC writes a 32-bit result;
     * F1B49A then shifts that register, not the 40-bit accumulator. */
    return (s32)avp_jerry_mac2_resmac_asr(a,(s32)(255u-fr),b,(s32)fr,8u);
}

/* FullSynth's compressed module word 2 refers to a cartridge-resident
 * POINTER CELL (e.g. door FX $815D38 -> $96B6E8), not necessarily to the
 * compressed bytes themselves.  Most retail cells are only four bytes long:
 * the following word is another pointer or unrelated packed data, never an
 * implied length.  Module word 4 gives the inclusive 8.8 end position. */
static int music_sample_ref(u32 ref,u32 end,const u8 **data,size_t *n)
{
    const u32 base=0x800000u;
    size_t off,need,source;
    u32 target;
    if(data)*data=NULL;
    if(n)*n=0;
    if(!data||!n||!F.rom||!F.rom->data||ref<base)return 0;
    need=((size_t)(end>>8))+1u;
    if(!need||need>F.rom->size)return 0;
    off=(size_t)(ref-base);
    if(off>F.rom->size)return 0;
    /* Authentic ROM pointers in the FullSynth modules are first-level cells.
     * Only read one big-endian pointer and bound it by the module's end.
     * Never interpret the next four bytes as the sample's byte count. */
    if(off+4u<=F.rom->size){
        target=be32(F.rom->data+off);
        if(target>=base){
            source=(size_t)(target-base);
            if(source<=F.rom->size&&need<=F.rom->size-source){
                *data=F.rom->data+source;*n=need;return 1;
            }
        }
    }
    /* Retain direct sample references used by earlier integrations and
     * isolated test fixtures, without changing the canonical table path. */
    if(need>F.rom->size-off)return 0;
    *data=F.rom->data+off;*n=need;
    return 1;
}

/* High-bit-tagged cartridge locations in the music descriptors are also
 * POINTER CELLS.  Load the actual score, and stop copying at its terminating
 * time word so a neighboring score/sample is never treated as music data. */
static int music_score_ref(u32 ref,const u8 **data,size_t *n)
{
    const u32 base=0x800000u;
    u32 addr=ref&0x7fffffffu,ptr;
    size_t off,j,limit;
    const u8 *src;
    if(data)*data=NULL;
    if(n)*n=0;
    if(!F.rom||!F.rom->data||!data||!n||addr<base)return 0;
    off=(size_t)(addr-base);
    if(off>F.rom->size||F.rom->size-off<8u)return 0;
    if(ref&0x80000000u){
        ptr=be32(F.rom->data+off);
        if(ptr<base)return 0;
        off=(size_t)(ptr-base);
        if(off>F.rom->size||F.rom->size-off<8u)return 0;
    }
    src=F.rom->data+off;
    limit=F.rom->size-off;
    if(limit>0x40000u)limit=0x40000u;
    for(j=0;j+8u<=limit;j+=8u){
        if(be32(src+j)==0x7fffffffu){
            *data=src;*n=j+8u;return 1;
        }
    }
    return 0;
}

static const u8 *music_patch(unsigned patch)
{
    size_t off;
    if(!load_rdb()||F.music.patch_addr<0x20300u)return NULL;
    off=(size_t)(F.music.patch_addr-0x20300u)+(size_t)patch*80u;
    if(off+80u>F.rdbn)return NULL;
    return F.rdb+off;
}

static void music_voice_off(unsigned v)
{
    AvpMusicVoice *m;
    if(v>=8u)return;
    m=&F.music.voice[v];
    if(!m->active)return;
    switch(m->p[0]){
    case 4u: /* FM + envelope */
        m->p[6]&=0x7fffffffu;
        m->p[12]=m->p[15];
        break;
    case 32u: /* compressed sampler */
    case 44u: /* compressed sampler + envelope */
        m->p[1]&=0x7fffffffu;
        m->p[4]=m->p[8];
        break;
    default:
        m->active=0;
        break;
    }
}

static void music_note_on(unsigned v,u32 ev)
{
    const u8 *p;
    AvpMusicVoice *m;
    unsigned i,patch=(ev>>21)&31u;
    u32 freq=(ev>>7)&0x1fffu;
    u32 amp=(ev&0x7fu)<<6;
    if(v>=8u||(p=music_patch(patch))==NULL)return;
    m=&F.music.voice[v];memset(m,0,sizeof(*m));
    for(i=0;i<20u;i++)m->p[i]=be32(p+i*4u);
    m->active=1;
    switch(m->p[0]){
    case 4u:{
        u32 fl=m->p[6]&0x80000000u;
        if(m->p[8]&0x80000000u)m->p[8]=(m->p[8]&0x7fffffffu)*freq;
        m->p[4]=(u32)(((uint64_t)m->p[4]*freq)>>4);
        m->orig_pitch=m->p[4];
        m->p[6]=fl|(u32)(((uint64_t)(m->p[6]&0x7fffffffu)*amp)>>8);
        m->orig_volume=m->p[6]&0x7fffffffu;
        break;
    }
    case 32u:
    case 44u:{
        u32 fl=m->p[1]&0x80000000u;
        m->p[3]=(u32)(((uint64_t)m->p[3]*freq)>>12);
        m->orig_pitch=m->p[3];
        m->p[1]=fl|(u32)(((uint64_t)(m->p[1]&0x7fffffffu)*amp)>>8);
        m->orig_volume=m->p[1]&0x7fffffffu;
        if(!music_sample_ref(m->p[2],m->p[4],&m->sample,&m->sample_len))m->active=0;
        break;
    }
    default:m->active=0;break;
    }
}

static void music_controller(unsigned v,u32 ev)
{
    AvpMusicVoice *m;
    unsigned ctrl=(ev>>15)&31u;
    u32 val=ev&0x7fffu;
    if(v>=8u)return;
    m=&F.music.voice[v];if(!m->active)return;
    if(ctrl==7u){
        u32 nv=(u32)(((uint64_t)m->orig_volume*val)>>8);
        if(m->p[0]==4u)m->p[6]=(m->p[6]&0x80000000u)|nv;
        else m->p[1]=(m->p[1]&0x80000000u)|nv;
    }else if(ctrl==9u){
        if(m->p[0]==4u)m->p[4]=(u32)(((uint64_t)m->orig_pitch*val)>>4);
        else m->p[3]=(u32)(((uint64_t)m->orig_pitch*val)>>12);
    }else if(ctrl==10u){
        u32 pan=(val<<8);if(pan)pan--;if(pan>0x7fffu)pan=0x7fffu;m->p[19]=pan;
    }
}

static void music_tick(void)
{
    AvpHostedMusic *m=&F.music;
    unsigned events;
    if(!m->active||!m->score)return;
    m->timer++;
    /* Jaguar's sequencer drains the event list through the current tick.
       One-event-per-tick playback audibly breaks chords, note-offs and
       controller changes that share a score timestamp. Bound malformed
       zero-delay branch loops rather than hanging the audio thread. */
    for(events=0;events<4096u&&m->score_pos+8u<=m->score_len;events++){
        u32 when=be32(m->score+m->score_pos),ev,type,v;
        int32_t disp;
        if(when==0x7fffffffu||when>=m->timer)break;
        ev=be32(m->score+m->score_pos+4u);m->score_pos+=8u;
        v=(ev>>26)&7u;type=ev>>29;
        if(ev&0x80000000u){music_note_on(v,ev);continue;}
        if(type==0u){music_voice_off(v);continue;}
        if(type==2u){music_controller(v,ev);continue;}
        if(type==3u){
            u32 count=ev&15u;
            /* The authored title score enters its backward branch with the
             * working count cleared and an 8-step reload in bits 4..7.
             * Reloading must not skip this very branch: doing so falls into
             * the score terminator, stopping the title track after one pass.
             * Bit 28 requests a repeating branch without decrement. */
            if(!count){
                count=(ev>>4)&15u;
                ev=(ev&~15u)|count;
                put_be32(m->score+m->score_pos-4u,ev);
            }
            if(count){
                if(!(ev&0x10000000u)){
                    ev--;
                    put_be32(m->score+m->score_pos-4u,ev);
                }
                disp=(int32_t)((ev>>8)&0xfffffu);
                if(disp&0x80000)disp|=(int32_t)~0xfffff;
                {int64_t np=(int64_t)m->score_pos+(int64_t)disp*8;
                 if(np>=0&&(uint64_t)np+8u<=m->score_len){
                     m->score_pos=(size_t)np;
                     m->timer=be32(m->score+m->score_pos);
                 }}
            }
        }
    }
}

/* Original JERRY IMULT only observes the signed low 16 bits of a DSP
 * register.  This helper is also used by the compressed/envelope handler;
 * place it before both synthesis paths rather than duplicating a cast. */
static s32 dsp_low16_signed(u32 word)
{
    u32 lo=word&0xffffu;
    return lo<=0x7fffu?(s32)lo:(s32)((int64_t)lo-0x10000ll);
}

static s32 synth_sample_compressed(AvpSynthVoice *m,int envelope)
{
    u32 pos,fr,ix;int s0,s1;int64_t s;u32 vol;
    if(!m->sample||!m->sample_len)return 0;
    pos=m->p[6]+m->p[3];m->p[6]=pos;
    /* In both original compressed-sample handlers the Jaguar performs
     * CMP r5,r3 (end - advanced position) and JR cc20: branch when the
     * wrapped subtraction's N flag is clear.  An unsigned pos > end test
     * is not equivalent when these 32-bit DSP registers wrap.  Keep the
     * existing branch actions and delay-slot loop subtraction unchanged. */
    if(((m->p[4]-pos)&0x80000000u)!=0u){
        /* The Jaguar branch delay slot always subtracts the loop length.
           Plain compressed samples then die when their loop flag is clear;
           the envelope variant continues so its release envelope can finish. */
        pos-=m->p[5];m->p[6]=pos;
        if(!(m->p[1]&0x80000000u)&&!envelope){m->active=0;return 0;}
    }
    ix=pos>>8;fr=pos&255u;
    if(ix>=m->sample_len){m->active=0;return 0;}
    s0=dec8(m->sample[ix]);s1=dec8(m->sample[ix+1u<m->sample_len?ix+1u:ix]);
    s=avp_jerry_asr64((int64_t)s0*(255u-fr)+(int64_t)s1*fr,7u);
    if(envelope){
        s32 cur=(s32)m->p[11],slope=(s32)m->p[12],dest=(s32)m->p[13],iv;
        if(!(m->p[1]&0x40000000u)&&!(m->p[1]&0x80000000u)){
            m->p[1]|=0x40000000u;m->p[12]=m->p[10];m->p[13]=0u;
            slope=(s32)m->p[12];dest=0;
        }
        /* The JERRY DSP adds two 32-bit registers modulo 2^32.  Signed
         * C overflow is undefined; use unsigned arithmetic for the exact
         * register wrap, then interpret the resulting two's-complement word.
         * The sign-comparison subtraction below is also a DSP word operation. */
        {
            u32 wrapped=(u32)cur+(u32)slope;
            cur=wrapped<=0x7fffffffu?(s32)wrapped:
                (s32)((int64_t)wrapped-0x100000000ll);
        }
        iv=(s32)avp_jerry_asr64(cur,15u);
        if((((u32)dest-(u32)iv)^(u32)slope)&0x80000000u){
            if(m->p[1]&0x40000000u){m->active=0;return 0;}
            cur=(s32)((u32)dest<<15);m->p[9]+=8u;
            {s32 ns,nd;if(!dsp_table_long(m->p[9],&ns)||!dsp_table_long(m->p[9]+4u,&nd)){m->active=0;return 0;}m->p[12]=(u32)ns;m->p[13]=(u32)nd;slope=ns;}
        }
        m->p[11]=(u32)cur;
        /* Original compressed-envelope module at F1B442/F1B444 uses
         * IMULT r3,r13 then SHARQ #15; r3 contains the envelope value
         * shifted by 15.  IMULT consumes only the signed low 16 bits of
         * BOTH register operands, not the full 32-bit C intermediate. */
        s=avp_jerry_asr64((int64_t)dsp_low16_signed((u32)s)*
                           dsp_low16_signed((u32)avp_jerry_asr64(cur,15u)),15u);
    }
    /* Both compressed handlers use IMULT for the final volume stage as
     * well (F1B2CC/F1B2CE and F1B446/F1B448). */
    vol=m->p[1]&0x7fffu;
    s=avp_jerry_asr64((int64_t)dsp_low16_signed((u32)s)*dsp_low16_signed(vol),15u);
    /* Caller applies the retail pan field after module synthesis. */
    return (s32)s;
}

static s32 synth_sample_fmenv(AvpSynthVoice *m)
{
    const u32 mask=0x007fffffu;u32 pos,fmp,ep,frac;int64_t wave,env,mod;
    s32 e0,e1;
    pos=(m->p[3]+m->p[4]+m->p[5])&mask;m->p[3]=pos;
    fmp=(m->p[7]+m->p[8])&mask;m->p[7]=fmp;
    ep=m->p[14]+m->p[11];m->p[14]=ep;
    /* Original FM handler F1B332 CMP r22,r24 compares the 32-bit end
     * register (r24) against the newly advanced envelope phase (r22).
     * F1B336 JR cc20 takes the in-range path when the subtraction's N bit
     * is CLEAR.  An unsigned C 'ep > end' comparison does not preserve this
     * condition when bit 31 is set in either register.  Subtract as Jaguar
     * 32-bit registers and inspect the actual wrapped-result sign bit. */
    if(((m->p[12]-ep)&0x80000000u)!=0u){
        if(m->p[6]&0x80000000u){ep-=m->p[13];m->p[14]=ep;}
        else{m->active=0;return 0;}
    }
    wave=host_wave_sample(m->p[1],pos);mod=host_wave_sample(m->p[2],fmp);
    frac=(ep>>8)&255u;
    if(!dsp_table_long(m->p[10]+((ep>>16)*4u),&e0)||!dsp_table_long(m->p[10]+((((ep>>16)+1u))*4u),&e1)){m->active=0;return 0;}
    /* JERRY FM envelope interpolation loads 32-bit DSP table entries, but
     * F1B36A IMULTN and F1B36C IMACN consume only their signed low 16 bits.
     * F1B36E RESMAC then returns the combined result and F1B372 shifts it.
     * Reading the full host s32 table values here leaks their high halves. */
    /* F1B36A/36C/36E/372: low signed halfwords accumulate in the MAC;
     * RESMAC exposes the low 32-bit register before SHARQ #8. */
    env=avp_jerry_mac2_resmac_asr(dsp_low16_signed((u32)e0),(s32)(255u-frac),
                                  dsp_low16_signed((u32)e1),(s32)frac,8u);
    /* F1B374 IMULT r5,r13 multiplies the envelope by the volume first,
     * and F1B378 SHARQ #15 rounds that intermediate DSP register.  Only
     * at F1B37C does IMULT r16,r13 apply the waveform.  Multiplying the
     * waveform by volume first (the R168 host order) changes the result
     * because the two intermediate signed-low-16 products do not commute
     * across the arithmetic shift. */
    /* Original FM module F1B376, F1B37C and F1B382 uses IMULT:
     * each input is its register's SIGNED LOW 16 bits, not the entire
     * 32-bit descriptor word or a wide host interpolation result.  Keep
     * the same original shifts and the existing two's-complement p[5]
     * register wrap.  In particular p[16] and feedback index p[9] may
     * legitimately contain nonzero upper bits. */
    {
        int64_t depth=avp_jerry_asr64(
            (int64_t)dsp_low16_signed((u32)env)*dsp_low16_signed(m->p[16]),15u);
        int64_t idx=(int64_t)(s32)m->p[9]+depth;
        m->p[5]=(u32)avp_jerry_asr64(
            (int64_t)dsp_low16_signed((u32)mod)*dsp_low16_signed((u32)idx),7u);
    }
    /* Preserve the previous independent feedback calculation above.  Only
     * the final carrier gain path changes: the DSP multiplies and shifts
     * envelope*volume before IMULT applies the oscillator. */
    env=avp_jerry_asr64((int64_t)dsp_low16_signed((u32)env)*
                          dsp_low16_signed(m->p[6]),15u);
    wave=avp_jerry_asr64(
        (int64_t)dsp_low16_signed((u32)wave)*dsp_low16_signed((u32)env),15u);
    return (s32)wave;
}

static s32 synth_sample(AvpSynthVoice *v)
{
    if(!v||!v->active)return 0;
    switch(v->p[0]){
    case 4u:return synth_sample_fmenv(v);
    case 32u:return synth_sample_compressed(v,0);
    case 44u:return synth_sample_compressed(v,1);
    default:v->active=0;return 0;
    }
}

/* The original FullSynth stereo accumulators r9/r26 are 32-bit DSP
 * registers, not the 40-bit MAC or unbounded signed host integers.  C unsigned
 * addition gives the same 32-bit carry discard as Jaguar ADD (F1B26A/26E,
 * F1B2D6/2DA, F1B392/396, F1B454/458). */
static void music_mix_one(u32 *L,u32 *R)
{
    unsigned i;
    for(i=0;i<8u;i++){
        AvpMusicVoice *m=&F.music.voice[i];s32 s;u32 pan;int64_t l,r;
        if(!m->active)continue;
        s=synth_sample(m);
        if(!m->active&&s==0)continue;
        /* The original FullSynth modules add their full 16x16 pan products
         * to r9/r26; the shared DSP mixer shifts each channel only after
         * all voices have contributed (F1B100/F1B102).  Do not round every
         * voice separately, particularly when quiet notes form a chord. */
        /* JERRY loads the full pan register and subtracts it from 0x7fff
         * before two signed-low-16 IMULTs.  Masking bit 15 of pan is not
         * equivalent to the original DSP for a signed out-of-range value. */
        pan=m->p[19];
        r=(int64_t)dsp_low16_signed((u32)s)*dsp_low16_signed(pan);
        l=(int64_t)dsp_low16_signed((u32)s)*dsp_low16_signed(0x7fffu-pan);
        *L+=(u32)l;*R+=(u32)r;
    }
}

static int dec8(u8 x)
{
    int m=x&127,v=m*m;
    return (x&128)?-v:v;
}

static int load_rdb(void)
{
    if(F.rdb)return 1;
    return avp_resources_decode_rdb(F.rom,&F.rdb,&F.rdbn);
}

/* MAIN/RDB.TXT $97E4..$9808 initializes two BSS sound-pointer cells
 * before playback.  Both point into the cocoon sample loaded through the
 * ROM pointer at $815E44.  EGGX1 and USECN are the only retail descriptors
 * that use these BSS cells instead of a direct cartridge pointer cell.
 * Leave the original descriptor words intact; resolve the runtime pointer
 * only for the host's sample-data view. */
static int runtime_sound_sample_ref(u32 ref,u32 end,const u8 **data,size_t *n)
{
    const size_t cell_off=0x815e44u-0x800000u;
    size_t extra,offset,need;
    u32 base;
    if(ref==0x2eec0u)extra=0x3a98u;
    else if(ref==0x2eec4u)extra=0x3e80u;
    else return 0;
    if(!F.rom||!F.rom->data||cell_off+4u>F.rom->size)return 0;
    base=be32(F.rom->data+cell_off);
    if(base<0x800000u)return 0;
    offset=(size_t)(base-0x800000u);
    need=(size_t)(end>>8)+1u;
    if(offset>F.rom->size||extra>F.rom->size-offset)return 0;
    offset+=extra;
    if(need>F.rom->size-offset)return 0;
    *data=F.rom->data+offset;
    *n=need;
    return 1;
}

static int load_sfx_descriptor(unsigned id,AvpSynthVoice *v)
{
    size_t o;unsigned i;
    if(!v||id<0x20300u||!load_rdb())return 0;
    o=(size_t)(id-0x20300u);
    if(o+80u>F.rdbn)return 0;
    memset(v,0,sizeof(*v));
    for(i=0;i<20u;i++)v->p[i]=be32(F.rdb+o+i*4u);
    if(id==AVP_FX_XP1B1&&F.explosion_rate_written&&
       (v->p[0]==32u||v->p[0]==44u))v->p[3]=F.explosion_rate;
    if(v->p[0]!=4u&&v->p[0]!=32u&&v->p[0]!=44u)return 0;
    if(v->p[0]==32u||v->p[0]==44u){
        if(!runtime_sound_sample_ref(v->p[2],v->p[4],&v->sample,&v->sample_len)&&
           !music_sample_ref(v->p[2],v->p[4],&v->sample,&v->sample_len))return 0;
    }
    v->orig_pitch=(v->p[0]==4u)?v->p[4]:v->p[3];
    v->orig_volume=(v->p[0]==4u)?(v->p[6]&0x7fffffffu):(v->p[1]&0x7fffffffu);
    v->active=1;
    return 1;
}

int avp_jaguar_fullsynth_init(const AvpRomImage *rom)
{
    F.rom=rom;
    if(synth_lock_init(&F.lock)!=0)return 0;
    F.lock_init=1;
    F.master_volume=0x7fffu;
    for(unsigned mi=0;mi<5u;mi++)F.mix_gain[mi]=100u;
    F.mix_music_on=F.mix_ambient_on=F.mix_hissing_on=1;
    init_host_wave_tables();
    host_wave_rom_exact=0;
    return 1;
}

void avp_jaguar_fullsynth_shutdown(void)
{
    free(F.music.score);
    free(F.rdbt);
    free(F.rdb);
    if(F.lock_init)synth_lock_destroy(&F.lock);
    memset(&F,0,sizeof(F));
}

void avp_jaguar_fullsynth_meter_samples(s32 *left,s32 *right)
{
    s32 l=0,r=0;
    if(F.lock_init){
        synth_lock_enter(&F.lock);
        l=F.meter_l;r=F.meter_r;
        synth_lock_leave(&F.lock);
    }
    if(left)*left=l;
    if(right)*right=r;
}

void avp_jaguar_fullsynth_mix(int16_t *buf,unsigned frames)
{
    unsigned f,i;
    if(!buf||!frames)return;
    memset(buf,0,(size_t)frames*2u*sizeof(*buf));
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
#ifdef AVP_LOCAL_MULTIPLAYER
    if(F.mp_session_paused){
        synth_lock_leave(&F.lock);
        return; /* silence without advancing any music or effect voice */
    }
    /* Compute one immutable stereo location per bot voice per audio chunk.
     * PCM sample generation continues even when the bot is out of range;
     * a returning bot is audible at its current original synthesis phase. */
    unsigned mp_gain_l[AVP_HOST_SFX_SLOTS],mp_gain_r[AVP_HOST_SFX_SLOTS];
    for(i=0;i<AVP_HOST_SFX_SLOTS;i++){
        mp_gain_l[i]=mp_gain_r[i]=256u;
        if(F.mp_spatial_active && F.v[i].mp_source_active &&
           F.v[i].mp_source_player<AVP_LOCAL_MP_MAX_ACTORS &&
           (F.mp_bot_mask & (1u<<F.v[i].mp_source_player))){
            const AvpMpSpatialGain gains=avp_mp_spatial_gain(
                F.v[i].mp_source_player,F.v[i].mp_source_level,
                F.v[i].mp_source_x,F.v[i].mp_source_y,F.mp_listeners);
            mp_gain_l[i]=gains.left;mp_gain_r[i]=gains.right;
        }
    }
#endif
    for(f=0;f<frames;f++){
        /* The original stereo ADD destinations are 32-bit r26/r9.  Use
         * defined unsigned addition so the hardware carry is discarded. */
        u32 L=0,R=0;
        s32 left,right;
        if(F.music.active&&F.music.pit_den){
            F.music.pit_accum+=26590906ull;
            while(F.music.pit_accum>=F.music.pit_den){F.music.pit_accum-=F.music.pit_den;music_tick();}
            if(F.mix_music_on && F.mix_gain[0]==100u){
                music_mix_one(&L,&R);
            }else{
                u32 ml=0,mr=0;
                music_mix_one(&ml,&mr); /* MIDI/voice phase still advances */
                if(F.mix_music_on && F.mix_gain[0]){
                    s32 sl=ml<=0x7fffffffu?(s32)ml:(s32)((int64_t)ml-0x100000000ll);
                    s32 sr=mr<=0x7fffffffu?(s32)mr:(s32)((int64_t)mr-0x100000000ll);
                    L+=(u32)((int64_t)sl*(int64_t)F.mix_gain[0]/100);
                    R+=(u32)((int64_t)sr*(int64_t)F.mix_gain[0]/100);
                }
            }
        }
        for(i=0;i<AVP_HOST_SFX_SLOTS;i++){
            AvpHostedVoice *v=&F.v[i];
            s32 s;u32 pan;int64_t l,r;
            if(!v->synth.active){
#ifdef AVP_LOCAL_MULTIPLAYER
                v->mp_meter_l=v->mp_meter_r=0;
#endif
                continue;
            }
            s=synth_sample(&v->synth);
            if(!v->synth.active&&s==0){
#ifdef AVP_LOCAL_MULTIPLAYER
                v->mp_meter_l=v->mp_meter_r=0;
#endif
                continue;
            }
            pan=v->synth.p[19];
            /* Original pan coefficients are full 32-bit registers until
             * signed-low-16 IMULT, including the 0x7fff-pan complement. */
            r=(int64_t)dsp_low16_signed((u32)s)*dsp_low16_signed(pan);
            l=(int64_t)dsp_low16_signed((u32)s)*dsp_low16_signed(0x7fffu-pan);
            /* Extra per-voice gain is a PC-only control. Unity is bypassed,
             * while adjusted gains retain their previous integer behavior. */
            if(v->gain_l!=0x7fff)l=avp_jerry_asr64(l*v->gain_l,15u);
            if(v->gain_r!=0x7fff)r=avp_jerry_asr64(r*v->gain_r,15u);
#ifdef AVP_LOCAL_MULTIPLAYER
            /* Predator's per-player waveform already filters the source
             * by listener distance. Save the ORIGINAL voice sample before
             * the separate host stereo routing to avoid double fading. */
            v->mp_meter_l=(s32)avp_jerry_asr64(l,15u);
            v->mp_meter_r=(s32)avp_jerry_asr64(r,15u);
            /* Bot-only positional attenuation happens after the authentic
             * DSP channel sample, never by modifying the original voice. */
            if(mp_gain_l[i]!=256u)l=avp_jerry_asr64(l*(int64_t)mp_gain_l[i],8u);
            if(mp_gain_r[i]!=256u)r=avp_jerry_asr64(r*(int64_t)mp_gain_r[i],8u);
#endif
            {
                const unsigned group=host_sound_mix_group(i,v->sound_id);
                const unsigned gain=((group==2u&&!F.mix_ambient_on)||
                                     (group==4u&&!F.mix_hissing_on))?0u:F.mix_gain[group];
                if(gain==100u){L+=(u32)l;R+=(u32)r;}
                else if(gain){L+=(u32)(l*(int64_t)gain/100);
                              R+=(u32)(r*(int64_t)gain/100);}
            }
        }
        /* Convert the two's-complement channel registers without relying on
         * implementation-defined unsigned-to-signed narrowing. */
        left=L<=0x7fffffffu?(s32)L:(s32)((int64_t)L-0x100000000ll);
        right=R<=0x7fffffffu?(s32)R:(s32)((int64_t)R-0x100000000ll);
        /* At F1B100/102 the DSP shifts 32-bit channel registers; at
         * F1B10C/10E IMULT consumes only the signed low 16 bits before the
         * second shift at F1B110/112. Do not replace IMULT by a 64-bit host
         * multiplication of the entire normalized accumulator. */
        left=(s32)avp_jerry_asr64(left,15u);
        right=(s32)avp_jerry_asr64(right,15u);
        left=(s32)avp_jerry_asr64((int64_t)dsp_low16_signed((u32)left)*(s16)F.master_volume,15u);
        right=(s32)avp_jerry_asr64((int64_t)dsp_low16_signed((u32)right)*(s16)F.master_volume,15u);
        /* Keep the existing final host PCM saturation (DAC equivalence is
         * not yet independently proved). */
        if(left>32767)left=32767;
        if(left<-32768)left=-32768;
        if(right>32767)right=32767;
        if(right<-32768)right=-32768;
        buf[f*2u]=(int16_t)left;
        buf[f*2u+1u]=(int16_t)right;
        F.meter_l=left;
        F.meter_r=right;
    }
    synth_lock_leave(&F.lock);
}

static void install_host_voice(unsigned slot,const AvpSynthVoice *src,u32 logical_handle,unsigned game_voice,unsigned sound_id)
{
    AvpHostedVoice nv;
    if(slot>=AVP_HOST_SFX_SLOTS||!src||!src->active||!F.lock_init)return;
    memset(&nv,0,sizeof(nv));
    nv.synth=*src;
    nv.gain_l=nv.gain_r=0x7fff;
    nv.logical_handle=logical_handle;nv.game_voice=game_voice;nv.sound_id=sound_id;
    synth_lock_enter(&F.lock);
#ifdef AVP_LOCAL_MULTIPLAYER
    nv.mp_source_active=F.mp_source_active;
    nv.mp_source_player=F.mp_source_player;
    nv.mp_source_level=F.mp_source_level;
    nv.mp_source_x=F.mp_source_x;nv.mp_source_y=F.mp_source_y;
#endif
    F.v[slot]=nv;
    synth_lock_leave(&F.lock);
}

/* MUSIC.o TFX/DoEffect $9C6C-$9C80 and its store path $9CE8-$9D4E (also
 * reached by PlacedEf $9C3A).  The caller's D1 word selects the priority:
 * TST.W D1; BPL keeps a non-negative caller word, otherwise D1 is the module
 * long at +4 (sampler) or +$18 (type 4).  The store then rewrites the LOW
 * WORD of that same DSP voice long, keeping its high word (loop flag):
 *   type 32/44: MOVE.W D1,D3            -> voice+4  low word = D1
 *   type 4:     MOVE.W D1,D3; LSR.W #2  -> voice+$18 low word = D1 >> 2
 * The DSP reads that word as the voice volume, so a type-4 effect starts at a
 * quarter of its module word and an explicit caller priority (MJP Do_Escape
 * passes $2600) replaces a sampler's module volume. */
static void tfx_store_volume(AvpSynthVoice *v,s32 d1)
{
    u32 *w=(v->p[0]==4u)?&v->p[6]:&v->p[1];
    u16 word=((d1&0x8000)==0)?(u16)d1:(u16)(*w&0xffffu);
    if(v->p[0]==4u)word=(u16)(word>>2);
    *w=(*w&0xffff0000u)|word;
    v->orig_volume=*w&0x7fffffffu;
}

static void start_voice(unsigned slot,unsigned id,s32 left,s32 right,int force_loop,u32 logical_handle,unsigned game_voice,s32 d1)
{
    AvpSynthVoice v;
    (void)left;(void)right;
    if(slot>=AVP_HOST_SFX_SLOTS||!F.rom||!F.lock_init||!load_sfx_descriptor(id,&v))return;
    tfx_store_volume(&v,d1);
    if(force_loop>=0){
        if(v.p[0]==4u){
            if(force_loop)v.p[6]|=0x80000000u;
            else v.p[6]&=0x7fffffffu;
        }else{
            if(force_loop)v.p[1]|=0x80000000u;
            else v.p[1]&=0x7fffffffu;
        }
    }
    install_host_voice(slot,&v,logical_handle,game_voice,id);
}

void avp_jaguar_fullsynth_play_tracker(unsigned id,u32 volume,u32 pitch,u32 env_rate,u32 mod_depth)
{
    AvpSynthVoice v;
    unsigned slot=9u+(F.rr++%7u);
    if(!F.rom||!F.lock_init||!load_sfx_descriptor(id,&v))return;
    /* HUD.S mutates FX_trac1 in place immediately before issuing SFX trac1.
       Preserve those exact four longword overrides on the per-playback copy so
       the immutable RDB image remains untouched. */
    if(v.p[0]==4u){
        v.p[6]=(v.p[6]&0x80000000u)|(volume&0x7fffffffu);
        v.p[4]=pitch;
        v.p[11]=env_rate;
        v.p[9]=mod_depth;
        v.orig_pitch=v.p[4];
        v.orig_volume=v.p[6]&0x7fffffffu;
    }
    /* `SFX trac1` then enters TFX with D1=-1: the HUD-written volume word is
     * stored through the same type-4 LSR.W #2 as every other effect. */
    tfx_store_volume(&v,-1);
    install_host_voice(slot,&v,0u,~0u,id);
}

/* AMP.S::generate writes to the reserved alarm voice (CPU voice 2)
 * volume/loop word and rate word.  Keep its phase and sound handle intact;
 * stale generator updates must never alter another alarm. */
void avp_jaguar_fullsynth_update_generator_alarm(s32 volume,s32 pitch)
{
    AvpHostedVoice *v;
    if(!F.lock_init)return;
    if(volume<0)volume=0;
    if(volume>0x7fff)volume=0x7fff;
    synth_lock_enter(&F.lock);
    v=&F.v[3u]; /* Hosted slot = 1 + AVPSOUND CPU voice index 2 */
    if(v->synth.active&&v->logical_handle&&v->game_voice==2u&&
       v->sound_id==AVP_FX_FORCE&&v->synth.p[0]==32u){
        v->synth.p[1]=0x80000000u|(u32)volume;
        v->synth.p[3]=(u32)pitch;
    }
    synth_lock_leave(&F.lock);
}

/* Runtime PC mix levels are kept on the synthesis lock so the audio thread
 * observes a consistent snapshot. Muting does not stop original time/voices. */
void avp_jaguar_fullsynth_set_audio_mix(unsigned music,unsigned sfx,
    unsigned ambience,unsigned voice,unsigned hissing,
    int music_on,int ambience_on,int hissing_on)
{
    const unsigned levels[]={music,sfx,ambience,voice,hissing};
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    for(unsigned i=0;i<5u;i++)F.mix_gain[i]=levels[i]>100u?100u:levels[i];
    F.mix_music_on=!!music_on;
    F.mix_ambient_on=!!ambience_on;
    F.mix_hissing_on=!!hissing_on;
    synth_lock_leave(&F.lock);
}

/* Original soundtrack and environmental loops have explicit synth lanes;
 * additional voice-only descriptors (alerts, speech, creature calls, pain)
 * are classified by the retail RDB descriptor address, not the slot. */
static unsigned host_sound_mix_group(unsigned slot,unsigned sound_id)
{
    /* PLAYER.S::BGSounds randomly dispatches precisely these two effects.
     * Keep their live PCM independent of the level ambience and SFX. */
    if(slot==0u)return 2u; /* reserved continuous level-ambience lane */
    if(sound_id==AVP_FX_AMB2 || sound_id==AVP_FX_AMB3)return 4u;
    if(sound_id==AVP_FX_AMB11 || sound_id==AVP_FX_AMB4X ||
       sound_id==AVP_FX_AMB2X)return 2u;
    switch(sound_id){
    case AVP_FX_ANYTI:case AVP_FX_CMON2:case AVP_FX_WHATO:
    case AVP_FX_LAFF:case AVP_FX_2MINB:case AVP_FX_1MINB:
    case AVP_FX_10B:case AVP_FX_9B:case AVP_FX_8B:case AVP_FX_7B:
    case AVP_FX_6B:case AVP_FX_B5:case AVP_FX_B4:case AVP_FX_B3:
    case AVP_FX_B2:case AVP_FX_B1:case AVP_FX_ASHRI:
    case AVP_FX_PREDA:case AVP_FX_PREDE:case AVP_FX_DEATH:
    case AVP_FX_NALN1:case AVP_FX_NALN2:case AVP_FX_XALN1:
    case AVP_FX_XALN2:case AVP_FX_UGH2A:case AVP_FX_UGH2B:
    case AVP_FX_UHH3A:case AVP_FX_UHH3B:case AVP_FX_UHH3C:
        return 3u;
    default:return 1u;
    }
}

void avp_jaguar_fullsynth_set_master_volume(u32 volume)
{
    if(!F.lock_init)return;
    if(volume>0x7fffu)volume=0x7fffu;
    synth_lock_enter(&F.lock);F.master_volume=volume;synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_pause_save(void)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    F.paused[0]=F.v[2u]; /* CPU voice 1 */
    F.paused[1]=F.v[3u]; /* CPU voice 2: destruction alarm */
    F.paused_valid=1;
    synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_pause_restore(u32 weapon_handle,u32 alarm_handle)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    if(F.paused_valid){
        const u32 handles[2]={weapon_handle,alarm_handle};
        unsigned i;
        for(i=0;i<2u;i++){
            const AvpHostedVoice *saved=&F.paused[i];
            if(handles[i]&&saved->synth.active&&
               saved->logical_handle==handles[i]&&saved->game_voice==i+1u)
                F.v[i+2u]=*saved;
        }
    }
    memset(F.paused,0,sizeof(F.paused));
    F.paused_valid=0;
    synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_stop_all(void)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);memset(F.v,0,sizeof(F.v));synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_stop_ambient(void)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);memset(&F.v[0],0,sizeof(F.v[0]));synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_start_ambient(unsigned level)
{
    u32 id;
    /* Retail RDB.DTA starts with the exact 15-entry level ambient descriptor
       table at source offset 0x130. Resolve it from the user's ROM rather than
       embedding sample data or inventing a level mapping. */
    if(level<1u||level>15u||!load_rdb()||F.rdbn<0x130u+15u*4u)return;
    id=be32(F.rdb+0x130u+(level-1u)*4u);
    avp_jaguar_fullsynth_stop_ambient();
    start_voice(0u,id,0,0,1,0u,0u,-1);
}

void avp_jaguar_fullsynth_play_explosion(s32 rate)
{
    /* A rate of -1 means the original CMP #40 failed; playback still
     * happens with the previous value from the mutable RDB descriptor. */
    if(rate>=0){F.explosion_rate=(u32)rate;F.explosion_rate_written=1;}
    avp_jaguar_fullsynth_play(AVP_FX_XP1B1,0,0);
}

void avp_jaguar_fullsynth_play(unsigned id,s32 left,s32 right)
{
    unsigned slot=9u+(F.rr++%7u);
    start_voice(slot,id,left,right,-1,0u,~0u,-1);
}

/* An effect started with an explicit TFX D1 priority word (MJP Do_Escape). */
void avp_jaguar_fullsynth_play_priority(unsigned id,s32 priority)
{
    unsigned slot=9u+(F.rr++%7u);
    start_voice(slot,id,0,0,-1,0u,~0u,priority);
}

#ifdef AVP_LOCAL_MULTIPLAYER
void avp_jaguar_fullsynth_mp_set_paused(int paused)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    F.mp_session_paused=!!paused;
    synth_lock_leave(&F.lock);
}
void avp_jaguar_fullsynth_mp_set_listeners(const AvpMpSpatialListener listeners[4],
                                            unsigned bot_mask)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    if(listeners){
        memcpy(F.mp_listeners,listeners,sizeof F.mp_listeners);
        F.mp_bot_mask=bot_mask&((1u<<AVP_LOCAL_MP_MAX_ACTORS)-1u);
        F.mp_spatial_active=1u;
    }else{
        memset(F.mp_listeners,0,sizeof F.mp_listeners);
        F.mp_bot_mask=0u;F.mp_spatial_active=0u;
    }
    synth_lock_leave(&F.lock);
}
void avp_jaguar_fullsynth_mp_set_sound_source(unsigned player,unsigned level,s32 x,s32 y)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    F.mp_source_active=(player<AVP_LOCAL_MP_MAX_ACTORS && level>=1u && level<=15u);
    F.mp_source_player=player;F.mp_source_level=level;
    F.mp_source_x=x;F.mp_source_y=y;
    /* A bot can move while a real original weapon loop is playing. Move
     * only looped voices; finite gunshots remain at their firing location. */
    if(F.mp_source_active)for(unsigned i=1u;i<AVP_HOST_SFX_SLOTS;i++){
        AvpHostedVoice *v=&F.v[i];
        unsigned looping;
        if(!v->synth.active||!v->mp_source_active ||
           v->mp_source_player!=player)continue;
        looping=v->synth.p[0]==4u ? !!(v->synth.p[6]&0x80000000u) :
                                          !!(v->synth.p[1]&0x80000000u);
        if(looping){
            v->mp_source_level=level;
            v->mp_source_x=x;v->mp_source_y=y;
        }
    }
    synth_lock_leave(&F.lock);
}
void avp_jaguar_fullsynth_mp_clear_sound_source(void)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    F.mp_source_active=0u;
    synth_lock_leave(&F.lock);
}
void avp_jaguar_fullsynth_mp_meter_near(unsigned listener,unsigned level,s32 x,s32 y,
                                        s32 *left,s32 *right)
{
    int64_t l=0,r=0;
    if(F.lock_init){
        synth_lock_enter(&F.lock);
        for(unsigned i=0;i<AVP_HOST_SFX_SLOTS;i++){
            const AvpHostedVoice *voice=&F.v[i];
            unsigned gain;
            if(!voice->mp_source_active||!voice->synth.active)continue;
            gain=avp_mp_waveform_gain(voice->mp_source_player,listener,
                (int)voice->mp_source_level,(int)level,voice->mp_source_x,
                voice->mp_source_y,x,y);
            l+=(int64_t)voice->mp_meter_l*(int64_t)gain/256;
            r+=(int64_t)voice->mp_meter_r*(int64_t)gain/256;
        }
        synth_lock_leave(&F.lock);
    }
    if(l>32767)l=32767;
    if(l<-32768)l=-32768;
    if(r>32767)r=32767;
    if(r<-32768)r=-32768;
    if(left)*left=(s32)l;
    if(right)*right=(s32)r;
}
uint64_t avp_jaguar_fullsynth_mp_weapon_start_count(unsigned player)
{
    uint64_t result=0;
    if(player>=AVP_LOCAL_MP_MAX_ACTORS||!F.lock_init)return 0;
    synth_lock_enter(&F.lock);
    result=F.mp_weapon_starts[player];
    synth_lock_leave(&F.lock);
    return result;
}

void avp_jaguar_fullsynth_mp_set_weapon_owner(unsigned player)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    F.mp_weapon_owner=player<AVP_LOCAL_MP_MAX_ACTORS?player:0u;
    synth_lock_leave(&F.lock);
}

#endif

void avp_jaguar_fullsynth_play_handle(unsigned id,u32 handle,unsigned voice,s32 left,s32 right)
{
    unsigned slot=1u+(voice&7u);
    /* Keep Jaguar CPU voice arbitration original; only the hosted waveform
     * for reserved weapon voice 1 needs independent lanes for co-op actors.
     * P1 continues in original DSP slot 2; P2..4 use PCM lanes 16..18. */
#ifdef AVP_LOCAL_MULTIPLAYER
    if((voice&7u)==1u && F.mp_weapon_owner>0u && F.mp_weapon_owner<AVP_LOCAL_MP_MAX_ACTORS)
        slot=15u+F.mp_weapon_owner;
    if((voice&7u)==1u && F.lock_init){
        synth_lock_enter(&F.lock);
        ++F.mp_weapon_starts[F.mp_weapon_owner<AVP_LOCAL_MP_MAX_ACTORS?F.mp_weapon_owner:0u];
        synth_lock_leave(&F.lock);
    }
#endif
    start_voice(slot,id,left,right,-1,handle,voice&7u,-1);
}

int avp_jaguar_fullsynth_handle_active(u32 handle)
{
    unsigned i;int active=0;
    if(!handle||!F.lock_init)return 0;
    synth_lock_enter(&F.lock);
    for(i=1u;i<AVP_HOST_SFX_SLOTS;i++){
        if(F.v[i].synth.active&&F.v[i].logical_handle==handle){active=1;break;}
    }
    synth_lock_leave(&F.lock);
    return active;
}

/* Jaguar AVPSOUND's type field is updated by DSP completion even after
 * StopLoop has cleared VIdent. The hosted owner is the 68000 voice index,
 * so query that actual mixer slot under the mixer lock. */
int avp_jaguar_fullsynth_voice_active(unsigned voice)
{
    int active=0;
    if(voice>=8u||!F.lock_init)return 0;
    synth_lock_enter(&F.lock);
    active=F.v[1u+voice].synth.active;
    synth_lock_leave(&F.lock);
    return active;
}

void avp_jaguar_fullsynth_stop_handle(u32 handle)
{
    unsigned i;
    if(!handle||!F.lock_init)return;
    synth_lock_enter(&F.lock);
    for(i=1;i<AVP_HOST_SFX_SLOTS;i++)if(F.v[i].synth.active&&F.v[i].logical_handle==handle){memset(&F.v[i],0,sizeof(F.v[i]));break;}
    synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_unloop_handle(u32 handle)
{
    unsigned i;
    if(!handle||!F.lock_init)return;
    synth_lock_enter(&F.lock);
    for(i=1;i<AVP_HOST_SFX_SLOTS;i++)if(F.v[i].synth.active&&F.v[i].logical_handle==handle){{
        if(F.v[i].synth.p[0]==4u){F.v[i].synth.p[6]&=0x7fffffffu;F.v[i].synth.p[12]=F.v[i].synth.p[15];}
        else{F.v[i].synth.p[1]&=0x7fffffffu;F.v[i].synth.p[4]=F.v[i].synth.p[8];}
        break;
    }}
    synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_mod_handle(u32 handle,s32 value)
{
    unsigned i;
    if(!handle||!F.lock_init)return;
    synth_lock_enter(&F.lock);
    for(i=1;i<AVP_HOST_SFX_SLOTS;i++)if(F.v[i].synth.active&&F.v[i].logical_handle==handle){
        int g=value;if(g<0)g=-g;if(g>0x7fff)g=0x7fff;F.v[i].gain_l=F.v[i].gain_r=g;break;
    }
    synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_position_handle(u32 handle,s32 x,s32 y)
{
    unsigned i;
    double dx,dy,ang,side,dist,pan,att;
    if(!handle||!F.lock_init)return;
    dx=((double)x-(double)(s32)x_pos)/65536.0;
    dy=((double)y-(double)(s32)y_pos)/65536.0;
    ang=-(double)((u16)(centre_angle>>16))*(2.0*M_PI/65536.0);
    side=dx*(-sin(ang))+dy*cos(ang);dist=sqrt(dx*dx+dy*dy);
    pan=side/8.0;if(pan<-1.0)pan=-1.0;if(pan>1.0)pan=1.0;
    att=1.0/(1.0+dist*0.18);
    synth_lock_enter(&F.lock);
    for(i=1;i<AVP_HOST_SFX_SLOTS;i++)if(F.v[i].synth.active&&F.v[i].logical_handle==handle){
        int l=(int)(32767.0*att*(pan>0.0?1.0-pan:1.0));
        int r=(int)(32767.0*att*(pan<0.0?1.0+pan:1.0));
        if(l<0)l=0;
        if(l>0x7fff)l=0x7fff;
        if(r<0)r=0;
        if(r>0x7fff)r=0x7fff;
        F.v[i].gain_l=l;F.v[i].gain_r=r;break;
    }
    synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_stop_music(void)
{
    if(!F.lock_init)return;
    synth_lock_enter(&F.lock);
    free(F.music.score);memset(&F.music,0,sizeof(F.music));
    synth_lock_leave(&F.lock);
}

void avp_jaguar_fullsynth_start_music(unsigned id)
{
    const u8 *d,*score_src=NULL;size_t score_n=0,o;u32 score_ref,patch_addr;u16 div,pre;
    u8 *copy=NULL;
    if(!F.lock_init||id<0x20300u||!load_rdb())return;
    o=(size_t)(id-0x20300u);if(o+20u>F.rdbn)return;d=F.rdb+o;
    score_ref=be32(d);patch_addr=be32(d+4u);div=(u16)((d[16]<<8)|d[17]);pre=(u16)((d[18]<<8)|d[19]);
    /* The retail high-bit score references ($80815F08, $80815F10) point
     * to cartridge POINTER CELLS, not to the score bytes.  For example the
     * title descriptor's $815F10 cell contains $AF46E4.  Direct cartridge
     * score refs remain supported for synthetic fixtures and other ports. */
    if(!music_score_ref(score_ref,&score_src,&score_n)){
        if(score_ref<0x20300u)return;
        {
            size_t so=(size_t)(score_ref-0x20300u),j;
            if(so>=F.rdbn)return;
            score_src=F.rdb+so;score_n=F.rdbn-so;
            for(j=0;j+8u<=score_n;j+=8u){
                if(be32(score_src+j)==0x7fffffffu){score_n=j+8u;break;}
            }
            if(j+8u>score_n)return;
        }
    }
    if(score_n<8u||patch_addr<0x20300u)return;
    copy=(u8*)malloc(score_n);if(!copy)return;memcpy(copy,score_src,score_n);
    synth_lock_enter(&F.lock);
    free(F.music.score);memset(&F.music,0,sizeof(F.music));
    F.music.score=copy;F.music.score_len=score_n;F.music.patch_addr=patch_addr;F.music.descriptor=id;F.music.active=1;
    /* JERRY PIT: period=(prescaler+1)*(divider+1)/26.590906 MHz.  The
       mixer runs at the retail FullSynth sample rate (20774 Hz). */
    F.music.pit_den=(uint64_t)(pre+1u)*(uint64_t)(div+1u)*20774ull;
    synth_lock_leave(&F.lock);
}
