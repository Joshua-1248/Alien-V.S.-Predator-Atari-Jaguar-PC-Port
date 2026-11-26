/* R317: non-power-of-two wrapped AF samples, including four texels at seams.
 * Compare the rewritten host-only sampler against the original generic
 * sampling implementation, which still performs four independent coord()
 * operations per tap. Image input, tap order and bilinear math are unchanged. */
#include "jaguar/avp_gpu/texture_filter.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *q,size_t n)
{(void)q;(void)n;return (u16)(((unsigned)r<<8u)|((unsigned)g^b));}
static unsigned seed=0x31716af0u;
static unsigned rand32(void)
{seed^=seed<<13u;seed^=seed>>17u;seed^=seed<<5u;return seed;}
static double seconds(void)
{
#ifdef _WIN32
    return (double)clock()/(double)CLOCKS_PER_SEC;
#else
    struct timespec tv;clock_gettime(CLOCK_MONOTONIC,&tv);
    return (double)tv.tv_sec+(double)tv.tv_nsec*1e-9;
#endif
}
static volatile uint64_t sink=0u;
int main(int argc,char **argv)
{
    const unsigned sizes[][2]={{1254u,1254u},{627u,627u},{127u,239u},{63u,65u},{1u,19u}};
    unsigned long long pixels=0;
    uint32_t *map=(uint32_t*)malloc(65536u*sizeof(uint32_t));assert(map);
    for(unsigned i=0;i<65536u;i++)map[i]=rand32()&0xffffffu;
    for(unsigned d=0;d<5u;d++){
        const unsigned w=sizes[d][0],h=sizes[d][1];
        uint16_t *cry=(uint16_t*)malloc((size_t)w*h*sizeof(uint16_t));
        uint8_t *rgb=(uint8_t*)malloc((size_t)w*h*3u);
        assert(cry && rgb);
        memset(rgb,0,(size_t)w*h*3u);
        for(size_t i=0;i<(size_t)w*h;i++)cry[i]=(uint16_t)rand32();
        AvpGpuRgbImage tex={0};tex.width=(int)w;tex.height=(int)h;
        tex.cry16=cry;tex.rgb=rgb;
        for(unsigned si=0;si<5u;si++){
            int shade=(const int[]){-512,-61,0,71,512}[si];
            uint8_t lut[256];avp_filter_make_shade_lut(lut,shade);
            for(unsigned tap=1u;tap<=16u;tap*=2u){
                AvpFilterPlan p={0u,tap,1u,0.037,0.014};
                AvpFilterCryFloorBlock block;
                assert(avp_filter_cry_floor_block_prepare(&block,&tex,NULL,p,1,map,lut)==1);
                /* Critical seams plus far-wrapped coordinates and arbitrary
                 * fractions. Random values stay away from undefined int casts. */
                const double seam[]={0.0,1.0,-1.0,2.0,-2.0,
                                     0.0000001,0.9999999,-0.0000001,1.0000001};
                for(unsigned i=0;i<512u;i++){
                    double u=(i<81u)?seam[(i/9u)%9u]:((double)(rand32()%65536u)/8192.0-4.0);
                    double v=(i<81u)?seam[i%9u]:((double)(rand32()%65536u)/8192.0-4.0);
                    unsigned alpha=255u;
                    uint32_t old=avp_filter_sample_prepared_shaded(
                        &tex,NULL,u,v,p,1,shade,map,&alpha,lut);
                    uint32_t now=avp_filter_cry_floor_block_sample(&block,u,v);
                    if(now!=old||alpha!=255u){
                        fprintf(stderr,"R317 mismatch size=%ux%u shade=%d taps=%u uv=%.17g %.17g old=%08x new=%08x alpha=%u\n",
                            w,h,shade,tap,u,v,old,now,alpha);
                        return 1;
                    }
                    pixels++;
                }
            }
        }
        free(cry);free(rgb);
    }
    printf("R317: %llu exact NPOT filtered comparisons, including wrapped seams PASS\n",pixels);
    free(map);
    if(argc>1 && strcmp(argv[1],"--bench")==0){
        const unsigned w=1254u,h=w;
        uint16_t *cry=(uint16_t*)malloc(w*h*sizeof(uint16_t));
        uint8_t *rgb=(uint8_t*)calloc((size_t)w*h,3u);
        map=(uint32_t*)malloc(65536u*sizeof(uint32_t));
        assert(cry && rgb && map);
        for(size_t i=0;i<(size_t)w*h;i++)cry[i]=(uint16_t)rand32();
        for(unsigned i=0;i<65536u;i++)map[i]=rand32()&0xffffffu;
        AvpGpuRgbImage tex={0};tex.width=w;tex.height=h;tex.cry16=cry;tex.rgb=rgb;
        uint8_t lut[256];avp_filter_make_shade_lut(lut,-31);
        AvpFilterPlan p={0u,8u,1u,0.084,0.037};
        AvpFilterCryFloorBlock block;
        assert(avp_filter_cry_floor_block_prepare(&block,&tex,NULL,p,1,map,lut));
        for(unsigned pass=0;pass<5u;pass++){
            const double start=seconds();uint64_t sum=0;
            for(unsigned y=0;y<540u;y++){
                const double v=((double)y+0.5)/540.0;
                for(unsigned x=0;x<1440u;x++){
                    const double u=((double)x+0.5)/1440.0;
                    sum+=avp_filter_cry_floor_block_sample(&block,u,v);
                }
            }
            sink+=sum;
            printf("R317 NPOT 1440x540 8xAF pass=%u seconds=%.6f checksum=%llu\n",
                    pass,seconds()-start,(unsigned long long)sum);
        }
        free(map);free(rgb);free(cry);
    }
    return 0;
}
