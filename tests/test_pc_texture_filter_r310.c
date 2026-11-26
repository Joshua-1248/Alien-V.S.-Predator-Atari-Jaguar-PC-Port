/* RE #45 R310 -- source-equivalent mip selection and full-tap ANISO check.
 * The native Jaguar and original point-sampling paths are not modified. */
#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *qconv,size_t len)
{(void)qconv;(void)len;return (u16)((r<<8u)|g|b);}
static uint32_t random_state=0x72ab391bu;
static uint32_t random_word(void)
{
    random_state^=random_state<<13u;
    random_state^=random_state>>17u;
    random_state^=random_state<<5u;
    return random_state;
}
static double random_frac(void){return (double)(random_word()&0xffffu)/65535.0;}
static AvpFilterPlan original_plan(const AvpGpuRgbImage *s,const AvpFilterImage *mips,
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
/* R309 exact multi-tap arithmetic, synthesized from one-tap calls to the
 * unchanged sampler. Compare all color and alpha channels independently. */
static uint32_t old_taps(const AvpGpuRgbImage *s,const AvpFilterImage *mips,
    double u,double v,AvpFilterPlan p,int wrap,int shade,const uint32_t *map,unsigned *alpha)
{
    if(p.taps<=1u)return avp_filter_sample_prepared(s,mips,u,v,p,wrap,shade,map,alpha);
    AvpFilterPlan single=p;single.taps=1u;
    if(!s->alpha){
        unsigned r=0,g=0,b=0;
        for(unsigned i=0;i<p.taps;i++){
            const double offs=((double)i+0.5)/(double)p.taps-0.5;
            unsigned a=255u;
            uint32_t c=avp_filter_sample_prepared(s,mips,u+offs*p.du,v+offs*p.dv,
                single,wrap,shade,map,&a);
            r+=(c>>16u)&255u;g+=(c>>8u)&255u;b+=c&255u;
        }
        *alpha=255u;
        return (((r+p.taps/2u)/p.taps)<<16u)|
               (((g+p.taps/2u)/p.taps)<<8u)|((b+p.taps/2u)/p.taps);
    }
    double rr=0.0,gg=0.0,bb=0.0,aa=0.0;
    for(unsigned i=0;i<p.taps;i++){
        double offs=((double)i+0.5)/(double)p.taps-0.5;
        unsigned a=255u;
        uint32_t c=avp_filter_sample_prepared(s,mips,u+offs*p.du,v+offs*p.dv,
            single,wrap,shade,map,&a);
        aa+=a;rr+=(double)((c>>16u)&255u)*a;
        gg+=(double)((c>>8u)&255u)*a;bb+=(double)(c&255u)*a;
    }
    *alpha=(unsigned)(aa/(double)p.taps+0.5);
    if(aa<0.01)return 0u;
    return ((uint32_t)(rr/aa+0.5)<<16u)|((uint32_t)(gg/aa+0.5)<<8u)|
           (uint32_t)(bb/aa+0.5);
}
int main(void)
{
    const unsigned w=64u,h=64u;
    uint8_t rgb[64u*64u*3u],alpha[64u*64u];
    uint32_t *map=(uint32_t*)malloc(65536u*sizeof(uint32_t));
    assert(map);
    for(unsigned i=0;i<65536u;i++)map[i]=i*0x169u;
    for(unsigned i=0;i<w*h;i++){
        rgb[3u*i]=(uint8_t)random_word();
        rgb[3u*i+1u]=(uint8_t)random_word();
        rgb[3u*i+2u]=(uint8_t)random_word();
        alpha[i]=(uint8_t)random_word();
    }
    AvpGpuRgbImage source={0};source.width=w;source.height=h;source.rgb=rgb;
    for(unsigned is_alpha=0;is_alpha<2u;is_alpha++){
        source.alpha=is_alpha?alpha:NULL;
        AvpFilterImage mips={0};assert(avp_filter_image_build(&mips,&source,NULL,0));
        for(unsigned j=0;j<30000u;j++){
            /* Positive and negative derivatives exercising small/large LODs. */
            const double scale=0.0003+random_frac()*0.35;
            AvpFilterDerivatives dd={scale*(2.0*random_frac()-1.0),
                scale*(2.0*random_frac()-1.0),
                scale*(2.0*random_frac()-1.0),
                scale*(2.0*random_frac()-1.0)};
            unsigned mode=(j%3u),aniso=((j/3u)%5u)?(1u<<((j/3u)%5u)):0u;
            if(aniso>16u)aniso=16u;
            AvpFilterPlan old=original_plan(&source,&mips,dd,mode,aniso);
            AvpFilterPlan now=avp_filter_plan(&source,&mips,dd,mode,aniso);
            assert(old.level==now.level&&old.taps==now.taps&&old.bilinear==now.bilinear);
            assert(old.du==now.du&&old.dv==now.dv);
        }
        for(unsigned taps=2u;taps<=16u;taps++){
            for(unsigned j=0;j<1200u;j++){
                AvpFilterPlan p={0u,taps,j&1u,
                    (random_frac()-0.5)*0.12,(random_frac()-0.5)*0.12};
                const double u=random_frac(),v=random_frac();
                unsigned olda=0u,newa=0u;
                uint32_t a=old_taps(&source,&mips,u,v,p,1,-18,map,&olda);
                uint32_t b=avp_filter_sample_prepared(&source,&mips,u,v,p,1,-18,map,&newa);
                if(a!=b||olda!=newa){
                    fprintf(stderr,"Mismatch taps %u iter %u: %08x %08x %u %u\n",
                        taps,j,a,b,olda,newa);abort();
                }
            }
        }
        avp_filter_image_destroy(&mips);
    }
    free(map);
    puts("R310: mip plan source equivalence and 36000 exact full-tap samples PASS");
    return 0;
}
