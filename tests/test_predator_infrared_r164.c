/* RE39 R164: screenshot-identified Infrared, independent enhanced palette.
 * No cartridge data is used: original filter pair comes from RDB.DTA. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static AvpAmp actor(unsigned creature)
{
    AvpAmp a;memset(&a,0,sizeof a);a.creature=(u16)creature;
    a.animseq=AS_STAND;return a;
}
static unsigned red(u32 v){return (v>>16)&255u;}
static unsigned green(u32 v){return (v>>8)&255u;}
static unsigned blue(u32 v){return v&255u;}
int main(void)
{
    AvpAmp human=actor(AC_HUMAN), pickup=actor(AC_FOOD);
    unsigned near=avp_thermal_heat_intensity(&human,1.0,0xf5f5f5);
    unsigned far=avp_thermal_heat_intensity(&human,14.0,0xf5f5f5);
    u32 hot=avp_infrared_palette(near),cool=avp_infrared_palette(far);
    u32 records[640]={0},scene[640];
    assert(near>far && near>220u && far<90u);
    assert(red(hot)>230u && red(hot)>blue(hot));
    assert(blue(cool)>red(cool) && blue(cool)>green(cool));
    assert(avp_infrared_palette(0)==0x0006063au);
    assert(avp_infrared_palette(255)==0x00fff080u);
    assert(avp_infrared_palette(near)!=avp_thermal_heat_palette(near));
    assert(avp_thermal_heat_palette(255)==0x00ff251au);
    assert(avp_thermal_palette(255)==0x00ffecccu);
    assert(avp_infrared_is_mode(1,PT_PREDATOR,2,0x06c7,0x8880));
    assert(!avp_infrared_is_mode(0,PT_PREDATOR,2,0x06c7,0x8880));
    assert(!avp_infrared_is_mode(1,PT_PREDATOR,1,0x06c7,0x8880));
    assert(!avp_infrared_is_mode(1,PT_PREDATOR,0,0x06c7,0x8880));
    assert(!avp_infrared_is_mode(1,PT_ALIEN,2,0x06c7,0x8880));
    assert(!avp_infrared_is_mode(1,PT_PREDATOR,2,0x0ec3,0x8880));
    assert(!avp_infrared_is_mode(1,PT_PREDATOR,2,0x06c7,0x26f8));
    assert(!avp_infrared_is_mode(1,PT_PREDATOR,2,0x06c7,0x9558));
    assert(avp_predator_vision_style_ex(0,0,0,PT_PREDATOR,2,0x06c7,0x8880)==0);
    assert(avp_predator_vision_style_ex(0,0,1,PT_PREDATOR,2,0x06c7,0x8880)
           ==AVP_VISION_ENHANCED_INFRARED);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x06c7,0x8880)
           ==AVP_VISION_ENHANCED_INFRARED);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x06c7,0x26f8)
           ==AVP_VISION_ENHANCED_THERMAL);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x06c7,0x9558)
           ==AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x0ec3,0x8880)==0);
    assert(avp_predator_vision_style_ex(1,1,1,PT_PREDATOR,2,0x0ec3,0x0cf8)==0);
    assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,0x06c7,0x8880)==0);
    assert(avp_thermal_heat_intensity(&pickup,1.0,0xffffff)==0u);

    /* Exact native RGB16 word grouping and occluded foreground invariants. */
    for(unsigned i=0;i<640;i++)scene[i]=0x00112233u;
    records[25]=avp_thermal_record(near,scene[25]);
    records[27]=avp_thermal_record(near,scene[27]);
    scene[27]=0x00123456u; /* foreground weapon/closed door */
    avp_thermal_remove_covered(records,scene,640);
    avp_thermal_composite_vmode(records,scene,640,1,6,AVP_VISION_ENHANCED_INFRARED);
    assert(scene[24]==hot && scene[25]==hot);
    assert(scene[26]==0x00112233u && scene[27]==0x00123456u);
    assert(scene[23]==0x00112233u && scene[28]==0x00112233u);
    puts("R164 independent Infrared palette, register binding, cloak, HUD/occlusion and other spectra PASS");
    return 0;
}
