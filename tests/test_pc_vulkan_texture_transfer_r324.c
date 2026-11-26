/* Test the exact host/GLSL integer-texture layout without Vulkan drivers.
 * This is an independent arithmetic comparison, NOT a GPU pixel-parity test. */
#include "jaguar/avp_gpu/world_texture_transfer.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* World-packet source-resolver dependencies; no original ROM required. */
int avp_gpu_wall_texture_file(unsigned level,u8 wall,unsigned *file)
{ (void)level;(void)wall;(void)file;return 0; }
int avp_gpu_wall_notice_index(unsigned level,u8 wall,unsigned *index)
{ (void)level;(void)wall;(void)index;return 0; }
int avp_gpu_flat_texture_file(unsigned level,u8 flat,unsigned *file)
{ (void)level;(void)flat;(void)file;return 0; }
unsigned avp_gpu_flat_texture_rotation(unsigned level,u8 flat)
{ (void)level;(void)flat;return 0u; }
int avp_gpu_wall_window_pair_file(unsigned level,u8 wall,unsigned *file)
{ (void)level;(void)wall;(void)file;return 0; }

static uint32_t rng=0x9e3779b9u;
static uint32_t next_rand(void)
{ rng^=rng<<13u;rng^=rng>>17u;rng^=rng<<5u;return rng; }
static uint32_t ref_channel(uint32_t c,unsigned shift) {return (c>>shift)&255u;}
static uint32_t reference_pixel(const uint32_t *texels,unsigned enc,
                                const uint32_t *lut,unsigned wx,unsigned wy,int shade)
{
    uint64_t w[4],r=0,g=0,b=0;
    uint32_t c[4];
    w[0]=(1024u-wx)*(1024u-wy);w[1]=wx*(1024u-wy);
    w[2]=(1024u-wx)*wy;w[3]=wx*wy;
    for(unsigned i=0;i<4u;i++){
        uint32_t t=texels[i];
        if(enc==AVP_WORLD_IMAGE_CRY16_ALPHA8){
            int y=(int)(t&255u)+shade;
            if(y<0)y=0;else if(y>255)y=255;
            c[i]=lut[(t&0xff00u)|(unsigned)y];
        }else{
            uint32_t cr=ref_channel(t,16),cg=ref_channel(t,8),cb=ref_channel(t,0);
            if(shade<0){unsigned k=(unsigned)(shade< -255?0:255+shade);
                cr=cr*k/255u;cg=cg*k/255u;cb=cb*k/255u;}
            c[i]=(cr<<16u)|(cg<<8u)|cb;
        }
        r+=(uint64_t)ref_channel(c[i],16)*w[i];
        g+=(uint64_t)ref_channel(c[i],8)*w[i];
        b+=(uint64_t)ref_channel(c[i],0)*w[i];
    }
    return (uint32_t)(((r+524288u)>>20u)<<16u)|
           (uint32_t)(((g+524288u)>>20u)<<8u)|
           (uint32_t)((b+524288u)>>20u);
}
int main(void)
{
    AvpWorldGpuStaging s={0};
    uint32_t *lut=malloc((size_t)AVP_WORLD_GPU_CRY_LUT_WORDS*sizeof(uint32_t));
    uint16_t raw_cry[16];
    uint8_t rgba_alpha[16];
    uint8_t rgb[48];
    AvpGpuRgbImage tex={4,4,rgb,raw_cry,rgba_alpha};
    uint32_t decoded[4],expected;
    const unsigned sizes[2]={128u,1254u};
    assert(lut);
    assert(!avp_world_gpu_build_cry_lut(NULL,AVP_WORLD_GPU_CRY_LUT_WORDS));
    assert(!avp_world_gpu_build_cry_lut(lut,AVP_WORLD_GPU_CRY_LUT_WORDS-1u));
    assert(avp_world_gpu_build_cry_lut(lut,AVP_WORLD_GPU_CRY_LUT_WORDS));
    for(unsigned cry=0;cry<65536u;cry++){
        u8 r,g,b;avp_jaguar_cry16_to_rgb((u16)cry,&r,&g,&b);
        assert(lut[cry]==((uint32_t)r<<16u|((uint32_t)g<<8u)|b));
    }
    for(unsigned i=0;i<16u;i++){
        raw_cry[i]=(uint16_t)(0x101u*i+7u);
        rgba_alpha[i]=255u;
        rgb[i*3u]=(uint8_t)(i*7u);
        rgb[i*3u+1u]=(uint8_t)(i*5u);
        rgb[i*3u+2u]=(uint8_t)(i*3u);
    }
    assert(!avp_world_gpu_stage_image(&s,&tex,63u));
    assert(!s.valid && s.byte_count==0u);
    assert(avp_world_gpu_stage_image(&s,&tex,64u));
    assert(s.valid && s.byte_count==64u && s.width==4u && s.height==4u);
    assert(s.encoding==AVP_WORLD_IMAGE_CRY16_ALPHA8);
    assert(s.words[0]==(0xff0000u|raw_cry[0]));
    assert(s.words[15]==(0xff0000u|raw_cry[15]));
    rgba_alpha[0]=0u;
    assert(avp_world_gpu_stage_image(&s,&tex,64u));
    assert(s.words[0]==raw_cry[0]);
    assert(!avp_world_gpu_opaque_bilinear(s.words,4u,4u,
        AVP_WORLD_IMAGE_CRY16_ALPHA8,lut,0,1,0,1,0,0,0,&expected));
    rgba_alpha[0]=255u;
    assert(avp_world_gpu_stage_image(&s,&tex,64u));
    assert(avp_world_gpu_opaque_bilinear(s.words,4,4,
        AVP_WORLD_IMAGE_CRY16_ALPHA8,lut,0,1,0,1,0,0,0,&expected));
    assert(expected==lut[raw_cry[0]]);
    tex.cry16=NULL;
    tex.alpha=NULL;
    assert(avp_world_gpu_stage_image(&s,&tex,64u));
    assert(s.encoding==AVP_WORLD_IMAGE_RGB8_ALPHA8);
    assert(s.words[4]==(0xff000000u|((uint32_t)rgb[12]<<16u)|
                          ((uint32_t)rgb[13]<<8u)|rgb[14]));
    tex.width=0;
    assert(!avp_world_gpu_stage_image(&s,&tex,64u));
    assert(s.word_capacity>=16u && !s.valid && s.byte_count==0u);
    tex.width=4;
    assert(avp_world_gpu_stage_image(&s,&tex,64u));
    assert(!avp_world_gpu_opaque_bilinear(s.words,4,4,
        AVP_WORLD_IMAGE_RGB8_ALPHA8,lut,0,1,0,1,1025u,0,0,&expected));
    assert(!avp_world_gpu_opaque_bilinear(s.words,4,4,
        AVP_WORLD_IMAGE_RGB8_ALPHA8,lut,4,1,0,1,0u,0,0,&expected));

    /* 150k independent 64-bit-reference sampling comparisons using RGBA
     * and the CRY path, including bilinear extremes and negative shading. */
    for(unsigned n=0;n<150000u;n++){
        uint32_t t[4];
        unsigned enc=n&1u?AVP_WORLD_IMAGE_CRY16_ALPHA8:AVP_WORLD_IMAGE_RGB8_ALPHA8;
        unsigned wx=(n&3u)==0u?0u:(n&3u)==1u?1024u:next_rand()%1025u;
        unsigned wy=(n&3u)==0u?1024u:(n&3u)==1u?0u:next_rand()%1025u;
        int shade=(int)(next_rand()%511u)-255;
        for(unsigned i=0;i<4u;i++){
            uint32_t c=next_rand();
            t[i]=enc==AVP_WORLD_IMAGE_CRY16_ALPHA8 ?
                 ((c&65535u)|0xff0000u):((c&16777215u)|0xff000000u);
        }
        assert(avp_world_gpu_opaque_bilinear(t,2u,2u,(AvpWorldImageEncoding)enc,
            lut,0,1,0,1,wx,wy,shade,&expected));
        decoded[0]=t[0];decoded[1]=t[1];decoded[2]=t[2];decoded[3]=t[3];
        assert(expected==reference_pixel(decoded,enc,lut,wx,wy,shade));
    }
    /* Larger NPOT asset: staging is explicitly per resource, not a 74-art
     * atlas. Exercise both sizes and the memory-limit rejection path. */
    for(unsigned i=0;i<2u;i++){
        size_t pixels=(size_t)sizes[i]*sizes[i];
        uint8_t *tmp=malloc(pixels*3u);
        AvpGpuRgbImage hi={(int)sizes[i],(int)sizes[i],tmp,NULL,NULL};
        assert(tmp);
        memset(tmp,0x76,pixels*3u);
        assert(!avp_world_gpu_stage_image(&s,&hi,pixels*4u-1u));
        assert(avp_world_gpu_stage_image(&s,&hi,pixels*4u));
        assert(s.byte_count==pixels*4u && s.words[pixels-1u]==0xff767676u);
        free(tmp);
    }
    avp_world_gpu_staging_dispose(&s);
    avp_world_gpu_staging_dispose(&s);
    free(lut);
    puts("R324 Vulkan texture transfer: 65,536 CRY values, 150,000 pixel references, 128/1254 sizes PASS");
    return 0;
}
