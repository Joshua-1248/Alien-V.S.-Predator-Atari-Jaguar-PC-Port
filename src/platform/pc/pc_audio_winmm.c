#include <stdlib.h>
#include "pc/pc_audio.h"
#include "jaguar/host/fullsynth_host.h"
#include "pc/pc_winmm_resample.h"
#include "pc/pc_audio_layout.h"
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <stdint.h>
#include <string.h>
/* Request native Jaguar rate first.  If the device rejects it, stream the
   unchanged FullSynth output through a continuous host-rate bridge instead
   of changing the mix clock (which would transpose sounds and music). */
enum { BLOCKS=4, SAMPLES=512, MAX_DEVICE_FRAMES=1280 };
static struct {
    HWAVEOUT device;
    HANDLE thread;
    volatile LONG running;
    WAVEHDR header[BLOCKS];
    unsigned device_rate,device_frames,opened_mode,opened_subs;
    volatile LONG request_revision,request_mode,request_subs,active_channels;
    AvpAudioBassState bass;
    AvpPcRateBridge bridge;
    AvpPcDeviceFrameClock frame_clock;
    int16_t pcm[BLOCKS][MAX_DEVICE_FRAMES*AVP_AUDIO_MAX_CHANNELS];
} W;
/* WinMM accepts standard PCM mono/stereo and (on compatible devices)
 * WAVEFORMATEXTENSIBLE multi-channel PCM. A failed format request falls back
 * to stereo; this is reported to the Audio menu as Active Output Channels. */
