/* R314: same Jaguar source texels, tap positions, CRY shade, integer bilinear
 * weights and AF accumulation, with floor/ceiling plan binding amortized
 * across scanline pixels. Regression comparisons against R313's public path. */
#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/cry.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *q,size_t n)
{(void)q;(void)n;return (u16)(((unsigned)r<<8u) | ((unsigned)g^b));}
static uint32_t seed=0x314ca7efu;
static uint32_t random_u32(void)
{seed^=seed<<13u;seed^=seed>>17u;seed^=seed<<5u;return seed;}
static double rnd_uv(void)
{ return ((double)(random_u32() & 0xffffffu)/16777215.0)*3.4-1.2; }
static double wallclock(void)
{
#ifdef _WIN32
    /* The fixture is built on Windows too; its optional benchmark has
     * a portable CPU-clock fallback when POSIX monotonic time is absent. */
    return (double)clock()/(double)CLOCKS_PER_SEC;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (double)t.tv_sec+(double)t.tv_nsec*1e-9;
#endif
}
static volatile uint64_t checksum_sink;
int main(int argc,char **argv)
{
    uint32_t *map=malloc(65536u*sizeof(uint32_t));assert(map);
    for(unsigned i=0;i<65536u;i++)map[i]=random_u32()&0xffffffu;
    const unsigned dims[][2]={{64,64},{256,128},{1024,1024},{127,239},{1254,1254}};
    unsigned long long comparisons=0,fallbacks=0;
    for(unsigned d=0;d<sizeof(dims)/sizeof(dims[0]);d++){
        const unsigned w=dims[d][0],h=dims[d][1];
        uint8_t *rgb=malloc((size_t)w*h*3u);
        uint16_t *cry=malloc((size_t)w*h*sizeof(uint16_t));
        uint8_t *alpha=malloc((size_t)w*h);
        assert(rgb&&cry&&alpha);
        for(size_t i=0;i<(size_t)w*h;i++){
            cry[i]=(uint16_t)random_u32();
            rgb[3*i]=(uint8_t)random_u32();
            rgb[3*i+1]=(uint8_t)random_u32();
            rgb[3*i+2]=(uint8_t)random_u32();
            alpha[i]=255u;
        }
        AvpGpuRgbImage src={0};src.rgb=rgb;src.cry16=cry;
        src.width=(int)w;src.height=(int)h;
        AvpFilterImage mip={0};mip.levels=3u;
        mip.mip[1].width=w/2u;mip.mip[1].height=h/2u;
        mip.mip[2].width=w/4u;mip.mip[2].height=h/4u;
        mip.mip[1].cry=cry;mip.mip[2].cry=cry;
        for(unsigned level=0;level<3;level++){
            for(unsigned sh=0;sh<5;sh++){
                int shade=(int[]){-512,-57,0,43,512}[sh];
                uint8_t lut[256];avp_filter_make_shade_lut(lut,shade);
                for(unsigned ti=0;ti<6;ti++){
                    const unsigned taps=(unsigned[]){1,2,3,4,8,16}[ti];
                    AvpFilterPlan p={level,taps,1u,rnd_uv()*0.2,rnd_uv()*0.2};
                    AvpFilterCryFloorBlock block;
                    int fast=avp_filter_cry_floor_block_prepare(&block,&src,&mip,p,1,map,lut);
                    assert(fast==1);
                    for(unsigned i=0;i<800;i++){
                        double u=rnd_uv(),v=rnd_uv();unsigned a=0;
                        uint32_t orig=avp_filter_sample_prepared_shaded(&src,&mip,u,v,p,1,shade,map,&a,lut);
                        if(fast){
                            uint32_t now=avp_filter_cry_floor_block_sample(&block,u,v);
                            if(orig!=now||a!=255u){
                                fprintf(stderr,"R314 mismatch %ux%u level %u taps %u shade %d UV %.17g %.17g original %08x new %08x alpha %u\n",
                                    w,h,level,taps,shade,u,v,orig,now,a);
                                return 1;
                            }
                            comparisons++;
                        }else fallbacks++;
                    }
                    assert(!avp_filter_cry_floor_block_prepare(&block,&src,&mip,p,0,map,lut));fallbacks++;
                    AvpFilterPlan nearest=p;nearest.bilinear=0u;
                    assert(!avp_filter_cry_floor_block_prepare(&block,&src,&mip,nearest,1,map,lut));fallbacks++;
                    src.alpha=alpha;
                    assert(!avp_filter_cry_floor_block_prepare(&block,&src,&mip,p,1,map,lut));fallbacks++;
                    src.alpha=NULL;
                    assert(!avp_filter_cry_floor_block_prepare(&block,&src,&mip,p,1,NULL,lut));fallbacks++;
                }
            }
        }
        free(rgb);free(cry);free(alpha);
    }
    printf("R314: %llu exact filtered-pixel comparisons; %llu correct fallbacks PASS\n",
        comparisons,fallbacks);
    free(map);
    if(argc>1 && strcmp(argv[1],"--bench")==0){
        for(unsigned dim=0;dim<2u;dim++){
        const unsigned w=dim?1254u:128u,h=w;
        uint16_t *cry=malloc(w*h*sizeof(uint16_t));
        uint8_t *rgb=malloc(w*h*3u);
        map=malloc(65536u*sizeof(uint32_t));
        assert(cry&&rgb&&map);
        for(unsigned i=0;i<w*h;i++)cry[i]=(uint16_t)random_u32();
        memset(rgb,0,w*h*3u);
        for(unsigned i=0;i<65536u;i++)map[i]=random_u32()&0xffffffu;
        AvpGpuRgbImage src={0};src.width=w;src.height=h;src.rgb=rgb;src.cry16=cry;
        const unsigned taps=8;
        uint8_t lut[256];avp_filter_make_shade_lut(lut,-31);
        AvpFilterPlan p={0u,taps,1u,0.084,0.037};
        AvpFilterCryFloorBlock block;
        assert(avp_filter_cry_floor_block_prepare(&block,&src,NULL,p,1,map,lut));
        /* Identical deterministic UV sequence and sampling count. Alternate
         * three times to reduce effects of processor scaling/load. */
        for(unsigned repeat=0;repeat<3;repeat++)for(int method=0;method<2;method++){
            uint64_t checksum=0u;
            const double start=wallclock();
            for(unsigned y=0;y<540;y++){
                double v=((double)y+0.5)/540.0;
                for(unsigned x=0;x<1920;x++){
                    const double u=((double)x+0.5)/1920.0;
                    uint32_t c;
                    if(method==0){unsigned a=0;
                        c=avp_filter_sample_prepared_shaded(&src,NULL,u,v,p,1,-31,map,&a,lut);
                    }else c=avp_filter_cry_floor_block_sample(&block,u,v);
                    checksum+=c;
                }
            }
            const double elapsed=wallclock()-start;
            checksum_sink+=checksum;
            printf("BENCH R314 size=%u repeat=%u method=%s sec=%.6f checksum=%llu\n",
                w,repeat,method?"bound":"R313_generic",elapsed,
                (unsigned long long)checksum);
        }
        free(cry);free(rgb);free(map);
        }
    }
    return 0;
}
