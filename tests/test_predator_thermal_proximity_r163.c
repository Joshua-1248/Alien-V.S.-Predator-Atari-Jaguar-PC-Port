/* RE39 R163: the blue Thermal color scale must get redder when a living
 * actor approaches, never as it retreats. This catches R162's palette inversion
 * without changing the separately validated Electromagnetic color scale. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AvpAmp creature(unsigned what)
{
    AvpAmp a;memset(&a,0,sizeof a);
    a.creature=(u16)what;a.animseq=AS_STAND;
    return a;
}
static unsigned red(u32 c){return (c>>16)&255u;}
static unsigned green(u32 c){return (c>>8)&255u;}
static unsigned blue(u32 c){return c&255u;}
int main(void)
{
    AvpAmp human=creature(AC_HUMAN), alien=creature(AC_ALIEN);
    AvpAmp pickup=creature(AC_FOOD);
    /* Keep fixed texel color: ONLY distance changes. Far pixels are blue or
     * cyan, while close bright pixels must reach red, not yellow/white. */
    const u32 bright=0x00f5f5f5;
    unsigned near=avp_thermal_heat_intensity(&human,1.0,bright);
    unsigned mid=avp_thermal_heat_intensity(&human,5.0,bright);
    unsigned far=avp_thermal_heat_intensity(&human,10.0,bright);
    const u32 near_rgb=avp_thermal_heat_palette(near);
    const u32 mid_rgb=avp_thermal_heat_palette(mid);
    const u32 far_rgb=avp_thermal_heat_palette(far);
    assert(near>mid && mid>far);
    assert(near>225u && mid<200u && far<90u);
    assert(red(near_rgb)>210u && red(near_rgb)>green(near_rgb)*3u);
    assert(red(far_rgb)<red(near_rgb)/2u);
    assert(blue(far_rgb)>red(far_rgb));
    assert(green(mid_rgb)>green(near_rgb));
    /* Every local texture brightness has non-increasing apparent intensity
     * with distance, including deep shadow and both living species. */
    for(unsigned texel=0;texel<256u;texel+=17u){
        u32 color=(texel<<16)|(texel<<8)|texel;
        for(unsigned species=0;species<2u;species++){
            const AvpAmp *a=species?&alien:&human;
            unsigned prev=256u;
            for(unsigned d=1;d<=24;d++){
                unsigned i=avp_thermal_heat_intensity(a,(double)d,color);
                assert(i<=prev && i>0u);
                prev=i;
            }
        }
    }
    /* Cool shadow vs hot texel on the same actor, same distance. */
    assert(avp_thermal_heat_intensity(&human,1.0,0x101010)<near);
    assert(avp_thermal_heat_intensity(&human,1.0,0x101010) >
           avp_thermal_heat_intensity(&human,10.0,0x101010));
    assert(avp_thermal_heat_intensity(&pickup,1.0,bright)==0);
    assert(avp_thermal_heat_intensity(&human,1.0,bright)==
           avp_thermal_heat_intensity(&human,3.5,bright));
    /* R162 mode mapping and the existing Electromagnetic palette untouched. */
    assert(avp_thermal_is_mode(1,PT_PREDATOR,2,0x06c7,0x26f8));
    assert(avp_electromagnetic_is_mode(1,PT_PREDATOR,2,0x06c7,0x9558));
    assert(avp_thermal_palette(0)==0x000a0f26);
    assert(avp_thermal_palette(255)==0x00ffeccc);
    puts("R163 blue Thermal proximity/false-color/EM isolation PASS");
    return 0;
}
