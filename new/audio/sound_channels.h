#ifndef AVP_SOUND_CHANNELS_H
#define AVP_SOUND_CHANNELS_H
#define AVP_SOUND_CHANNELS_DEFAULT 8u
#define AVP_SOUND_CHANNELS_MAX 256u
#define AVP_SOUND_CHANNELS_STEP 8u
static inline unsigned avp_sound_channels_valid(unsigned n)
{
    return n>=AVP_SOUND_CHANNELS_DEFAULT && n<=AVP_SOUND_CHANNELS_MAX &&
           n%AVP_SOUND_CHANNELS_STEP==0u ? n : AVP_SOUND_CHANNELS_DEFAULT;
}
static inline unsigned avp_sound_channels_adjust(unsigned n,int dir)
{
    n=avp_sound_channels_valid(n);
    if(dir>0 && n<AVP_SOUND_CHANNELS_MAX)return n+AVP_SOUND_CHANNELS_STEP;
    if(dir<0 && n>AVP_SOUND_CHANNELS_DEFAULT)return n-AVP_SOUND_CHANNELS_STEP;
    return n;
}
void avp_music_set_sound_channels(unsigned count);
#endif
