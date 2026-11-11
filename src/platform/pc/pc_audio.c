#include <stdlib.h>
#include "pc/pc_audio.h"
#include "jaguar/host/fullsynth_host.h"
#include "pc/pc_audio_layout.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Minimal dynamically-loaded ALSA ABI keeps release builds independent of
   distro development headers while using the normal libasound.so.2 runtime. */
typedef struct _snd_pcm snd_pcm_t;
typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;
typedef int (*fn_open)(snd_pcm_t**,const char*,int,int);
typedef int (*fn_set_params)(snd_pcm_t*,int,int,unsigned,unsigned,int,unsigned);
typedef snd_pcm_sframes_t (*fn_writei)(snd_pcm_t*,const void*,snd_pcm_uframes_t);
typedef int (*fn_prepare)(snd_pcm_t*);
typedef int (*fn_close)(snd_pcm_t*);

/* libpulse-simple is an optional fallback for PipeWire/PulseAudio desktops
   whose ALSA 'default' PCM is unavailable.  Use its runtime ABI directly so
   users do not need libpulse development headers or new build dependencies. */
typedef struct pa_simple pa_simple;
typedef struct AvpPaSampleSpec { int format; uint32_t rate; uint8_t channels; } AvpPaSampleSpec;
typedef pa_simple *(*fn_pa_new)(const char*,const char*,int,const char*,const char*,
                                const AvpPaSampleSpec*,const void*,const void*,int*);
typedef int (*fn_pa_write)(pa_simple*,const void*,size_t,int*);
typedef void (*fn_pa_free)(pa_simple*);

static struct {
    void *so;
    snd_pcm_t *pcm;
    fn_open open_pcm;
    fn_set_params set_params;
    fn_writei writei;
    fn_prepare prepare;
    fn_close close;
    void *pulse_so;
    pa_simple *pulse;
    fn_pa_new pulse_new;
    fn_pa_write pulse_write;
    fn_pa_free pulse_free;
    pthread_t thread;
    int run,ok;
    unsigned output_channels,opened_mode,opened_subs,active_channels;
    unsigned requested_mode,requested_subs;
    AvpAudioBassState bass;

} D={0};

static int open_pulse(unsigned channels);
static int open_alsa(unsigned channels);
static void reopen_output(void);

/* Deliver the complete synthesized block, including after an ALSA
 * underrun.  A successful snd_pcm_prepare() resets the PCM stream but does
 * not consume any frames from the caller's buffer.  Discarding the unwritten
 * tail here would skip already-advanced FullSynth samples.  Bound repeated
 * no-progress recovery so a disconnected device cannot spin indefinitely. */
static void write_alsa_block(const int16_t *buf,unsigned frames)
{
    unsigned written=0u,stalls=0u;
    while(D.run&&written<frames){
        snd_pcm_sframes_t n=D.writei(D.pcm,buf+written*D.output_channels,frames-written);
        if(n>0){
            written+=(unsigned)n;
            stalls=0u;
            continue;
        }
        if(n<0&&(!D.prepare||D.prepare(D.pcm)<0))break;
        if(++stalls>=4u)break;
        {struct timespec ts={0,2000000L};(void)nanosleep(&ts,NULL);}
    }
    /* A PCM that continuously rejects blocks may have been disconnected.
     * Retire its handle and let the worker try PulseAudio rather than staying
     * attached to a dead sink for the remainder of the session. */
    if(D.run&&written<frames&&stalls>=4u&&D.close){
        D.close(D.pcm);
        D.pcm=NULL;
        __atomic_store_n(&D.active_channels,0u,__ATOMIC_RELEASE);
    }
    /* With a permanently failing PCM sink, FullSynth must still advance at
     * its nominal rate rather than busy-looping on undeliverable buffers. */
    if(D.run&&written<frames){
        uint64_t ns=(uint64_t)(frames-written)*1000000000ull/20774ull;
        struct timespec ts={(time_t)(ns/1000000000ull),(long)(ns%1000000000ull)};
        (void)nanosleep(&ts,NULL);
    }
}

