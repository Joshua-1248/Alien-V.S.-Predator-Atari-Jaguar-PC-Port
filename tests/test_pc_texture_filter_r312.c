/* R312: verify specialized opaque Jaguar CRY16 bilinear/AF readout against
 * the R310/R311 public reference path. Include non-power-of-two fallback,
 * source/mipmap, clamp/wrap, and dimensions of the external hi-res pack. */
#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/cry.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *q,size_t len)
{(void)q;(void)len;return (u16)(((unsigned)r<<8u) | ((unsigned)g ^ b));}
static uint32_t rng_state=0x09b8d457u;
static uint32_t random_u32(void)
{
    rng_state^=rng_state<<13u;
    rng_state^=rng_state>>17u;
    rng_state^=rng_state<<5u;
    return rng_state;
}
static double frac(void){return (double)(random_u32()&0xffffu)/65535.0;}
int main(void)
{
    uint32_t *map=(uint32_t*)malloc(65536u*sizeof(uint32_t));
    uint8_t *qconv=(uint8_t*)calloc(131072u,1u);
    unsigned long long comparisons=0;
    assert(map&&qconv);
    for(unsigned j=0;j<65536u;j++)map[j]=((random_u32()&255u)<<16u)|
        ((random_u32()&255u)<<8u)|(random_u32()&255u);
    const unsigned sizes[3][2]={{64u,64u},{31u,47u},{1254u,1254u}};
    for(unsigned sz=0;sz<3;sz++){
        unsigned w=sizes[sz][0],h=sizes[sz][1];
        size_t count=(size_t)w*h;
        uint8_t *rgb=(uint8_t*)malloc(count*3u);
        uint16_t *cry=(uint16_t*)malloc(count*sizeof(uint16_t));
        assert(rgb&&cry);
        for(size_t i=0;i<count;i++){
            rgb[3u*i]=(uint8_t)random_u32();
            rgb[3u*i+1u]=(uint8_t)random_u32();
            rgb[3u*i+2u]=(uint8_t)random_u32();
            cry[i]=(uint16_t)random_u32();
        }
        AvpGpuRgbImage src={0};src.width=(int)w;src.height=(int)h;
        src.rgb=rgb;src.cry16=cry;
        /* Verify the new worker-only lookup never lazily allocates and
         * sees exactly the mip chain prepared by the render owner. */
        AvpFilterCache *cache=avp_filter_cache_create();assert(cache);
        AvpGpuRgbImage originals[74]={0},notices[18]={0};
        originals[0]=src;
        avp_filter_cache_prepare(cache,NULL,NULL,originals,notices,0,qconv,131072u);
        assert(!avp_filter_cache_lookup_ready(cache,&originals[0]));
        const AvpFilterImage *primed=avp_filter_cache_lookup(cache,&originals[0]);
        assert(primed && primed->levels>1u);
        assert(avp_filter_cache_lookup_ready(cache,&originals[0])==primed);
        avp_filter_cache_destroy(cache);
        AvpFilterImage mip={0};
        assert(avp_filter_image_build(&mip,&src,qconv,131072u));
        const int shades[]={-512,-255,-65,-1,0,1,23,255,512};
        for(unsigned shade_i=0;shade_i<sizeof(shades)/sizeof(shades[0]);shade_i++){
            const int shade=shades[shade_i];
            uint8_t lut[256];avp_filter_make_shade_lut(lut,shade);
            for(unsigned test=0;test<6000u;test++){
                AvpFilterPlan plan;
                plan.level=random_u32()%mip.levels;
                plan.bilinear=1u;
                plan.taps=(unsigned[]){1u,2u,4u,8u,16u}[random_u32()%5u];
                plan.du=(frac()-0.5)*0.3;
                plan.dv=(frac()-0.5)*0.3;
                /* Coordinates outside [0,1] exercise both edge clamp and
                 * wrapped negative/positive UVs, including mip level 0. */
                const double u=(frac()-0.5)*5.0;
                const double v=(frac()-0.5)*5.0;
                const int wrap=(int)(random_u32()&1u);
                unsigned ref_alpha=12,fast_alpha=31;
                const uint32_t original=avp_filter_sample_prepared(&src,&mip,
                    u,v,plan,wrap,shade,map,&ref_alpha);
                const uint32_t accelerated=avp_filter_sample_prepared_shaded(&src,&mip,
                    u,v,plan,wrap,shade,map,&fast_alpha,lut);
                if(original!=accelerated||ref_alpha!=fast_alpha){
                    fprintf(stderr,"R312 mismatch: size=%ux%u mip=%u taps=%u wrap=%d shade=%d\n",
                            w,h,plan.level,plan.taps,wrap,shade);
                    return 1;
                }
                comparisons++;
            }
        }
        avp_filter_image_destroy(&mip);
        free(rgb);free(cry);
    }
    free(map);free(qconv);
    printf("R312: %llu opaque CRY16 exact sample/alpha comparisons PASS\n",comparisons);
    return 0;
}