static int open_device_format(unsigned mode,unsigned subs)
{
    static const unsigned device_rates[]={AVP_SYNTH_RATE,48000u,44100u};
    static const GUID pcm_guid={0x00000001u,0x0000u,0x0010u,
        {0x80u,0x00u,0x00u,0xaau,0x00u,0x38u,0x9bu,0x71u}};
    WAVEFORMATEXTENSIBLE ext;
    WAVEFORMATEX *fmt=&ext.Format;
    unsigned chans=avp_audio_layout_channels(mode,subs),j;
    memset(&ext,0,sizeof ext);
    fmt->nChannels=(WORD)chans;
    fmt->wBitsPerSample=16u;
    fmt->nBlockAlign=(WORD)(chans*2u);
    fmt->wFormatTag=(chans>2u||subs>0u)?WAVE_FORMAT_EXTENSIBLE:WAVE_FORMAT_PCM;
    if(fmt->wFormatTag==WAVE_FORMAT_EXTENSIBLE){
        fmt->cbSize=(WORD)(sizeof ext-sizeof(WAVEFORMATEX));
        ext.Samples.wValidBitsPerSample=16u;
        ext.SubFormat=pcm_guid;
        /* Common Windows speaker masks for the standard 1..8ch beds.
         * Unsupported high-channel formats have no universally valid mask. */
        if(subs<=1u && chans<=8u){
            static const unsigned bed_masks[]={0,0x4u,0x3u,0x7u,0x33u,0x37u,0x137u,
                                                0x637u,0x737u};
            unsigned beds=avp_audio_layout_bed(mode);
            if(beds<=7u)ext.dwChannelMask=bed_masks[beds]+(subs?0x8u:0u);
            else if(beds==8u)ext.dwChannelMask=0x737u+(subs?0x8u:0u);
        }

    }
    for(j=0u;j<sizeof(device_rates)/sizeof(device_rates[0]);j++){
        fmt->nSamplesPerSec=device_rates[j];
        fmt->nAvgBytesPerSec=device_rates[j]*fmt->nBlockAlign;
        if(waveOutOpen(&W.device,WAVE_MAPPER,fmt,0,0,CALLBACK_NULL)==MMSYSERR_NOERROR){
            W.device_rate=device_rates[j];
            W.device_frames=(unsigned)(((uint64_t)SAMPLES*W.device_rate+
                                        AVP_SYNTH_RATE-1u)/AVP_SYNTH_RATE);
            if(W.device_frames>MAX_DEVICE_FRAMES){
                waveOutClose(W.device);W.device=NULL;continue;
            }
            avp_pc_device_frame_clock_init(&W.frame_clock,W.device_rate);
            if(W.device_rate!=AVP_SYNTH_RATE)
                avp_pc_rate_bridge_init(&W.bridge,W.device_rate);
            W.opened_mode=mode;W.opened_subs=subs;
            InterlockedExchange(&W.active_channels,(LONG)chans);
            W.bass.left=W.bass.right=0;
            return 1;
        }
    }
    return 0;
}
static void close_device(void)
{
    unsigned j;
    if(!W.device)return;
    waveOutReset(W.device);
    for(j=0u;j<BLOCKS;j++){
        if(W.header[j].dwFlags&WHDR_PREPARED)
            (void)waveOutUnprepareHeader(W.device,&W.header[j],sizeof(W.header[j]));
        memset(&W.header[j],0,sizeof W.header[j]);
    }
    waveOutClose(W.device);W.device=NULL;
    InterlockedExchange(&W.active_channels,0);
}
static DWORD WINAPI sound_thread(LPVOID unused)
{
    unsigned i=0u;LONG last_revision=-1;
    ULONGLONG next_retry_ms=0u;
    int16_t stereo[MAX_DEVICE_FRAMES*2u];
    (void)unused;
    while(InterlockedCompareExchange(&W.running,1,1)){
        LONG revision=InterlockedCompareExchange(&W.request_revision,0,0);
        /* Avoid endlessly probing missing WinMM hardware. An explicit
         * speaker change still retries immediately. */
        ULONGLONG now_ms=GetTickCount64();
        if(revision!=last_revision||(!W.device&&now_ms>=next_retry_ms)){
            unsigned mode=(unsigned)InterlockedCompareExchange(&W.request_mode,0,0);
            unsigned subs=(unsigned)InterlockedCompareExchange(&W.request_subs,0,0);
            close_device();
            if(!open_device_format(mode,subs))
                (void)open_device_format(1u,0u);
            last_revision=revision;next_retry_ms=now_ms+5000u;i=0u;
            if(!W.device){
                avp_jaguar_fullsynth_mix(stereo,SAMPLES);
                Sleep((DWORD)((uint64_t)SAMPLES*1000u/AVP_SYNTH_RATE));
                continue;
            }
        }
        {
            WAVEHDR *h=&W.header[i];
            if(h->dwFlags&WHDR_PREPARED){
                if(!(h->dwFlags&WHDR_DONE)){Sleep(1);continue;}
                if(waveOutUnprepareHeader(W.device,h,sizeof(*h))!=MMSYSERR_NOERROR){Sleep(1);continue;}
            }
            W.device_frames=avp_pc_device_frame_clock_next(&W.frame_clock);
            if(W.device_rate==AVP_SYNTH_RATE)
                avp_jaguar_fullsynth_mix(stereo,SAMPLES);
            else
                avp_pc_rate_bridge_render(&W.bridge,stereo,W.device_frames,
                                          avp_jaguar_fullsynth_mix);
            avp_audio_layout_convert(stereo,W.pcm[i],W.device_frames,
                  W.opened_mode,W.opened_subs,&W.bass);
            memset(h,0,sizeof(*h));h->lpData=(LPSTR)W.pcm[i];
            h->dwBufferLength=(DWORD)(W.device_frames*
                avp_audio_layout_channels(W.opened_mode,W.opened_subs)*sizeof(int16_t));
            if(waveOutPrepareHeader(W.device,h,sizeof(*h))==MMSYSERR_NOERROR)
                (void)waveOutWrite(W.device,h,sizeof(*h));
            i=(i+1u)%BLOCKS;
        }
    }
    return 0;
}
int avp_pc_audio_init(const AvpRomImage *rom)
{
    if(!avp_jaguar_fullsynth_init(rom))return 0;
    {const char *wave=getenv("AVP_JERRY_WAVE_ROM");
     if(wave&&wave[0]) (void)avp_jaguar_fullsynth_load_wave_rom(wave);
     else {
         char path[MAX_PATH];
         DWORD n=GetModuleFileNameA(NULL,path,(DWORD)sizeof(path));
         int loaded=0;
         if(n>0u&&n<sizeof(path)){
             char *slash,*backslash;size_t len;
             path[n]=0;slash=strrchr(path,'/');backslash=strrchr(path,'\\');
             if(!slash||(backslash&&backslash>slash))slash=backslash;
             if(slash){len=(size_t)(slash-path);
                 if(len+sizeof("\\jagwave.rom")<sizeof(path)){
                     memcpy(slash,"\\jagwave.rom",sizeof("\\jagwave.rom"));
                     loaded=avp_jaguar_fullsynth_load_wave_rom(path);
                 }
             }
         }
         if(!loaded)(void)avp_jaguar_fullsynth_load_wave_rom("jagwave.rom");
     }}
    memset(&W,0,sizeof(W));
    W.request_mode=1;W.request_subs=0;
    InterlockedExchange(&W.running,1);
    W.thread=CreateThread(NULL,0,sound_thread,NULL,0,NULL);
    if(!W.thread){InterlockedExchange(&W.running,0);avp_jaguar_fullsynth_shutdown();return 0;}
    return 1;
}
void avp_pc_audio_shutdown(void)
{
    InterlockedExchange(&W.running,0);
    if(W.thread){WaitForSingleObject(W.thread,INFINITE);CloseHandle(W.thread);}
    close_device();
    memset(&W,0,sizeof(W));avp_jaguar_fullsynth_shutdown();
}
void avp_pc_audio_play(unsigned id,s32 l,s32 r){avp_jaguar_fullsynth_play(id,l,r);}
void avp_pc_audio_play_priority(unsigned id,s32 priority){avp_jaguar_fullsynth_play_priority(id,priority);}
void avp_pc_audio_play_explosion(s32 rate)
{avp_jaguar_fullsynth_play_explosion(rate);}
void avp_pc_audio_update_generator_alarm(s32 volume,s32 pitch){avp_jaguar_fullsynth_update_generator_alarm(volume,pitch);}
void avp_pc_audio_play_tracker(unsigned id,u32 v,u32 p,u32 e,u32 m){avp_jaguar_fullsynth_play_tracker(id,v,p,e,m);}
void avp_pc_audio_play_handle(unsigned id,u32 h,unsigned voice,s32 l,s32 r){avp_jaguar_fullsynth_play_handle(id,h,voice,l,r);}
void avp_pc_audio_stop_handle(u32 h){avp_jaguar_fullsynth_stop_handle(h);}
int avp_pc_audio_handle_active(u32 h){return avp_jaguar_fullsynth_handle_active(h);}
int avp_pc_audio_voice_active(unsigned voice){return avp_jaguar_fullsynth_voice_active(voice);}
void avp_pc_audio_unloop_handle(u32 h){avp_jaguar_fullsynth_unloop_handle(h);}
void avp_pc_audio_mod_handle(u32 h,s32 v){avp_jaguar_fullsynth_mod_handle(h,v);}
void avp_pc_audio_position_handle(u32 h,s32 x,s32 y){avp_jaguar_fullsynth_position_handle(h,x,y);}
void avp_pc_audio_start_ambient(unsigned lev){avp_jaguar_fullsynth_start_ambient(lev);}
void avp_pc_audio_stop_ambient(void){avp_jaguar_fullsynth_stop_ambient();}
void avp_pc_audio_start_music(unsigned id){avp_jaguar_fullsynth_start_music(id);}
void avp_pc_audio_stop_music(void){avp_jaguar_fullsynth_stop_music();}
void avp_pc_audio_stop_all(void){avp_jaguar_fullsynth_stop_all();}
void avp_pc_audio_pause_save(void){avp_jaguar_fullsynth_pause_save();}
void avp_pc_audio_pause_restore(u32 weapon_handle,u32 alarm_handle)
{avp_jaguar_fullsynth_pause_restore(weapon_handle,alarm_handle);}
void avp_pc_audio_set_master_volume(u32 v){avp_jaguar_fullsynth_set_master_volume(v);}

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
    if((unsigned)InterlockedCompareExchange(&W.request_mode,0,0)!=mode ||
       (unsigned)InterlockedCompareExchange(&W.request_subs,0,0)!=subs){
        InterlockedExchange(&W.request_mode,(LONG)mode);
        InterlockedExchange(&W.request_subs,(LONG)subs);
        InterlockedIncrement(&W.request_revision);
    }
}
unsigned avp_pc_audio_active_channels(void)
{
    return (unsigned)InterlockedCompareExchange(&W.active_channels,0,0);
}
