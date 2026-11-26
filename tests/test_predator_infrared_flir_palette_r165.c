/* RE39 R165: supplied FLIR infrared reference: cooler clothing, warm skin.
 * Regression exercises the actual host Infrared palette; only this LUT changes.
 * This is an artistic false-color approximation, not radiometric measurement. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned r(unsigned c){return (c>>16)&255u;}
static unsigned g(unsigned c){return (c>>8)&255u;}
static unsigned b(unsigned c){return c&255u;}
static AvpAmp human(void)
{
    AvpAmp a;memset(&a,0,sizeof(a));
    a.creature=AC_HUMAN;a.animseq=AS_STAND;return a;
}
int main(void)
{
    AvpAmp a=human();
    const unsigned cool_texel=avp_thermal_heat_intensity(&a,2.0,0x333333u);
    const unsigned warm_texel=avp_thermal_heat_intensity(&a,2.0,0xf0f0f0u);
    const unsigned distant_texel=avp_thermal_heat_intensity(&a,14.0,0xf0f0f0u);
    const unsigned cool=avp_infrared_palette(cool_texel);
    const unsigned warm=avp_infrared_palette(warm_texel);
    const unsigned far=avp_infrared_palette(distant_texel);
    assert(cool_texel<warm_texel && distant_texel<warm_texel);
    /* The new reference's cooler fabric is magenta/purple, NOT R164's red. */
    assert(avp_infrared_palette(147)==0x00c61a8cu);
    assert(avp_infrared_palette(171)==0x00dd2d77u);
    assert(b(cool)>105u && g(cool)<75u && r(cool)>b(cool));
    /* Bright face/hand texels are golden orange/yellow rather than flat red. */
    assert(avp_infrared_palette(219)==0x00faa538u);
    assert(r(warm)>235u && g(warm)>170u && b(warm)<95u);
    /* Preserve the original dark blue/cold and R164 yellow/hot endpoints. */
    assert(avp_infrared_palette(0)==0x0006063au);
    assert(avp_infrared_palette(255)==0x00fff080u);
    assert(b(far)>r(far) && b(far)>g(far));
    /* Other independently approved modes/selectors are not changed. */
    assert(avp_thermal_heat_palette(255)==0x00ff251au);
    assert(avp_thermal_palette(255)==0x00ffecccu);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x06c7,0x8880)
           ==AVP_VISION_ENHANCED_INFRARED);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x06c7,0x26f8)
           ==AVP_VISION_ENHANCED_THERMAL);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x06c7,0x9558)
           ==AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x0ec3,0x8880)
           ==AVP_VISION_ORIGINAL);
    puts("R171 Infrared sweet-spot palette + unchanged other spectra PASS");
    return 0;
}
