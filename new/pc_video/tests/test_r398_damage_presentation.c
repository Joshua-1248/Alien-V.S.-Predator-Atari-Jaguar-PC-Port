#include "../damage_presentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static u8 q[131072];
    memset(q,0x88,sizeof q);
    const u32 pixel=0x00908070u;
    const u16 colors[]={0xf0bcu,0x40bcu,0xcff8u};
    for(unsigned i=0;i<3u;i++){
        AvpDamagePresentationSample a=avp_damage_sample(0x8880,0x8880,(s16)(i*4),1,1);
        AvpDamagePresentationSample z=avp_damage_sample(colors[i],0x8880,(s16)(i*4),1,1);
        u32 full=avp_jaguar_rmw_xrgb8888(pixel,avp_jaguar_game_clut(z.color,z.extra,0),q,sizeof q);
        AvpDamagePresentationBlend b=avp_damage_blend(a,z,0,1,0.0);
        assert(avp_damage_blend_pixel(pixel,b,q,sizeof q)==pixel);
        b=avp_damage_blend(a,z,0,1,1.0);
        assert(avp_damage_blend_pixel(pixel,b,q,sizeof q)==full);
        b=avp_damage_blend(a,z,0,1,0.5);
        u32 mid=avp_damage_blend_pixel(pixel,b,q,sizeof q);
        assert(mid!=full && mid!=pixel);
        assert(avp_damage_blend_pixel(pixel,avp_damage_blend(z,a,0,1,0.5),q,sizeof q)==mid);
        for(unsigned step=0;step<=256u;step++){
            b=avp_damage_blend(z,a,0,1,(double)step/256.0);
            u32 out=avp_damage_blend_pixel(pixel,b,q,sizeof q);
            for(unsigned shift=0;shift<24;shift+=8){
                unsigned expected=((((full>>shift)&255u)*(256u-step)+((pixel>>shift)&255u)*step+128u)>>8);
                assert(((out>>shift)&255u)==expected);
            }
        }
        assert(avp_damage_blend(a,z,0,0,0.2).weight==256u);
        assert(avp_damage_blend(a,z,4,1,0.2).weight==256u);
        a.valid=0;assert(avp_damage_blend(a,z,0,1,0.2).weight==256u);
        a.valid=1;a.level=2;assert(avp_damage_blend(a,z,0,1,0.2).weight==256u);
        a.level=1;a.species=-1;assert(avp_damage_blend(a,z,0,1,0.2).weight==256u);
        a.species=z.species;a.extra=0x8881;assert(avp_damage_blend(a,z,0,1,0.2).weight==256u);
        a.extra=z.extra;z.valid=0;assert(avp_damage_blend(a,z,0,1,0.2).weight==256u);
    }
    puts("R398 damage interpolation: three species, exact endpoints, recovery, bypass and isolated snapshots PASS");
    return 0;
}
