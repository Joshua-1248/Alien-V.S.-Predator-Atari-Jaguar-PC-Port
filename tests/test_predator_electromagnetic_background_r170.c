/* RE39 R170: scenery-only Electromagnetic palette photo-reference calibration.
 * No positional pattern; approved Thermal, Infrared and actor palettes stable. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>

static unsigned red(u32 x){return (x>>16)&255u;}
static unsigned green(u32 x){return (x>>8)&255u;}
static unsigned blue(u32 x){return x&255u;}
int main(void)
{
    const unsigned em=AVP_VISION_ENHANCED_ELECTROMAGNETIC;
    const unsigned thermal=AVP_VISION_ENHANCED_THERMAL;
    const unsigned infrared=AVP_VISION_ENHANCED_INFRARED;
    unsigned last_blue=0;
    for(unsigned lum=0;lum<=255u;lum++){
        const u32 input=(lum<<16)|(lum<<8)|lum;
        const u32 c=avp_predator_background_color(em,input,11u,23u);
        /* Identical colors never depend on position/tile/pixel coordinates. */
        assert(c==avp_predator_background_color(em,input,290u,210u));
        assert(c==avp_predator_background_color(em,input,0u,0u));
        assert(green(c)<=57u);
        assert(blue(c)>=red(c));
        assert(blue(c)>=last_blue);
        last_blue=blue(c);
    }
    { /* Reference-oriented contrast landmarks. */
      const u32 dark=avp_predator_background_color(em,0x00141414u,0,0);
      const u32 middle=avp_predator_background_color(em,0x00888888u,0,0);
      const u32 light=avp_predator_background_color(em,0x00dededeu,0,0);
      assert(red(dark)<=5u && blue(dark)<=35u);
      assert(red(middle)>=45u && blue(middle)>=160u);
      assert(red(light)>=195u && blue(light)>=245u);
      assert(red(middle)>red(dark) && red(light)>red(middle));
      assert(green(light)<red(light)/3u);
    }
    /* The background toggle now excludes Thermal but not either remaining
     * enhanced background. This must not disable Thermal actor heat. */
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x26f8)==AVP_VISION_ORIGINAL);
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x9558)==(int)em);
    assert(avp_predator_background_style(1,PT_PREDATOR,2,0x06c7,0x8880)==(int)infrared);
    assert(avp_predator_vision_style_ex(1,0,0,PT_PREDATOR,2,0x06c7,0x26f8)==(int)thermal);
    /* Absolute values for the other modes and actors are not to be touched. */
    assert(avp_predator_background_color(thermal,0x00c7bd80u,42,66)==0x001254dcu);
    assert(avp_predator_background_color(infrared,0x00c7bd80u,42,66)==0x00e8299au);
    assert(avp_thermal_heat_palette(255u)==0x00ff251au);
    assert(avp_thermal_palette(255u)==0x00ffecccu);
    assert(avp_infrared_palette(255u)==0x00fff080u);
    puts("R174 Electromagnetic stable + Infrared purple-to-pink depth cue PASS");
    return 0;
}
