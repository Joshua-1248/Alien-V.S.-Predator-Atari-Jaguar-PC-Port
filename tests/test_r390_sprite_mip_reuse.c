/* R390: repeat-visible AAF mip rebuild must match a fresh mip on every pixel.
 * Compare the actual image data, including CRY opacity and odd dimensions.
 */
#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/avp_gpu/sprite_scratch.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"R390 FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)
static uint32_t seed=0x3909e8cdu;
static uint32_t next(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static int compare(const AvpFilterImage*a,const AvpFilterImage*b){
    CHECK(a->levels==b->levels);
    for(unsigned l=1;l<a->levels;l++){
        const AvpFilterMip*x=&a->mip[l],*y=&b->mip[l];
        CHECK(x->width==y->width && x->height==y->height);
        const size_t n=(size_t)x->width*x->height;
        CHECK(x->rgba && y->rgba);
        CHECK(memcmp(x->rgba,y->rgba,n*4u)==0);
        CHECK((x->cry!=NULL)==(y->cry!=NULL));
        if(x->cry)CHECK(memcmp(x->cry,y->cry,n*sizeof(uint16_t))==0);
    }
    return 0;
}
int main(void){
    AvpGpuSpriteScratch view[4]={{0}};
    uint32_t *cry_rgb=(uint32_t*)malloc(65536u*sizeof(uint32_t));
    CHECK(cry_rgb);
    for(unsigned i=0;i<65536u;i++)cry_rgb[i]=(i*273u)&0xffffffu;
    uint8_t *qconv=(uint8_t*)malloc(131072u);
    CHECK(qconv);
    for(size_t j=0;j<131072u;j++)qconv[j]=(uint8_t)(j^0x9fu);
    uint64_t exact=0,retained=0;
    const unsigned sizes[][2]={{8,9},{41,63},{128,128},{3,3},{150,131},{33,11},{255,255},{1,37}};
    for(unsigned frame=0;frame<320u;frame++){
        AvpGpuSpriteScratch *s=&view[frame&3u];
        const unsigned ix=(frame/32u)%8u;
        unsigned w=sizes[ix][0],h=sizes[ix][1];
        const size_t n=(size_t)w*h;
        CHECK(avp_gpu_sprite_scratch_prepare(s,n));
        for(size_t i=0;i<n;i++){
            const uint32_t v=next();
            s->rgb[i*3u]=(uint8_t)v;
            s->rgb[i*3u+1u]=(uint8_t)(v>>8);
            s->rgb[i*3u+2u]=(uint8_t)(v>>16);
            s->alpha[i]=(uint8_t)(v>>24);
        }
        AvpGpuRgbImage src={(int)w,(int)h,s->rgb,s->cry16,s->alpha};
        AvpFilterImage fresh={0};
        const int use_cry=(frame%3u)!=0u;
        const uint8_t *q=use_cry?qconv:NULL;
        const size_t qn=use_cry?131072u:0u;
        CHECK(avp_filter_image_build(&fresh,&src,q,qn));
        uint8_t *saved=s->mip_chain.mip[1].rgba;
        const int compatible=s->mip_chain.source_width==(int)w && s->mip_chain.source_height==(int)h;
        CHECK(avp_filter_image_rebuild(&s->mip_chain,&src,q,qn));
        CHECK(compare(&fresh,&s->mip_chain)==0);
        if(compatible && saved){CHECK(saved==s->mip_chain.mip[1].rgba);retained++;}
        AvpFilterDerivatives d={0.12,0,0,0.12};
        AvpFilterPlan a=avp_filter_plan(&src,&fresh,d,AVP_TEXFILTER_NEAREST_MIP,0);
        AvpFilterPlan b=avp_filter_plan(&src,&s->mip_chain,d,AVP_TEXFILTER_NEAREST_MIP,0);
        CHECK(a.level==b.level && a.taps==b.taps && a.bilinear==b.bilinear && a.du==b.du && a.dv==b.dv);
        for(int k=0;k<160;k++){
            const double u=(double)(next()%16384u)/16383.0;
            const double v=(double)(next()%16384u)/16383.0;
            unsigned aa=0,ab=0;
            uint32_t ca=avp_filter_sample_prepared(&src,&fresh,u,v,a,0,0,cry_rgb,&aa);
            uint32_t cb=avp_filter_sample_prepared(&src,&s->mip_chain,u,v,b,0,0,cry_rgb,&ab);
            CHECK(ca==cb && aa==ab);
            exact++;
        }
        avp_filter_image_destroy(&fresh);
    }
    /* Cloning a local viewport detaches all owned pixels, including mips. */
    AvpGpuSpriteScratch borrowed=view[0];
    avp_gpu_sprite_scratch_detach(&borrowed);
    CHECK(!borrowed.mip_chain.levels && !borrowed.rgb);
    avp_gpu_sprite_scratch_destroy(&borrowed);
    for(unsigned i=0;i<4u;i++)avp_gpu_sprite_scratch_destroy(&view[i]);
    free(qconv);
    free(cry_rgb);
    printf("R390: 320 source frames, %llu identical mip samples, %llu retained allocations PASS\n",
           (unsigned long long)exact,(unsigned long long)retained);
    return 0;
}
