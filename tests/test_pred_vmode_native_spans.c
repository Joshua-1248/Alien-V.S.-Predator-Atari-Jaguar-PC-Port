/* Production TOM multispectrum output vs. the R127 per-destination-pixel
 * nearest-neighbour expansion. Synthetic RGB/QCONV values; no retail assets. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s16 player_type=PT_PREDATOR;
u8 invis_stat=2;
u16 pred_vision_vmode;

/* Copy of R127's exact two format branches; intentionally do not call any
 * R128 span helper here, so this is an independent previous-code oracle. */
static void r127_predator_vmode(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb)
{
    const unsigned mode=(unsigned)(pred_vision_vmode&6u);
    const u8 *q=state->qconv;
    size_t n=state->qconv_size;
    if(mode==6u){
        for(unsigned y=0;y<fb->height;y++){
            u32 native[320];
            unsigned w=fb->width;
            if(!w)break;
            for(unsigned i=0;i<320u;i++){
                unsigned sx=(unsigned)(((unsigned long long)(2u*i+1u)*w)/640u);
                u32 c;u16 word;
                if(sx>=w)sx=w-1u;
                c=fb->pixels[(size_t)y*w+sx];
                word=avp_jaguar_rgb_to_cry_qconv((u8)(c>>16),(u8)(c>>8),(u8)c,q,n);
                native[i]=avp_jaguar_vmode_rgb16(word);
            }
            for(unsigned x=0;x<w;x++){
                unsigned i=(unsigned)(((unsigned long long)x*320u)/w);
                if(i>=320u)i=319u;
                fb->pixels[(size_t)y*w+x]=native[i];
            }
        }
    }else{
        for(unsigned y=0;y<fb->height;y++){
            u32 pairs[160];
            unsigned w=fb->width;
            if(!w)break;
            for(unsigned i=0;i<160u;i++){
                unsigned x0=(unsigned)(((unsigned long long)(4u*i+1u)*w)/640u);
                unsigned x1=(unsigned)(((unsigned long long)(4u*i+3u)*w)/640u);
                u32 c0,c1;u16 first,second;
                if(x0>=w)x0=w-1u;
                if(x1>=w)x1=w-1u;
                c0=fb->pixels[(size_t)y*w+x0];
                c1=fb->pixels[(size_t)y*w+x1];
                first=avp_jaguar_rgb_to_cry_qconv((u8)(c0>>16),(u8)(c0>>8),(u8)c0,q,n);
                second=avp_jaguar_rgb_to_cry_qconv((u8)(c1>>16),(u8)(c1>>8),(u8)c1,q,n);
                pairs[i]=avp_jaguar_vmode_rgb24(first,second);
            }
            for(unsigned x=0;x<w;x++){
                unsigned i=(unsigned)(((unsigned long long)x*160u)/w);
                if(i>=160u)i=159u;
                fb->pixels[(size_t)y*w+x]=pairs[i];
            }
        }
    }
}

static void fill_frame(u32 *pixels,size_t count,unsigned variant)
{
    for(size_t k=0;k<count;k++){
        u32 v=(u32)(k*2654435761u+variant*0x9e3779b9u);
        pixels[k]=(v^(v>>7)^(v<<9))&0xffffffu;
    }
}

int main(void)
{
    static const unsigned sizes[][2]={
        {1,1},{2,1},{7,4},{159,9},{160,7},{161,5},
        {319,12},{320,228},{321,229},{853,480},{1280,720},
        {1920,1080},{2560,1440},{3440,1440},{3840,2160}
    };
    const unsigned modes[2]={6u,2u};
    AvpAvpGpuState *state=calloc(1,sizeof(*state));
    u8 *q=malloc(131072u);
    assert(state&&q);
    for(size_t j=0;j<131072u;j++)q[j]=(u8)((j*157u+19u)^(j>>7));
    state->qconv=q;state->qconv_size=131072u;
    for(unsigned words=160u;words<=320u;words+=160u){
        for(unsigned w=0;w<=1025u;w++){
            unsigned boundaries[321];
            avp_pred_vmode_span_bounds(boundaries,w,words);
            assert(boundaries[0]==0&&boundaries[words]==w);
            for(unsigned i=0;i<words;i++){
                assert(boundaries[i]<=boundaries[i+1u]);
                for(unsigned x=boundaries[i];x<boundaries[i+1u];x++)
                    assert((unsigned)(((uint64_t)x*words)/w)==i);
            }
        }
    }
    unsigned total=0;
    for(unsigned mi=0;mi<2u;mi++){
        pred_vision_vmode=(u16)modes[mi];
        for(unsigned si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++){
            const unsigned w=sizes[si][0],h=sizes[si][1];
            const size_t count=(size_t)w*h;
            u32 *a=malloc(count*sizeof(u32)),*b=malloc(count*sizeof(u32));
            AvpJaguarFramebuffer fb={w,h,NULL};
            assert(a&&b);
            for(unsigned pattern=0;pattern<2u;pattern++){
                fill_frame(a,count,pattern+si*13u);
                memcpy(b,a,count*sizeof(u32));
                fb.pixels=a;r127_predator_vmode(state,&fb);
                fb.pixels=b;present_predator_vmode(state,&fb);
                if(memcmp(a,b,count*sizeof(u32))){
                    fprintf(stderr,"Mismatch: mode %u, %ux%u, pattern %u\n",modes[mi],w,h,pattern);
                    abort();
                }
                total++;
            }
            free(a);free(b);
        }
    }
    printf("Predator TOM RGB16/RGB24 native-span parity PASS (%u complete frames)\n",total);
    free(q);free(state);
    return 0;
}
