/* R160 host-only thermal vision: strictly tests sprite visibility/color and
 * range falloff without touching the original Jaguar gameplay globals. */
#include "jaguar/avp_gpu/thermal_vision.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static AvpAmp actor(int creature)
{
    AvpAmp a;memset(&a,0,sizeof a);a.creature=(u16)creature;
    a.animseq=AS_STAND;return a;
}
int main(void)
{
    AvpAmp marine=actor(AC_HUMAN),alien=actor(AC_ALIEN),egg=actor(AC_EGG);
    AvpAmp pickup=actor(AC_FOOD),flame=actor(AC_FIRE),corpse=actor(AC_DEADMED);
    u32 layer[8]={0},frame[8]={0x123456,0x112233,0x334455,0x445566,
                               0x080808,0x111111,0x222222,0x333333};
    u32 original[8];unsigned near,mid,far;
    assert(!avp_thermal_is_mode(0,PT_PREDATOR,2,0x06c7,0x26f8));
    assert(!avp_thermal_is_mode(1,PT_ALIEN,2,0x06c7,0x26f8));
    assert(!avp_thermal_is_mode(1,PT_PREDATOR,0,0x06c7,0x26f8));
    assert(!avp_thermal_is_mode(1,PT_PREDATOR,1,0x06c7,0x26f8));
    assert(!avp_thermal_is_mode(1,PT_PREDATOR,2,0x0ec3,0x0cf8));
    assert(!avp_thermal_is_mode(1,PT_PREDATOR,2,0x06c7,0x8880));
    assert(!avp_thermal_is_mode(1,PT_PREDATOR,2,0x06c7,0x9558));
    assert(avp_thermal_is_mode(1,PT_PREDATOR,2,0x06c7,0x26f8));
    assert(avp_thermal_is_living(&marine) && avp_thermal_is_living(&alien));
    assert(avp_thermal_is_living(&egg));
    assert(!avp_thermal_is_living(&pickup) && !avp_thermal_is_living(&flame));
    assert(!avp_thermal_is_living(&corpse));
    marine.flags=(u16)(1u<<AMP_DEAD);
    assert(!avp_thermal_is_living(&marine));marine.flags=0;
    marine.animseq=AS_DEATH;assert(!avp_thermal_is_living(&marine));marine.animseq=AS_STAND;
    near=avp_thermal_intensity(&marine,1.0,0xf5f5f5);
    mid=avp_thermal_intensity(&marine,8.0,0xf5f5f5);
    far=avp_thermal_intensity(&marine,32.0,0xf5f5f5);
    assert(near>mid && mid>far && far>0);
    assert(avp_thermal_intensity(&alien,1,0xf5f5f5)<near);
    assert(avp_thermal_intensity(&egg,1,0xf5f5f5)<near);
    assert(avp_thermal_intensity(&pickup,1,0xffffff)==0);
    assert(avp_thermal_intensity(&marine,8,0x101010)<mid);
    assert(avp_thermal_intensity(&marine,1e30,0xffffff)>0);
    assert((avp_thermal_palette(near)>>16)>200u); /* hot: red */
    assert(((avp_thermal_palette(far)>>8)&255u)>10u); /* cooler */
    memcpy(original,frame,sizeof frame);
    layer[0]=avp_thermal_record(near,frame[0]); /* living Marine */
    layer[1]=avp_thermal_record(mid,frame[1]);  /* living Alien */
    layer[2]=0; /* scenery never enters heat mask */
    layer[3]=avp_thermal_record(far,frame[3]);  /* behind FP weapon */
    frame[3]=0x888888;
    layer[4]=avp_thermal_record(near,frame[4]);
    layer[4]=0; /* closer pickup sprite must clear the old hot target */
    avp_thermal_remove_covered(layer,frame,8);
    assert(layer[3]==0 && layer[0]!=0 && layer[1]!=0);
    avp_thermal_composite(layer,frame,8);
    assert(frame[0]==avp_thermal_palette(near));
    assert(frame[1]==avp_thermal_palette(mid));
    assert(frame[2]==original[2]);
    assert(frame[3]==0x888888); /* no glow through foreground weapon */
    assert(frame[4]==original[4]);
    for(unsigned i=5;i<8;i++)assert(frame[i]==original[i]);
    /* VMODE RGB16 samples source 320 words. At 640 host columns the
     * sample for native word 12 is x=25; its output spans x=24..25.
     * A HUD pixel over that source must clear the record first. */
    {
        u32 mask[640]={0},out[640]={0};
        for(unsigned x=0;x<640;x++)out[x]=0x00112233;
        mask[25]=avp_thermal_record(near,0x00112233);
        mask[27]=avp_thermal_record(mid,0x00112233);
        out[27]=0x00abcdef; /* HUD obscures second warm sprite */
        avp_thermal_remove_covered(mask,out,640);
        assert(mask[25] && !mask[27]);
        avp_thermal_composite_rgb16(mask,out,640,1);
        assert(out[24]==avp_thermal_palette(near));
        assert(out[25]==avp_thermal_palette(near));
        assert(out[26]==0x00112233 && out[27]==0x00abcdef);
        assert(out[23]==0x00112233);
    }
    puts("R160 enhanced Predator thermal vision, distance, VMODE and HUD PASS");
    return 0;
}
