/* RE39 R175: legacy R174 brightness-only Infrared background LUT remains
 * stable for compatibility. The new in-game depth-based Infrared compositor
 * is independently tested in test_predator_infrared_background_depth_r175.c.
 * Do not mistake this legacy color-only LUT for runtime distance semantics. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>

static unsigned red(u32 x){return (x>>16)&255u;}
static unsigned green(u32 x){return (x>>8)&255u;}
static unsigned blue(u32 x){return x&255u;}

int main(void)
{
    const unsigned ir=AVP_VISION_ENHANCED_INFRARED;
    const unsigned em=AVP_VISION_ENHANCED_ELECTROMAGNETIC;
    const unsigned th=AVP_VISION_ENHANCED_THERMAL;
    static const struct {unsigned luma;u32 color;} landmarks[]={
        {0u,0x00060122u}, {30u,0x0011023du}, {60u,0x00280463u},
        {85u,0x00430585u}, {110u,0x0067089du},
        {120u,0x007709a4u}, {132u,0x008d0baau},
        {155u,0x00b913abu}, {180u,0x00e0239eu},
        {210u,0x00f64194u}, {255u,0x00ff7896u}
    };
    unsigned previous_red=0u;
    for(unsigned v=0u;v<=255u;v++){
        const u32 src=(v<<16)|(v<<8)|v;
        const u32 c=avp_predator_background_color(ir,src,10u,20u);
        assert(c==avp_predator_background_color(ir,src,200u,150u));
        assert(c==avp_predator_background_color(ir,src,0u,0u));
        assert(red(c)>=previous_red);
        assert(green(c)<=122u);
        previous_red=red(c);
    }
    for(unsigned i=0u;i<sizeof(landmarks)/sizeof(landmarks[0]);i++){
        const u32 lum=landmarks[i].luma;
        const u32 src=(lum<<16)|(lum<<8)|lum;
        assert(avp_predator_background_color(ir,src,0u,0u)==landmarks[i].color);
    }
    { /* Legacy brightness-only fallback: dark stays violet, while bright
       scenery can still reach pink. R175's live pass uses depth instead. */
        const u32 shadow=avp_predator_background_color(ir,0x003c3c3cu,1u,1u);
        const u32 middle=avp_predator_background_color(ir,0x00848484u,1u,1u);
        const u32 bright=avp_predator_background_color(ir,0x00bbbbbbU,1u,1u);
        assert(blue(shadow)>red(shadow));
        assert(blue(middle)>red(middle) && red(middle)>135u);
        assert(red(bright)>=220u && blue(bright)>125u);
    }
    assert(avp_predator_background_color(ir,0x00c7bd80u,42u,66u)==0x00e8299au);
    assert(avp_predator_background_style(1,PT_PREDATOR,2u,0x06c7u,0x8880u)==(int)ir);
    assert(avp_predator_background_style(1,PT_PREDATOR,2u,0x06c7u,0x9558u)==(int)em);
    assert(avp_predator_background_style(1,PT_PREDATOR,2u,0x06c7u,0x26f8u)==AVP_VISION_ORIGINAL);
    assert(avp_predator_vision_style_ex(1,0,0,PT_PREDATOR,2u,0x06c7u,0x26f8u)==(int)th);
    /* Original actor heat and the distinct EM/TH world look are unchanged. */
    assert(avp_infrared_palette(255u)==0x00fff080u);
    assert(avp_infrared_palette(171u)==0x00dd2d77u);
    assert(avp_thermal_heat_palette(255u)==0x00ff251au);
    assert(avp_thermal_palette(255u)==0x00ffecccu);
    assert(avp_predator_background_color(em,0x00c7bd80u,42u,66u)==0x00b30ef8u);
    assert(avp_predator_background_color(th,0x00c7bd80u,42u,66u)==0x001254dcu);
    puts("R175 original R174 brightness LUT retained; real infrared depth path separately tested PASS");
    return 0;
}
