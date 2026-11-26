/* R311: exact source-texel filtering and anisotropic-plan regression.
 * Compare the row-local CRY shade table against the R310 public sampler;
 * the original Jaguar point-sampler remains an independent unchanged path. */
#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *q,size_t len)
{(void)q;(void)len;return (u16)((((unsigned)r&255u)<<8u)|((g^b)&255u));}
static uint32_t seed=0x572c9a3fu;
static uint32_t rng(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static double rnd(void){return (double)(rng()&0xffffffu)/16777215.0;}
static AvpFilterPlan legacy_plan(const AvpGpuRgbImage *s,const AvpFilterImage *mips,
    AvpFilterDerivatives dd,unsigned mode,unsigned aniso)
{
    AvpFilterPlan p={0u,1u,mode==AVP_TEXFILTER_BILINEAR,0.0,0.0};
    if(!s||s->width<=0||s->height<=0)return p;
    if(aniso<=1u&&(mode!=AVP_TEXFILTER_NEAREST_MIP||!mips||mips->levels<=1u))return p;
    double a=hypot(dd.dudx*s->width,dd.dvdx*s->height);
    double b=hypot(dd.dudy*s->width,dd.dvdy*s->height);
    double major=a>b?a:b,minor=a<b?a:b;
    if(!isfinite(major)||!isfinite(minor)){major=1.0;minor=1.0;}
    if(aniso>1u&&major>minor+0.0001&&major>1.0){
        double ratio=major/(minor>0.001?minor:0.001);
        p.taps=ratio>=(double)aniso?aniso:(unsigned)ceil(ratio);
        if(p.taps<1u)p.taps=1u;
    }
    if(mips&&mips->levels>1u&&(mode==AVP_TEXFILTER_NEAREST_MIP||p.taps>1u)){
        double lod=p.taps>1u?minor:major;
        if(lod>1.0){
            int ilod=(int)floor(log2(lod)+0.5);
            if(ilod>0)p.level=(unsigned)ilod>=mips->levels?mips->levels-1u:(unsigned)ilod;
        }
    }
    if(p.taps>1u){p.du=a>b?dd.dudx:dd.dudy;p.dv=a>b?dd.dvdx:dd.dvdy;}
    return p;
}
static void check_plan(const AvpGpuRgbImage *s,const AvpFilterImage *mips,
    AvpFilterDerivatives dd,unsigned mode,unsigned aniso)
{
    AvpFilterPlan a=legacy_plan(s,mips,dd,mode,aniso);
    AvpFilterPlan b=avp_filter_plan(s,mips,dd,mode,aniso);
    if(a.level!=b.level||a.taps!=b.taps||a.bilinear!=b.bilinear||
       a.du!=b.du||a.dv!=b.dv){
        fprintf(stderr,"R311 plan mismatch: mode=%u aniso=%u level=%u/%u taps=%u/%u\n",
            mode,aniso,a.level,b.level,a.taps,b.taps);
        abort();
    }
}
int main(void)
{
    uint8_t rgb[64*64*3], alpha[64*64],qconv[131072]={0};
    uint16_t cry[64*64];
    uint32_t *map=malloc(65536u*sizeof(*map));assert(map);
    for(unsigned i=0;i<65536u;i++)map[i]=((rng()&0xffu)<<16u)|
        ((rng()&0xffu)<<8u)|(rng()&0xffu);
    for(unsigned i=0;i<64*64u;i++){
        rgb[i*3u]=(uint8_t)rng();rgb[i*3u+1u]=(uint8_t)rng();
        rgb[i*3u+2u]=(uint8_t)rng();alpha[i]=(uint8_t)rng();cry[i]=(uint16_t)rng();
    }
    AvpGpuRgbImage s={0};s.width=64;s.height=64;s.rgb=rgb;
    unsigned long long pixels=0,plans=0;
    for(unsigned has_alpha=0;has_alpha<2;has_alpha++)
    for(unsigned has_cry=0;has_cry<2;has_cry++)
    for(unsigned mip_cry=0;mip_cry<2;mip_cry++){
        s.alpha=has_alpha?alpha:NULL;s.cry16=has_cry?cry:NULL;
        AvpFilterImage mips={0};
        assert(avp_filter_image_build(&mips,&s,mip_cry?qconv:NULL,mip_cry?sizeof(qconv):0));
        for(unsigned shade_case=0;shade_case<6;shade_case++){
            int shade=(int[]){-512,-255,-32,0,55,512}[shade_case];
            uint8_t lut[256];avp_filter_make_shade_lut(lut,shade);
            for(unsigned i=0;i<256u;i++)assert(lut[i]==(uint8_t)((int)i+shade<0?0:(int)i+shade>255?255:(int)i+shade));
            for(unsigned i=0;i<3000;i++){
                AvpFilterPlan p={0};
                p.level=rng()%mips.levels;
                p.taps=(unsigned[]){1u,2u,4u,8u,16u}[rng()%5u];
                p.bilinear=rng()%2u;
                p.du=(rnd()-0.5)*0.4;p.dv=(rnd()-0.5)*0.4;
                double u=(rnd()-0.5)*6.0,v=(rnd()-0.5)*6.0;
                int wrap=rng()%2u;
                unsigned a=123,b=123;
                uint32_t c=avp_filter_sample_prepared(&s,&mips,u,v,p,wrap,shade,map,&a);
                uint32_t d=avp_filter_sample_prepared_shaded(&s,&mips,u,v,p,wrap,shade,map,&b,lut);
                if(c!=d||a!=b){
                    fprintf(stderr,"R311 shade mismatch mode=%u lvl=%u taps=%u at %.17g %.17g\n",p.bilinear,p.level,p.taps,u,v);
                    abort();
                }
                pixels++;
            }
        }
        for(unsigned i=0;i<100000u;i++){
            double scale=0.00001+0.8*rnd();
            AvpFilterDerivatives dd={scale*(rnd()*2-1),scale*(rnd()*2-1),
                                      scale*(rnd()*2-1),scale*(rnd()*2-1)};
            unsigned aniso=(unsigned[]){0u,2u,4u,8u,16u}[i%5];
            check_plan(&s,&mips,dd,i%3u,aniso);plans++;
        }
        /* Exercise the original integer-tap and sqrt(2)*2^LOD boundaries. */
        for(unsigned level=0;level<10u;level++){
            double major=sqrt(2.0)*ldexp(1.0,(int)level);
            for(int eps=-100;eps<=100;eps++){
                AvpFilterDerivatives dd={major*(1+eps*1e-13)/s.width,0,
                                         (major/4.0)/s.width,0};
                check_plan(&s,&mips,dd,1u,8u);plans++;
                for(unsigned ratio=1;ratio<=16;ratio++){
                    dd.dudy=(major/(double)ratio)*(1+eps*1e-13)/s.width;
                    check_plan(&s,&mips,dd,2u,16u);plans++;
                }
            }
        }
        avp_filter_image_destroy(&mips);
    }
    free(map);
    printf("R311: %llu equivalent filtered pixels, %llu exact original anisotropic plans PASS\n",
           pixels,plans);
    return 0;
}
