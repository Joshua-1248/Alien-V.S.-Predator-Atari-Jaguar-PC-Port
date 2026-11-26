/* Keep the logical allocation limit and host mixer capacity synchronized.
 * Invoked on the game thread, never on the PCM thread. */
#include "pc/pc_audio.h"
#include "jaguar/host/fullsynth_host.h"
#include "sound_channels.h"
void avp_pc_audio_set_sound_channels(unsigned count)
{
    avp_music_set_sound_channels(count);
    avp_jaguar_fullsynth_set_sound_channels(count);
}
