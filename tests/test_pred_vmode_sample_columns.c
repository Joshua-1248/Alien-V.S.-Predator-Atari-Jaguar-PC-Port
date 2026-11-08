/* R129: compare the production TOM video pass with R128's literal per-row
 * source-column calculations. Synthetic pixels/QCONV; no Jaguar ROM/assets. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s16 player_type=PT_PREDATOR;
u8 invis_stat=2;
u16 pred_vision_vmode;

/* Exact R128 input-column expressions, before the prebinding optimization.
 * The output expansion remains the unchanged R128 native-span code. */
static void r128_predator_vmode(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb)
{
    unsigned mode=(unsigned)(pred_vision_vmode&6u);
    const u8 *q=state->qconv;
    size_t n=state->qconv_size;
    unsigned bounds[321];
    if(!state || !fb || !fb->pixels || player_type!=PT_PREDATOR ||
       invis_stat==0 || !pred_vision_vmode)return;
    if(mode!=6u && mode!=2u)return;
    if(!q || n<131072u)return;
    avp_pred_vmode_span_bounds(bounds,fb->width,mode==6u?320u:160u);
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
            avp_pred_vmode_expand_row(fb->pixels+(size_t)y*w,native,320u,bounds);
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
            avp_pred_vmode_expand_row(fb->pixels+(size_t)y*w,pairs,160u,bounds);
        }
    }
}

static void fill_frame(u32 *pixels,size_t count,unsigned seed)
{
    for(size_t i=0;i<count;i++){
        u32 v=(u32)(i*2654435761u+seed*0x9e3779b9u);
        pixels[i]=(v^(v>>7)^(v<<9))&0xffffffu;
    }
}

int main(void)
{
    static const unsigned sizes[][2]={
        {1,1},{2,1},{7,4},{159,9},{160,7},{161,5},
        {319,12},{320,228},{321,229},{853,480},{1280,720},
        {1920,1080},{2560,1440},{3440,1440},{3840,2160},
        {0,0},{0,1},{12,0}
    };
    const unsigned modes[2]={6u,2u};
    AvpAvpGpuState *state=calloc(1,sizeof(*state));
    u8 *q=malloc(131072u);
    unsigned checks=0;
    assert(state&&q);
    for(size_t j=0;j<131072u;j++)q[j]=(u8)((j*157u+19u)^(j>>7));
    state->qconv=q;state->qconv_size=131072u;

    /* Exhaustively check all 320 native sample columns over small and
     * common desktop widths, including the one-pixel and zero-width cases. */
    for(unsigned mi=0;mi<2u;mi++){
        unsigned mode=modes[mi];
        for(unsigned w=1;w<=4096u;w++){
            unsigned columns[320];
            avp_pred_vmode_sample_columns(columns,w,mode);
            for(unsigned j=0;j<320u;j++){
                unsigned factor=mode==6u?(2u*j+1u):(4u*(j/2u)+1u+(j%2u)*2u);
                unsigned old=(unsigned)(((uint64_t)factor*w)/640u);
                if(old>=w)old=w-1u;
                assert(columns[j]==old);
            }
        }
    }
    for(unsigned mi=0;mi<2u;mi++){
        pred_vision_vmode=(u16)modes[mi];
        for(unsigned si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++){
            unsigned w=sizes[si][0],h=sizes[si][1];
            size_t count=(size_t)w*h;
            u32 *a=malloc((count?count:1u)*sizeof(u32));
            u32 *b=malloc((count?count:1u)*sizeof(u32));
            AvpJaguarFramebuffer fb={w,h,NULL};
            assert(a&&b);
            for(unsigned pattern=0;pattern<2u;pattern++){
                fill_frame(a,count,pattern+si*13u);
                memcpy(b,a,count*sizeof(u32));
                fb.pixels=a;r128_predator_vmode(state,&fb);
                fb.pixels=b;present_predator_vmode(state,&fb);
                if(memcmp(a,b,count*sizeof(u32))){
                    fprintf(stderr,"R129 mismatch mode %u %ux%u variant %u\n",
                            modes[mi],w,h,pattern);
                    abort();
                }
                checks++;
            }
            free(a);free(b);
        }
    }
    printf("Predator TOM native input-sample prebinding parity PASS (%u complete frames)\n",checks);
    free(q);free(state);
    return 0;
}
