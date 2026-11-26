#include "jaguar/avp_gpu/texture_filter.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crgb[65536];
static uint32_t ink_apply(void *unused,uint32_t bg,uint16_t ink)
{
    (void)unused;
    unsigned g=(bg>>8)&255u;
    g+=ink;
    if(g>255u)g=255u;
    return (bg&0xffff00ffu)|(g<<8);
}
int main(void)
{
    AvpFilterDerivatives dd={0};
    unsigned alpha=0;
    unsigned char pixels[12]={255,0,0, 0,255,0, 0,0,255, 255,255,255};
    AvpGpuRgbImage tex={0};
    tex.width=2;tex.height=2;tex.rgb=pixels;
    uint32_t at=avp_filter_sample(&tex,NULL,0.25,0.25,dd,AVP_TEXFILTER_NEAREST,0,0,0,crgb,&alpha);
    assert(at==0x00ff0000u&&alpha==255u);
    at=avp_filter_sample(&tex,NULL,0.5,0.5,dd,AVP_TEXFILTER_BILINEAR,0,0,0,crgb,&alpha);
    assert(at==0x00808080u&&alpha==255u);
    /* No red bleed from the fully transparent green neighbour. */
    unsigned char coverage[4]={255,0,255,0};
    unsigned char a2[6]={255,0,0, 0,255,0};
    tex.width=2;tex.height=1;tex.rgb=a2;tex.alpha=coverage;
    at=avp_filter_sample(&tex,NULL,0.5,0.5,dd,AVP_TEXFILTER_BILINEAR,0,0,0,crgb,&alpha);
    assert(at==0x00ff0000u && alpha==128u);
    /* True 2x downsampling builds a mip, not a framebuffer blur. */
    unsigned char checker[8u*8u*3u];
    for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){
        unsigned c=((x+y)&1u)?255u:0u;
        unsigned o=(y*8u+x)*3u;
        for(unsigned k=0;k<3u;k++)checker[o+k]=(unsigned char)c;
    }
    tex=(AvpGpuRgbImage){0};tex.width=8;tex.height=8;tex.rgb=checker;
    AvpFilterImage mip={0};
    assert(avp_filter_image_build(&mip,&tex,NULL,0));
    assert(mip.levels==4u&&mip.mip[3].width==1u&&mip.mip[3].height==1u);
    dd=(AvpFilterDerivatives){1.0,0,0,1.0};
    at=avp_filter_sample(&tex,&mip,0.1,0.1,dd,AVP_TEXFILTER_NEAREST_MIP,0,1,0,crgb,&alpha);
    assert(((at>>16)&255u)>=120u&&((at>>16)&255u)<=135u);
    avp_filter_image_destroy(&mip);
    /* Long horizontal footprint, short vertical footprint: anisotropic taps
     * integrate the streak without needlessly blurring vertical detail. */
    unsigned char striped[32u*16u*3u];
    for(unsigned y=0;y<16;y++)for(unsigned x=0;x<32;x++){
        unsigned c=(x&1u)?255u:0u;
        unsigned o=(y*32u+x)*3u;
        for(unsigned k=0;k<3u;k++)striped[o+k]=(unsigned char)c;
    }
    tex.width=32;tex.height=16;tex.rgb=striped;tex.alpha=NULL;
    dd=(AvpFilterDerivatives){0.5,0.0,0.0,1.0/16.0};
    uint32_t aliased=avp_filter_sample(&tex,NULL,17.5/32.0,0.5,dd,
        AVP_TEXFILTER_NEAREST,0,1,0,crgb,&alpha);
    uint32_t filtered=avp_filter_sample(&tex,NULL,17.5/32.0,0.5,dd,
        AVP_TEXFILTER_NEAREST,16,1,0,crgb,&alpha);
    assert(((aliased>>16)&255u)==255u);
    assert(((filtered>>16)&255u)>=110u&&((filtered>>16)&255u)<=145u);
    /* Clones borrow their owner's actual RGB storage, not struct address. */
    AvpFilterCache *cache=avp_filter_cache_create();
    AvpGpuRgbImage original[74]={0},notice[18]={0};
    original[0]=tex;
    avp_filter_cache_prepare(cache,NULL,NULL,original,notice,0,NULL,0);
    AvpGpuRgbImage clones[74];memcpy(clones,original,sizeof(clones));
    const AvpFilterImage *shared=avp_filter_cache_lookup(cache,&clones[0]);
    assert(shared&&shared->levels>1);
    avp_filter_cache_prepare(cache,NULL,NULL,clones,notice,0,NULL,0);
    assert(avp_filter_cache_lookup(cache,&original[0])==shared);
    /* A changed texture invalidates only the changed slot on prepare;
     * other 91 source textures must not be rebuilt eagerly. */
    unsigned char replacement[4u*4u*3u]={0};
    original[1].width=4;original[1].height=4;original[1].rgb=replacement;
    avp_filter_cache_prepare(cache,NULL,NULL,original,notice,0,NULL,0);
    assert(avp_filter_cache_lookup(cache,&original[0])==shared);
    const AvpFilterImage *second=avp_filter_cache_lookup(cache,&original[1]);
    assert(second&&second->levels==3u);
    avp_filter_cache_destroy(cache);
    /* Native jagged weapon silhouette gains fractional coverage when enabled. */
    const size_t count=320u*228u;
    uint32_t *before=(uint32_t*)calloc(count,4u),
             *after=(uint32_t*)calloc(count,4u),
             *host=(uint32_t*)calloc(640u*456u,4u);
    assert(before&&after&&host);
    after[100u*320u+100u]=0x00ffffffu;
    avp_filter_native_overlay(host,640,456,before,after);
    assert(host[200u*640u+200u]!=0u);
    memset(host,0,640u*456u*4u);
    AvpHudRmwOps *ops=(AvpHudRmwOps*)calloc(count,sizeof(AvpHudRmwOps));
    assert(ops);
    ops[100u*320u+100u].ink[0]=16u;
    ops[100u*320u+100u].count=1;
    after[100u*320u+100u]=0x00001000u;
    host[200u*640u+200u]=0x00335577u;
    avp_filter_hud_replay(host,640,456,before,after,ops,ink_apply,NULL,NULL,0);
    assert((host[200u*640u+200u]&0x00ff00ffu)==0x00330077u);
    assert(((host[200u*640u+200u]>>8)&255u)>0x55u);
    free(ops);free(before);free(after);free(host);
    puts("R308 filtering: nearest, bilinear alpha, real mips, anisotropic taps, clone cache, weapon/HUD compositing PASS");
    return 0;
}
