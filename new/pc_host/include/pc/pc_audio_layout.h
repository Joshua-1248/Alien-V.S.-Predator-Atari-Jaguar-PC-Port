#ifndef AVP_PC_AUDIO_LAYOUT_H
#define AVP_PC_AUDIO_LAYOUT_H
#include <stdint.h>
/* PC-host stereo PCM matrix routing; not a proprietary immersive codec. */
#define AVP_AUDIO_LAYOUT_COUNT 13u
#define AVP_AUDIO_MAX_CHANNELS 24u
unsigned avp_audio_layout_bed(unsigned mode);
unsigned avp_audio_layout_channels(unsigned mode,unsigned subs);
const char *avp_audio_layout_name(unsigned mode);
typedef struct AvpAudioBassState { int32_t left,right; } AvpAudioBassState;
/* Actual output channel count must match the opened device. The converter
 * never writes past frames*channels and is deterministic across blocks. */
void avp_audio_layout_convert(const int16_t *in,int16_t *out,unsigned frames,
                              unsigned mode,unsigned subs,AvpAudioBassState *bass);
#endif
