/* PC-only per-viewport audio analyzer attenuation; actual PCM comes from
 * FullSynth's existing real synthesized voices, not invented SFX data. */
#include "jaguar/host/mp_waveform_proximity.h"
#include "jaguar/avp_gpu/pred_scope_raster.h"
#include "jaguar/avp_gpu/hud_weapon_icons.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL waveform %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    CHECK(avp_mp_waveform_gain(0u,0u,15,15,0,0,0,0)==256u);
    CHECK(avp_mp_waveform_gain(1u,1u,15,15,0,0,50<<16,50<<16)==256u);
    CHECK(avp_mp_waveform_gain(0u,1u,15,15,0,0,0,0)==256u);
    CHECK(avp_mp_waveform_gain(0u,1u,15,15,0,0,6<<16,0)==128u);
    CHECK(avp_mp_waveform_gain(0u,1u,15,15,0,0,12<<16,0)==0u);
    CHECK(avp_mp_waveform_gain(0u,1u,15,14,0,0,0,0)==0u);
    CHECK(avp_mp_waveform_gain(3u,2u,1,2,0,0,0,0)==0u);
    CHECK(avp_mp_waveform_gain(4u,1u,15,15,0,0,0,0)==256u);
    CHECK(avp_mp_waveform_gain(15u,1u,15,15,0,0,0,0)==256u);
    CHECK(avp_mp_waveform_gain(16u,1u,15,15,0,0,0,0)==0u);
    CHECK(avp_mp_waveform_gain(0u,4u,15,15,0,0,0,0)==0u);
    /* Predator has the original visible analyzer, Marine and Alien do not.
     * All species can still contribute proximity-filtered AUDIO samples. */
    CHECK(avp_mp_scope_visible(2u));
    CHECK(!avp_mp_scope_visible(0u));
    CHECK(!avp_mp_scope_visible(1u));
    CHECK(avp_mp_scope_origin_x(2u)==AVP_PRED_SCOPE_X);
    puts("PASS waveform: self, nearby, far, different levels, HUD isolation");
    return 0;
}