/* A PulseAudio stream may open successfully and subsequently fail (server
 * restart, device removal).  pa_simple_write is a blocking all-or-error
 * operation; there is no caller-visible frame count to retry safely.  Do
 * not retain the broken stream or pump 512 samples every fixed 10 ms -- at
 * 20774 Hz each block represents about 24.65 ms of game sound.  Close the
 * failed stream and advance through the existing paced, silent-sink path.
 * Account for time already spent inside pa_simple_write before sleeping. */
static void write_pulse_block(const int16_t *buf,unsigned frames)
{
    struct timespec started,finished,ts;
    uint64_t elapsed,block_ns;
    int result;
    (void)clock_gettime(CLOCK_MONOTONIC,&started);
    result=D.pulse_write(D.pulse,buf,(size_t)frames*D.output_channels*sizeof(*buf),NULL);
    if(result>=0)return;
    if(D.pulse_free)D.pulse_free(D.pulse);
    D.pulse=NULL;
    __atomic_store_n(&D.active_channels,0u,__ATOMIC_RELEASE);
    if(!D.run)return;
    (void)clock_gettime(CLOCK_MONOTONIC,&finished);
    block_ns=(uint64_t)frames*1000000000ull/20774ull;
    elapsed=(uint64_t)(finished.tv_sec-started.tv_sec)*1000000000ull;
    if(finished.tv_nsec>=started.tv_nsec)
        elapsed+=(uint64_t)(finished.tv_nsec-started.tv_nsec);
    else elapsed-=(uint64_t)(started.tv_nsec-finished.tv_nsec);
    if(elapsed>=block_ns)return;
    block_ns-=elapsed;
    ts.tv_sec=(time_t)(block_ns/1000000000ull);
    ts.tv_nsec=(long)(block_ns%1000000000ull);
    (void)nanosleep(&ts,NULL);
}

/* Only the audio worker changes sinks after initialization. UI writes atomic
 * requests; a device rejection falls back to honest stereo output. */
static int open_alsa(unsigned channels)
{
    if(!D.open_pcm||!D.set_params||!D.writei||!D.close)return 0;
    if(D.open_pcm(&D.pcm,"default",0,0)<0)return 0;
    if(D.set_params(D.pcm,2,3,channels,20774u,1,50000u)>=0)return 1;
    D.close(D.pcm);D.pcm=NULL;
    return 0;
}
static int open_pulse(unsigned channels)
{
    AvpPaSampleSpec spec={3,20774u,(uint8_t)channels};
    if(!D.pulse_so){
        D.pulse_so=dlopen("libpulse-simple.so.0",RTLD_NOW|RTLD_LOCAL);
        if(!D.pulse_so)return 0;
        *(void**)(&D.pulse_new)=dlsym(D.pulse_so,"pa_simple_new");
        *(void**)(&D.pulse_write)=dlsym(D.pulse_so,"pa_simple_write");
        *(void**)(&D.pulse_free)=dlsym(D.pulse_so,"pa_simple_free");
    }
    if(!D.pulse_new||!D.pulse_write||!D.pulse_free)return 0;
    D.pulse=D.pulse_new(NULL,"Alien vs Predator (Jaguar)",1,NULL,
                        "FullSynth",&spec,NULL,NULL,NULL);
    return D.pulse!=NULL;
}
static void reopen_output(void)
{
    unsigned mode=__atomic_load_n(&D.requested_mode,__ATOMIC_ACQUIRE);
    unsigned subs=__atomic_load_n(&D.requested_subs,__ATOMIC_ACQUIRE);
    unsigned requested=avp_audio_layout_channels(mode,subs);
    if(D.pcm){D.close(D.pcm);D.pcm=NULL;}
    if(D.pulse){D.pulse_free(D.pulse);D.pulse=NULL;}
    D.output_channels=requested;
    if(open_alsa(requested)||open_pulse(requested)){
        D.opened_mode=mode;D.opened_subs=subs;
    }else{
        D.output_channels=2u;
        (void)(open_alsa(2u)||open_pulse(2u));
        D.opened_mode=1u;D.opened_subs=0u;
    }
    __atomic_store_n(&D.active_channels,(D.pcm||D.pulse)?D.output_channels:0u,
                     __ATOMIC_RELEASE);
    D.bass.left=D.bass.right=0;
}
static void *audio_thread(void *unused)
{
    int16_t stereo[512u*2u],output[512u*AVP_AUDIO_MAX_CHANNELS];
    unsigned current_mode=~0u,current_subs=~0u;
    uint64_t next_retry_ns=0ull;
    (void)unused;
    while(D.run){
        unsigned req_mode=__atomic_load_n(&D.requested_mode,__ATOMIC_ACQUIRE);
        unsigned req_subs=__atomic_load_n(&D.requested_subs,__ATOMIC_ACQUIRE);
        /* Do not repeatedly reopen an unavailable sink for every 25 ms audio
         * block. Retry missing output at most every five seconds, but respond
         * immediately when the user changes the requested speaker layout. */
        struct timespec now;
        uint64_t now_ns;
        (void)clock_gettime(CLOCK_MONOTONIC,&now);
        now_ns=(uint64_t)now.tv_sec*1000000000ull+(uint64_t)now.tv_nsec;
        if(req_mode!=current_mode||req_subs!=current_subs ||
           ((!D.pcm&&!D.pulse)&&now_ns>=next_retry_ns)){
            reopen_output();current_mode=req_mode;current_subs=req_subs;
            next_retry_ns=now_ns+5000000000ull;
        }
        avp_jaguar_fullsynth_mix(stereo,512u);
        avp_audio_layout_convert(stereo,output,512u,D.opened_mode,
                                  D.opened_subs,&D.bass);
        /* Maintain voice time when device is absent, with paced silent sink. */
        if(!D.pcm&&!D.pulse){
            struct timespec ts={0,(long)(512000000000ull/20774ull)};
            (void)nanosleep(&ts,NULL);
            continue;
        }
        if(D.pulse){write_pulse_block(output,512u);continue;}
        write_alsa_block(output,512u);
    }
    return NULL;
}

