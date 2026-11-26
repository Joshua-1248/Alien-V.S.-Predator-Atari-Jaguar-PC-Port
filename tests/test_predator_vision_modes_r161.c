/* RE39 R162: corrected real screenshot-identified Thermal/Electromagnetic
 * selection. Retain both independent host enhancements and original game mode. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static AvpAmp actor(int creature){AvpAmp a;memset(&a,0,sizeof a);
    a.creature=(u16)creature;a.animseq=AS_STAND;return a;}
int main(void)
{
    AvpAmp human=actor(AC_HUMAN),alien=actor(AC_ALIEN),corpse=actor(AC_DEADMED);
    unsigned near,mid,far;
    u32 *mask,*scene;
    const unsigned w=640;
    /* Five distinct pairs from original RDB.DTA, validated by the submitted
     * green, pink/orange, blue and pink/yellow vision screenshots. */
    static const struct {u16 vmode,extra;int thermal,em;} cases[5]={
        {0x06c7,0x9558,0,AVP_VISION_ENHANCED_ELECTROMAGNETIC},
        {0x0ec3,0x0cf8,0,0},
        {0x0ec3,0x8880,0,0}, /* GREEN Night Vision: no enhancements */
        {0x06c7,0x26f8,AVP_VISION_ENHANCED_THERMAL,0},
        {0x06c7,0x8880,0,0}  /* pink/yellow Infrared: unchanged */
    };
    for(unsigned i=0;i<5;i++){
        const u16 vmode=cases[i].vmode,xfade=cases[i].extra;
        assert(avp_predator_vision_style(0,0,PT_PREDATOR,2,vmode,xfade)==0);
        assert(avp_predator_vision_style(1,0,PT_PREDATOR,2,vmode,xfade)==cases[i].thermal);
        assert(avp_predator_vision_style(0,1,PT_PREDATOR,2,vmode,xfade)==cases[i].em);
        assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,vmode,xfade)==(cases[i].thermal?cases[i].thermal:cases[i].em));
        assert(avp_predator_vision_style(1,1,PT_PREDATOR,1,vmode,xfade)==0);
        assert(avp_predator_vision_style(1,1,PT_PREDATOR,0,vmode,xfade)==0);
        assert(avp_predator_vision_style(1,1,PT_ALIEN,2,vmode,xfade)==0);
    }
    /* VMODE or X_FADE alone is insufficient to identify a filter. */
    assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,0x0ec3,0x9558)==0);
    assert(avp_predator_vision_style(1,1,PT_PREDATOR,2,0x0ec3,0x26f8)==0);
    near=avp_thermal_heat_intensity(&human,1.0,0xfefefe);
    mid=avp_thermal_heat_intensity(&human,10.0,0xfefefe);
    far=avp_thermal_heat_intensity(&human,40.0,0xfefefe);
    assert(near>mid && mid>far && far>0u);
    assert(avp_thermal_heat_intensity(&alien,1.0,0xfefefe)<near);
    assert(avp_thermal_heat_intensity(&corpse,1.0,0xffffff)==0u);
    assert(avp_thermal_heat_palette(near)!=avp_thermal_palette(near));
    mask=(u32*)calloc(w,sizeof(u32));scene=(u32*)calloc(w,sizeof(u32));
    assert(mask&&scene);
    for(unsigned i=0;i<w;i++)scene[i]=0x00112233u;
    /* VMODE RGB16: 320 source words -> 320 output spans, x24..25. */
    mask[25]=avp_thermal_record(near,scene[25]);
    avp_thermal_composite_vmode(mask,scene,w,1,6,AVP_VISION_ENHANCED_THERMAL);
    assert(scene[24]==avp_thermal_heat_palette(near));
    assert(scene[25]==avp_thermal_heat_palette(near));
    assert(scene[23]==0x00112233u && scene[26]==0x00112233u);
    for(unsigned i=0;i<w;i++){scene[i]=0x00112233u;mask[i]=0;}
    mask[25]=avp_thermal_record(near,scene[25]);
    avp_thermal_composite_vmode(mask,scene,w,1,6,AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    assert(scene[24]==avp_thermal_palette(near));
    assert(scene[25]==avp_thermal_palette(near));
    assert(scene[23]==0x00112233u && scene[26]==0x00112233u);
    /* The corrected Electromagnetic view is RGB16 too, with the old cool
     * false-color palette. The rest of this test also keeps the independent
     * generic RGB24 pairing/HUD protection coverage. */
    for(unsigned i=0;i<w;i++){scene[i]=0x00112233u;mask[i]=0;}
    mask[25]=avp_thermal_record(near,scene[25]);
    avp_thermal_composite_vmode(mask,scene,w,1,2,AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    assert(scene[24]==0x00112233u && scene[25]==0x00112233u);
    mask[26]=avp_thermal_record(near,scene[26]);
    /* Word group 6 reads 25 and 27 for width=640. */
    mask[27]=avp_thermal_record(mid,scene[27]);
    avp_thermal_composite_vmode(mask,scene,w,1,2,AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    {
        unsigned intensity=(near+mid+1)/2;
        u32 expect=avp_thermal_palette(intensity);
        for(unsigned i=24;i<28;i++)assert(scene[i]==expect);
        assert(scene[23]==0x00112233u && scene[28]==0x00112233u);
    }
    for(unsigned i=0;i<w;i++){scene[i]=0x00112233u;mask[i]=0;}
    mask[25]=avp_thermal_record(near,scene[25]);
    mask[27]=avp_thermal_record(mid,scene[27]);
    scene[27]=0x00abcdefu; /* weapon/HUD cover must remove heat record */
    avp_thermal_remove_covered(mask,scene,w);
    avp_thermal_composite_vmode(mask,scene,w,1,2,AVP_VISION_ENHANCED_ELECTROMAGNETIC);
    assert(scene[24]==0x00112233u && scene[27]==0x00abcdefu);
    free(mask);free(scene);
    puts("RE39 R162 screenshot-verified blue Thermal / Electromagnetic RGB16 assignments, all 5 filters, RGB24 fallback and HUD masks PASS");
    return 0;
}
