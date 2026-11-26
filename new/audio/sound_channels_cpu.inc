/* PC-only logical voice storage. The original reference remains eight voices. */
#ifdef AVP_PC_HOST
#include "../../new/audio/sound_channels.h"
static unsigned pc_sound_channels=AVP_SOUND_CHANNELS_DEFAULT;
#define AVP_SOUND_VOICES pc_sound_channels
#define AVP_SOUND_VOICE_CAPACITY AVP_SOUND_CHANNELS_MAX
#else
#define AVP_SOUND_VOICES 8u
#define AVP_SOUND_VOICE_CAPACITY AVP_SOUND_VOICES
#endif