int avp_pc_audio_init(const AvpRomImage *rom)
{
    if(!avp_jaguar_fullsynth_init(rom))return 0;
    {const char *wave=getenv("AVP_JERRY_WAVE_ROM");
     int loaded=(wave&&wave[0])?avp_jaguar_fullsynth_load_wave_rom(wave):0;
     if(!loaded){
         /* Even an invalid environment override must not hide a valid,
          * user-provided original JERRY wave-ROM beside the executable. */
         /* The normal Linux Mint double-click path need not set cwd to
          * the executable folder. Look there before the cwd fallback. */
         char path[1024];
         ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1u);
         if(n>0&&(size_t)n<sizeof(path)){
             char *slash;
             path[n]=0;slash=strrchr(path,'/');
             if(slash&&((size_t)(slash-path)+sizeof("/jagwave.rom")<sizeof(path))){
                 memcpy(slash,"/jagwave.rom",sizeof("/jagwave.rom"));
                 loaded=avp_jaguar_fullsynth_load_wave_rom(path);
             }
         }
         if(!loaded)(void)avp_jaguar_fullsynth_load_wave_rom("jagwave.rom");
     }}
    D.so=dlopen("libasound.so.2",RTLD_NOW|RTLD_LOCAL);
    if(D.so){
        *(void**)(&D.open_pcm)=dlsym(D.so,"snd_pcm_open");
        *(void**)(&D.set_params)=dlsym(D.so,"snd_pcm_set_params");
        *(void**)(&D.writei)=dlsym(D.so,"snd_pcm_writei");
        *(void**)(&D.prepare)=dlsym(D.so,"snd_pcm_prepare");
        *(void**)(&D.close)=dlsym(D.so,"snd_pcm_close");
    }
    D.requested_mode=1u;D.requested_subs=0u;
    D.opened_mode=1u;D.output_channels=2u;
    D.run=1;
    D.ok=pthread_create(&D.thread,NULL,audio_thread,NULL)==0;
    if(!D.ok){
        D.run=0;
        if(D.pcm&&D.close){D.close(D.pcm);D.pcm=NULL;}
        if(D.pulse&&D.pulse_free){D.pulse_free(D.pulse);D.pulse=NULL;}
        return 0;
    }
    return 1;
}

void avp_pc_audio_shutdown(void)
{
    if(D.run){D.run=0;if(D.ok)pthread_join(D.thread,NULL);}
    if(D.pcm&&D.close)D.close(D.pcm);
    if(D.pulse&&D.pulse_free)D.pulse_free(D.pulse);
    if(D.so)dlclose(D.so);
    if(D.pulse_so)dlclose(D.pulse_so);
    memset(&D,0,sizeof(D));
    avp_jaguar_fullsynth_shutdown();
}

void avp_pc_audio_play(unsigned id,s32 left,s32 right){avp_jaguar_fullsynth_play(id,left,right);}
void avp_pc_audio_play_priority(unsigned id,s32 priority){avp_jaguar_fullsynth_play_priority(id,priority);}
void avp_pc_audio_play_explosion(s32 rate)
{avp_jaguar_fullsynth_play_explosion(rate);}
void avp_pc_audio_update_generator_alarm(s32 volume,s32 pitch)
{avp_jaguar_fullsynth_update_generator_alarm(volume,pitch);}
void avp_pc_audio_play_tracker(unsigned id,u32 volume,u32 pitch,u32 env_rate,u32 mod_depth){avp_jaguar_fullsynth_play_tracker(id,volume,pitch,env_rate,mod_depth);}
void avp_pc_audio_play_handle(unsigned id,u32 handle,unsigned voice,s32 left,s32 right){avp_jaguar_fullsynth_play_handle(id,handle,voice,left,right);}
void avp_pc_audio_play_handle_d1(unsigned id,u32 handle,unsigned voice,s32 left,s32 right,s32 d1)
{avp_jaguar_fullsynth_play_handle_d1(id,handle,voice,left,right,d1);}
void avp_pc_audio_stop_handle(u32 handle){avp_jaguar_fullsynth_stop_handle(handle);}
int avp_pc_audio_handle_active(u32 handle){return avp_jaguar_fullsynth_handle_active(handle);}
int avp_pc_audio_voice_active(unsigned voice){return avp_jaguar_fullsynth_voice_active(voice);}
void avp_pc_audio_unloop_handle(u32 handle){avp_jaguar_fullsynth_unloop_handle(handle);}
void avp_pc_audio_mod_handle(u32 handle,s32 value){avp_jaguar_fullsynth_mod_handle(handle,value);}
void avp_pc_audio_position_handle(u32 handle,s32 x,s32 y){avp_jaguar_fullsynth_position_handle(handle,x,y);}
void avp_pc_audio_start_ambient(unsigned level){avp_jaguar_fullsynth_start_ambient(level);}
void avp_pc_audio_stop_ambient(void){avp_jaguar_fullsynth_stop_ambient();}
void avp_pc_audio_start_music(unsigned id){avp_jaguar_fullsynth_start_music(id);}
void avp_pc_audio_stop_music(void){avp_jaguar_fullsynth_stop_music();}
void avp_pc_audio_stop_all(void){avp_jaguar_fullsynth_stop_all();}
void avp_pc_audio_pause_save(void){avp_jaguar_fullsynth_pause_save();}
void avp_pc_audio_pause_restore(u32 weapon_handle,u32 alarm_handle)
{avp_jaguar_fullsynth_pause_restore(weapon_handle,alarm_handle);}
void avp_pc_audio_set_master_volume(u32 volume){avp_jaguar_fullsynth_set_master_volume(volume);}

void avp_pc_audio_set_mix(unsigned music,unsigned sfx,unsigned ambience,
                          unsigned voice,unsigned hissing,
                          int music_on,int ambience_on,int hissing_on)
{
    avp_jaguar_fullsynth_set_audio_mix(music,sfx,ambience,voice,hissing,
                                      music_on,ambience_on,hissing_on);
}
void avp_pc_audio_set_speaker_mode(unsigned mode,unsigned subs)
{
    if(mode>=AVP_AUDIO_LAYOUT_COUNT)mode=1u;
    if(subs>2u)subs=2u;
    __atomic_store_n(&D.requested_mode,mode,__ATOMIC_RELEASE);
    __atomic_store_n(&D.requested_subs,subs,__ATOMIC_RELEASE);
}
unsigned avp_pc_audio_active_channels(void)
{
    return __atomic_load_n(&D.active_channels,__ATOMIC_ACQUIRE);
}
