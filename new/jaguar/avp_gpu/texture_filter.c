#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/cry.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

/* This registry belongs to the host video instance; local-MP viewport copies
 * borrow it. The original Jaguar decoded images are never modified. */
struct AvpFilterCache {
    AvpFilterImage slot[74+18];
    const AvpGpuRgbImage *candidate[74+18];
    /* R386: immutable open-addressed lookup, rebuilt before worker dispatch.
     * Slots hold index+1; zero means empty. Duplicate source identities keep
     * the first candidate exactly as the original linear scan did. */
    uint16_t by_source[256];
    const unsigned char *qconv;
    size_t qconv_size;
};
void avp_filter_image_destroy(AvpFilterImage *im)
{
    if(!im)return;
    for(unsigned l=1;l<AVP_FILTER_MIP_MAX;l++){
        free(im->mip[l].rgba);free(im->mip[l].cry);
    }
    memset(im,0,sizeof(*im));
}
AvpFilterCache *avp_filter_cache_create(void){return (AvpFilterCache*)calloc(1,sizeof(AvpFilterCache));}
void avp_filter_cache_clear(AvpFilterCache *cache)
{
    if(!cache)return;
    for(unsigned i=0;i<74u+18u;i++){
        avp_filter_image_destroy(&cache->slot[i]);
        cache->candidate[i]=NULL;
    }
    memset(cache->by_source,0,sizeof(cache->by_source));
    cache->qconv=NULL;cache->qconv_size=0;
}
void avp_filter_cache_destroy(AvpFilterCache *cache)
{
    if(!cache)return;
    avp_filter_cache_clear(cache);
    free(cache);
}
static int avp_filter_image_build_core(AvpFilterImage *im,const AvpGpuRgbImage *s,
                           const unsigned char *qconv,size_t qconv_size,int reuse)
{
    if(!im)return 0;
    /* R390: the filtered-AAF viewport scratch may keep its mip allocations.
     * Recompute EVERY mip texel from the current source each call, but do not
     * allocate new storage for unchanged dimensions. Other users retain the
     * old destroy-and-build entry point. */
    if(!reuse || !s || !s->rgb || im->source_width!=s->width ||
       im->source_height!=s->height)
        avp_filter_image_destroy(im);
    if(!s||!s->rgb||s->width<1||s->height<1||s->width>4096||s->height>4096)return 0;
    im->source=s;im->source_rgb=s->rgb;im->source_width=s->width;im->source_height=s->height;
    im->levels=1u;im->mip[0].width=(unsigned)s->width;im->mip[0].height=(unsigned)s->height;
    unsigned w=(unsigned)s->width,h=(unsigned)s->height;
    while((w>1u||h>1u) && im->levels<AVP_FILTER_MIP_MAX){
        unsigned nw=(w+1u)/2u,nh=(h+1u)/2u;
        size_t count=(size_t)nw*nh;
        AvpFilterMip *dst=&im->mip[im->levels];
        dst->width=nw;dst->height=nh;
        if(count>SIZE_MAX/4u)break;
        if(!dst->rgba)dst->rgba=(uint8_t*)malloc(count*4u);
        if(!dst->rgba)break;
        if(qconv && qconv_size>=131072u){
            if(!dst->cry)dst->cry=(uint16_t*)malloc(count*sizeof(uint16_t));
        }else{
            free(dst->cry);
            dst->cry=NULL;
        }
        /* Source is either the original image or the already premultiplied
         * previous mip. Coverage is averaged BEFORE colour is unmatted. */
        for(unsigned y=0;y<nh;y++)for(unsigned x=0;x<nw;x++){
            unsigned sums[4]={0,0,0,0},n=0u;
            for(unsigned dy=0;dy<2u;dy++)for(unsigned dx=0;dx<2u;dx++){
                unsigned sx=2u*x+dx,sy=2u*y+dy;
                unsigned a,r,g,b;
                if(sx>=w||sy>=h)continue;
                if(im->levels==1u){
                    size_t o=(size_t)sy*w+sx;
                    a=s->alpha?s->alpha[o]:255u;
                    r=(unsigned)s->rgb[o*3u]*a/255u;
                    g=(unsigned)s->rgb[o*3u+1u]*a/255u;
                    b=(unsigned)s->rgb[o*3u+2u]*a/255u;
                }else{
                    const uint8_t *p=im->mip[im->levels-1u].rgba+((size_t)sy*w+sx)*4u;
                    r=p[0];g=p[1];b=p[2];a=p[3];
                }
                sums[0]+=r;sums[1]+=g;sums[2]+=b;sums[3]+=a;++n;
            }
            uint8_t *p=dst->rgba+((size_t)y*nw+x)*4u;
            for(unsigned c=0;c<4;c++)p[c]=(uint8_t)((sums[c]+n/2u)/n);
            if(dst->cry){
                unsigned a=p[3],r=a?(unsigned)p[0]*255u/a:0u,
                         g=a?(unsigned)p[1]*255u/a:0u,b=a?(unsigned)p[2]*255u/a:0u;
                dst->cry[(size_t)y*nw+x]=avp_jaguar_rgb_to_cry_qconv(
                    (uint8_t)(r>255u?255u:r),(uint8_t)(g>255u?255u:g),
                    (uint8_t)(b>255u?255u:b),qconv,qconv_size);
            }
        }
        im->levels++;w=nw;h=nh;
    }
    return im->levels>0;
}
int avp_filter_image_build(AvpFilterImage *im,const AvpGpuRgbImage *s,
                           const unsigned char *qconv,size_t qconv_size)
{
    return avp_filter_image_build_core(im,s,qconv,qconv_size,0);
}
int avp_filter_image_rebuild(AvpFilterImage *im,const AvpGpuRgbImage *s,
                           const unsigned char *qconv,size_t qconv_size)
{
    return avp_filter_image_build_core(im,s,qconv,qconv_size,1);
}
/* Identity is RGB buffer + dimensions (not the image struct pointer):
 * that is the original R308 matching rule, including duplicate sources. */
static unsigned avp_filter_source_bucket(const AvpGpuRgbImage *source)
{
    size_t h=((size_t)(uintptr_t)source->rgb>>4u) ^
             ((size_t)(uintptr_t)source->rgb>>13u);
    h^=(size_t)(unsigned)source->width*(size_t)0x9e3779b1u;
    h^=(size_t)(unsigned)source->height*(size_t)0x85ebca6bu;
    h^=h>>11u;
    return (unsigned)(h&255u);
}
static int avp_filter_source_equal(const AvpGpuRgbImage *a,
                                    const AvpGpuRgbImage *b)
{
    return a && b && a->rgb==b->rgb && a->width==b->width &&
           a->height==b->height;
}
static int avp_filter_cache_find(const AvpFilterCache *cache,
                                  const AvpGpuRgbImage *source)
{
    unsigned bucket=avp_filter_source_bucket(source);
    for(unsigned probe=0;probe<256u;probe++,bucket=(bucket+1u)&255u){
        const unsigned entry=cache->by_source[bucket];
        if(!entry)return -1;
        if(avp_filter_source_equal(cache->candidate[entry-1u],source))
            return (int)(entry-1u);
    }
    return -1;
}
static void avp_filter_cache_index(AvpFilterCache *cache)
{
    memset(cache->by_source,0,sizeof(cache->by_source));
    for(unsigned i=0;i<74u+18u;i++){
        const AvpGpuRgbImage *source=cache->candidate[i];
        if(!source||!source->rgb)continue;
        unsigned bucket=avp_filter_source_bucket(source);
        for(unsigned probe=0;probe<256u;probe++,bucket=(bucket+1u)&255u){
            unsigned existing=cache->by_source[bucket];
            if(!existing){cache->by_source[bucket]=(uint16_t)(i+1u);break;}
            if(avp_filter_source_equal(cache->candidate[existing-1u],source))
                break; /* Preserve the lowest index for duplicate data. */
        }
    }
}
void avp_filter_cache_prepare(AvpFilterCache *cache,
                             const AvpGpuRgbImage *world,const AvpGpuRgbImage *notice,
                             const AvpGpuRgbImage *original,const AvpGpuRgbImage *original_notice,
                             int use_hires,const unsigned char *qconv,size_t qconv_size)
{
    if(!cache||!original||!original_notice)return;
    cache->qconv=qconv;cache->qconv_size=qconv_size;
    for(unsigned i=0;i<74u+18u;i++){
        const AvpGpuRgbImage *src;
        if(i<74u){src=original+i;
            if(use_hires&&world&&world[i].rgb)src=world+i;
        }else{
            unsigned ni=i-74u;src=original_notice+ni;
            if(use_hires&&notice&&notice[ni].rgb)src=notice+ni;
        }
        AvpFilterImage *slot=&cache->slot[i];
        cache->candidate[i]=src;
        if(slot->source_rgb==src->rgb &&
           slot->source_width==src->width && slot->source_height==src->height)continue;
        /* An active replacement changed: discard its old mip levels, but do
         * not rebuild anything until this surface is actually drawn. */
        avp_filter_image_destroy(slot);
    }
    avp_filter_cache_index(cache);
}
const AvpFilterImage *avp_filter_cache_lookup(AvpFilterCache *cache,
                                               const AvpGpuRgbImage *source)
{
    if(!cache||!source||!source->rgb)return NULL;
    const int index=avp_filter_cache_find(cache,source);
    if(index<0)return NULL;
    AvpFilterImage *s=&cache->slot[(unsigned)index];
    if(s->source_rgb!=source->rgb || s->source_width!=source->width ||
       s->source_height!=source->height){
        /* Only the owner thread builds mipmaps. The read-only lookup below
         * never mutates this shared cache from parallel wall workers. */
        if(!avp_filter_image_build(s,source,cache->qconv,cache->qconv_size))
            return NULL;
    }
    return s;
}
const AvpFilterImage *avp_filter_cache_lookup_ready(const AvpFilterCache *cache,
                                                     const AvpGpuRgbImage *source)
{
    if(!cache||!source||!source->rgb)return NULL;
    const int index=avp_filter_cache_find(cache,source);
    if(index<0)return NULL;
    const AvpFilterImage *s=&cache->slot[(unsigned)index];
    return s->source_rgb==source->rgb && s->source_width==source->width &&
           s->source_height==source->height && s->levels>0u ? s:NULL;
}
static inline uint32_t packrgb(unsigned r,unsigned g,unsigned b){return (r<<16u)|(g<<8u)|b;}
static unsigned coord(int k,unsigned extent,int wrap)
{
    if(wrap){
        int n=(int)extent;
        if(k>=0&&k<n)return (unsigned)k;
        /* Most retail panels and mip levels have power-of-two dimensions.
         * Exact two's-complement modulo avoids a hardware integer divide
         * for AF taps crossing a wrapped texture boundary. */
        if((extent&(extent-1u))==0u)return (unsigned)k&(extent-1u);
        k%=n;if(k<0)k+=n;return (unsigned)k;
    }
    return (unsigned)(k<0?0:k>=(int)extent?(int)extent-1:k);
}
static void fetch(const AvpGpuRgbImage *t,const AvpFilterImage *chain,
                  unsigned level,int x,int y,int wrap,int shade,const uint32_t *cry_rgb,
                  unsigned *r,unsigned *g,unsigned *b,unsigned *a)
{
    unsigned w=level?chain->mip[level].width:(unsigned)t->width;
    unsigned h=level?chain->mip[level].height:(unsigned)t->height;
    size_t at=(size_t)coord(y,h,wrap)*w+coord(x,w,wrap);
    uint16_t cry=0;
    if(!level){
        *a=t->alpha?t->alpha[at]:255u;
        if(t->cry16)cry=t->cry16[at];
        else{
            const uint8_t *p=t->rgb+at*3u;*r=p[0];*g=p[1];*b=p[2];
        }
    }else{
        const AvpFilterMip *m=&chain->mip[level];
        const uint8_t *p=m->rgba+at*4u;
        *a=p[3];
        if(m->cry)cry=m->cry[at];
        else {
            *r=p[3]?(unsigned)p[0]*255u/p[3]:0u;
            *g=p[3]?(unsigned)p[1]*255u/p[3]:0u;
            *b=p[3]?(unsigned)p[2]*255u/p[3]:0u;
        }
    }
    if((!level&&t->cry16)||(level&&chain->mip[level].cry)){
        int v=(int)(cry&255u)+shade;
        if(v<0)v=0;
        if(v>255)v=255;
        const uint32_t c=cry_rgb[(cry&0xff00u)|(uint16_t)v];
        *r=(c>>16u)&255u;*g=(c>>8u)&255u;*b=c&255u;
    }else if(shade<0){
        const unsigned z=(unsigned)(shade< -255?0:255+shade);
        *r=*r*z/255u;*g=*g*z/255u;*b=*b*z/255u;
    }
}
/* Hot path for fully opaque world panels: four source texels, integer
 * bilinear weights, one alpha result.  R308 used four generic alpha-aware
 * fetches with floating-point colour/coverage division even when all four
 * pixels were opaque.  Native point filtering never uses this function. */
static inline uint32_t opaque_texel(const AvpGpuRgbImage *t,
                      const AvpFilterImage *chain,unsigned level,size_t at,
                      int shade,const uint32_t *cry_rgb)
{
    uint16_t cry=0;
    if(level){
        const AvpFilterMip *m=&chain->mip[level];
        if(m->cry)cry=m->cry[at];
        else{
            const uint8_t *src=m->rgba+at*4u;
            unsigned r=src[0],g=src[1],b=src[2];
            if(shade<0){unsigned k=(unsigned)(shade< -255?0:255+shade);
                r=r*k/255u;g=g*k/255u;b=b*k/255u;}
            return packrgb(r,g,b);
        }
    }else if(t->cry16)cry=t->cry16[at];
    else{
        const uint8_t *src=t->rgb+at*3u;
        unsigned r=src[0],g=src[1],b=src[2];
        if(shade<0){unsigned k=(unsigned)(shade< -255?0:255+shade);
            r=r*k/255u;g=g*k/255u;b=b*k/255u;}
        return packrgb(r,g,b);
    }
    int value=(int)(cry&255u)+shade;
    if(value<0)value=0;
    if(value>255)value=255;
    return cry_rgb[(cry&0xff00u)|(uint16_t)value];
}
/* R312 fast path, strictly limited to opaque CRY16 source/mips with
 * power-of-two wrapping. These are the hot Jaguar floor/ceiling textures.
 * The sampler is specialized only after source format, dimensions and mip
 * level have been selected once for the entire pixel; each AF tap avoids
 * repeated cache/format/wrap-mode checks. All original 4 CRY texels and
 * 1024-step integer bilinear weights are preserved exactly. */
static inline uint32_t sample_opaque_cry_pow2(const uint16_t *cry,
                    unsigned width,unsigned height,double u,double v,
                    const uint8_t shade_lut[256],const uint32_t *cry_rgb)
{
    const double fx=u*(double)width-0.5,fy=v*(double)height-0.5;
    const int x=(int)floor(fx),y=(int)floor(fy);
    unsigned ax=(unsigned)((fx-(double)x)*1024.0+0.5);
    unsigned ay=(unsigned)((fy-(double)y)*1024.0+0.5);
    if(ax>1024u)ax=1024u;
    if(ay>1024u)ay=1024u;
    const unsigned xm=width-1u,ym=height-1u;
    const unsigned x0=(unsigned)x&xm,x1=(unsigned)(x+1)&xm;
    const unsigned y0=(unsigned)y&ym,y1=(unsigned)(y+1)&ym;
    const size_t row0=(size_t)y0*width,row1=(size_t)y1*width;
    const uint16_t t00=cry[row0+x0],t10=cry[row0+x1];
    const uint16_t t01=cry[row1+x0],t11=cry[row1+x1];
    const uint32_t c00=cry_rgb[(t00&0xff00u)|shade_lut[t00&255u]];
    const uint32_t c10=cry_rgb[(t10&0xff00u)|shade_lut[t10&255u]];
    const uint32_t c01=cry_rgb[(t01&0xff00u)|shade_lut[t01&255u]];
    const uint32_t c11=cry_rgb[(t11&0xff00u)|shade_lut[t11&255u]];
    const unsigned w00=(1024u-ax)*(1024u-ay),w10=ax*(1024u-ay);
    const unsigned w01=(1024u-ax)*ay,w11=ax*ay;
    const unsigned r=((c00>>16u)*w00+(c10>>16u)*w10+
                      (c01>>16u)*w01+(c11>>16u)*w11+524288u)>>20u;
    const unsigned g=(((c00>>8u)&255u)*w00+((c10>>8u)&255u)*w10+
                      ((c01>>8u)&255u)*w01+((c11>>8u)&255u)*w11+524288u)>>20u;
    const unsigned b=((c00&255u)*w00+(c10&255u)*w10+
                      (c01&255u)*w01+(c11&255u)*w11+524288u)>>20u;
    return packrgb(r,g,b);
}
/* R314: identical CRY16 math for non-power-of-two replacements (notably
 * the 1254x1254 optional pack). R312's specialized floor path was limited
 * to power-of-two coordinates; this retains the original coord() wrapping,
 * index clamping and bilinear weights from sample_opaque_bilinear_lut(). */
static inline uint32_t sample_opaque_cry_npot(const uint16_t *cry,
                    unsigned width,unsigned height,double u,double v,
                    const uint8_t shade_lut[256],const uint32_t *cry_rgb)
{
    const double fx=u*(double)width-0.5,fy=v*(double)height-0.5;
    const int x=(int)floor(fx),y=(int)floor(fy);
    unsigned ax=(unsigned)((fx-(double)x)*1024.0+0.5);
    unsigned ay=(unsigned)((fy-(double)y)*1024.0+0.5);
    if(ax>1024u)ax=1024u;
    if(ay>1024u)ay=1024u;
    /* R317: wrapping by one texel is exact after wrapping the first
     * coordinate. Avoid two more variable-modulus coord() calls for every
     * bilinear AF tap on non-power-of-two hi-res replacement textures.
     * x0/y0 are always in [0,extent), so the second coordinate is either
     * its successor or zero at the seam. Source texels and weights remain
     * identical; the native Jaguar point renderer is not affected. */
    const unsigned x0=coord(x,width,1),y0=coord(y,height,1);
    const unsigned x1=x0+1u==width?0u:x0+1u;
    const unsigned y1=y0+1u==height?0u:y0+1u;
    const size_t row0=(size_t)y0*width,row1=(size_t)y1*width;
    const uint16_t t00=cry[row0+x0],t10=cry[row0+x1];
    const uint16_t t01=cry[row1+x0],t11=cry[row1+x1];
    const uint32_t c00=cry_rgb[(t00&0xff00u)|shade_lut[t00&255u]];
    const uint32_t c10=cry_rgb[(t10&0xff00u)|shade_lut[t10&255u]];
    const uint32_t c01=cry_rgb[(t01&0xff00u)|shade_lut[t01&255u]];
    const uint32_t c11=cry_rgb[(t11&0xff00u)|shade_lut[t11&255u]];
    const unsigned w00=(1024u-ax)*(1024u-ay),w10=ax*(1024u-ay);
    const unsigned w01=(1024u-ax)*ay,w11=ax*ay;
    const unsigned r=((c00>>16u)*w00+(c10>>16u)*w10+
                      (c01>>16u)*w01+(c11>>16u)*w11+524288u)>>20u;
    const unsigned g=(((c00>>8u)&255u)*w00+((c10>>8u)&255u)*w10+
                      ((c01>>8u)&255u)*w01+((c11>>8u)&255u)*w11+524288u)>>20u;
    const unsigned b=((c00&255u)*w00+(c10&255u)*w10+
                      (c01&255u)*w01+(c11&255u)*w11+524288u)>>20u;
    return packrgb(r,g,b);
}
/* R312 wall counterpart. Projected wall columns clamp instead of wrap,
 * so source addressing is specialized without performing four full generic
 * texel lookups per tap. The same 4 CRY source words, row shades, and weight
 * arithmetic from the R311 bilinear sampler are used in the same order. */
static inline uint32_t sample_opaque_cry_clamp(const uint16_t *cry,
                    unsigned width,unsigned height,double u,double v,
                    const uint8_t shade_lut[256],const uint32_t *cry_rgb)
{
    const double fx=u*(double)width-0.5,fy=v*(double)height-0.5;
    const int x=(int)floor(fx),y=(int)floor(fy);
    unsigned ax=(unsigned)((fx-(double)x)*1024.0+0.5);
    unsigned ay=(unsigned)((fy-(double)y)*1024.0+0.5);
    if(ax>1024u)ax=1024u;
    if(ay>1024u)ay=1024u;
    const unsigned x0=(unsigned)(x<0?0:x>=(int)width?(int)width-1:x);
    const unsigned x1=(unsigned)(x+1<0?0:x+1>=(int)width?(int)width-1:x+1);
    const unsigned y0=(unsigned)(y<0?0:y>=(int)height?(int)height-1:y);
    const unsigned y1=(unsigned)(y+1<0?0:y+1>=(int)height?(int)height-1:y+1);
    const size_t row0=(size_t)y0*width,row1=(size_t)y1*width;
    const uint16_t t00=cry[row0+x0],t10=cry[row0+x1];
    const uint16_t t01=cry[row1+x0],t11=cry[row1+x1];
    const uint32_t c00=cry_rgb[(t00&0xff00u)|shade_lut[t00&255u]];
    const uint32_t c10=cry_rgb[(t10&0xff00u)|shade_lut[t10&255u]];
    const uint32_t c01=cry_rgb[(t01&0xff00u)|shade_lut[t01&255u]];
    const uint32_t c11=cry_rgb[(t11&0xff00u)|shade_lut[t11&255u]];
    const unsigned w00=(1024u-ax)*(1024u-ay),w10=ax*(1024u-ay);
    const unsigned w01=(1024u-ax)*ay,w11=ax*ay;
    const unsigned r=((c00>>16u)*w00+(c10>>16u)*w10+
                      (c01>>16u)*w01+(c11>>16u)*w11+524288u)>>20u;
    const unsigned g=(((c00>>8u)&255u)*w00+((c10>>8u)&255u)*w10+
                      ((c01>>8u)&255u)*w01+((c11>>8u)&255u)*w11+524288u)>>20u;
    const unsigned b=((c00&255u)*w00+(c10&255u)*w10+
                      (c01&255u)*w01+(c11&255u)*w11+524288u)>>20u;
    return packrgb(r,g,b);
}
static inline uint32_t sample_opaque_bilinear_lut(const AvpGpuRgbImage *t,
                             const AvpFilterImage *chain,unsigned level,
                             double u,double v,int wrap,int shade,
                             const uint32_t *cry_rgb,const uint8_t *shade_lut)
{
    const unsigned w=level?chain->mip[level].width:(unsigned)t->width;
    const unsigned h=level?chain->mip[level].height:(unsigned)t->height;
    const double fx=u*w-0.5,fy=v*h-0.5;
    const int x=(int)floor(fx),y=(int)floor(fy);
    unsigned ax=(unsigned)((fx-(double)x)*1024.0+0.5);
    unsigned ay=(unsigned)((fy-(double)y)*1024.0+0.5);
    if(ax>1024u)ax=1024u;
    if(ay>1024u)ay=1024u;
    const unsigned x0=coord(x,w,wrap),x1=coord(x+1,w,wrap);
    const unsigned y0=coord(y,h,wrap),y1=coord(y+1,h,wrap);
    const size_t pos00=(size_t)y0*w+x0,pos10=(size_t)y0*w+x1;
    const size_t pos01=(size_t)y1*w+x0,pos11=(size_t)y1*w+x1;
    uint32_t c00,c10,c01,c11;
    const uint16_t *cry=level?chain->mip[level].cry:t->cry16;
    if(cry){
        /* The Jaguar source is overwhelmingly CRY16. Resolve the source
         * format once per bilinear sample, rather than four times for the
         * four texels. This uses the unchanged CRY lookup and fog clamp. */
        uint16_t w00=cry[pos00],w10=cry[pos10],w01=cry[pos01],w11=cry[pos11];
        unsigned v00,v10,v01,v11;
        if(shade_lut){
            /* One 256-byte host-only shade table per flat row. This removes
             * four fog clamp calculations for every bilinear AF tap, while
             * looking up the identical source CRY16 RGB value. */
            v00=shade_lut[w00&255u];v10=shade_lut[w10&255u];
            v01=shade_lut[w01&255u];v11=shade_lut[w11&255u];
        }else{
            int a=(int)(w00&255u)+shade,b=(int)(w10&255u)+shade;
            int c=(int)(w01&255u)+shade,d=(int)(w11&255u)+shade;
            v00=(unsigned)(a<0?0:a>255?255:a);
            v10=(unsigned)(b<0?0:b>255?255:b);
            v01=(unsigned)(c<0?0:c>255?255:c);
            v11=(unsigned)(d<0?0:d>255?255:d);
        }
        c00=cry_rgb[(w00&0xff00u)|v00];
        c10=cry_rgb[(w10&0xff00u)|v10];
        c01=cry_rgb[(w01&0xff00u)|v01];
        c11=cry_rgb[(w11&0xff00u)|v11];
    }else{
        c00=opaque_texel(t,chain,level,pos00,shade,cry_rgb);
        c10=opaque_texel(t,chain,level,pos10,shade,cry_rgb);
        c01=opaque_texel(t,chain,level,pos01,shade,cry_rgb);
        c11=opaque_texel(t,chain,level,pos11,shade,cry_rgb);
    }
    const unsigned w00=(1024u-ax)*(1024u-ay),w10=ax*(1024u-ay);
    const unsigned w01=(1024u-ax)*ay,w11=ax*ay;
    const unsigned red=((c00>>16u)*w00+(c10>>16u)*w10+
                       (c01>>16u)*w01+(c11>>16u)*w11+524288u)>>20u;
    const unsigned green=(((c00>>8u)&255u)*w00+((c10>>8u)&255u)*w10+
                          ((c01>>8u)&255u)*w01+((c11>>8u)&255u)*w11+524288u)>>20u;
    const unsigned blue=((c00&255u)*w00+(c10&255u)*w10+
                         (c01&255u)*w01+(c11&255u)*w11+524288u)>>20u;
    return packrgb(red,green,blue);
}
static inline uint32_t sample_opaque_bilinear(const AvpGpuRgbImage *t,
                             const AvpFilterImage *chain,unsigned level,
                             double u,double v,int wrap,int shade,
                             const uint32_t *cry_rgb)
{
    return sample_opaque_bilinear_lut(t,chain,level,u,v,wrap,shade,cry_rgb,NULL);
}
static uint32_t sample_level(const AvpGpuRgbImage *t,const AvpFilterImage *chain,
                              unsigned level,double u,double v,int wrap,int shade,
                              unsigned bilinear,const uint32_t *cry_rgb,unsigned *alpha)
{
    unsigned w=level?chain->mip[level].width:(unsigned)t->width;
    unsigned h=level?chain->mip[level].height:(unsigned)t->height;
    if(bilinear && !t->alpha){
        *alpha=255u;
        return sample_opaque_bilinear(t,chain,level,u,v,wrap,shade,cry_rgb);
    }
    if(!bilinear){
        const int x=(int)floor(u*w),y=(int)floor(v*h);
        if(!t->alpha){
            size_t at=(size_t)coord(y,h,wrap)*w+coord(x,w,wrap);
            *alpha=255u;
            return opaque_texel(t,chain,level,at,shade,cry_rgb);
        }
        unsigned r=0,g=0,b=0,a=255;
        fetch(t,chain,level,x,y,wrap,shade,cry_rgb,&r,&g,&b,&a);
        *alpha=a;return packrgb(r,g,b);
    }
    double fx=u*w-0.5,fy=v*h-0.5;
    int x=(int)floor(fx),y=(int)floor(fy);
    double ax=fx-x,ay=fy-y;
    double sum_a=0,sum_r=0,sum_g=0,sum_b=0;
    for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
        unsigned r=0,g=0,b=0,a=255;
        double wt=(dx?ax:1.0-ax)*(dy?ay:1.0-ay);
        fetch(t,chain,level,x+dx,y+dy,wrap,shade,cry_rgb,&r,&g,&b,&a);
        sum_a+=wt*a;sum_r+=wt*a*r;sum_g+=wt*a*g;sum_b+=wt*a*b;
    }
    *alpha=(unsigned)(sum_a+0.5);
    if(sum_a<0.0001)return 0u;
    return packrgb((unsigned)(sum_r/sum_a+0.5),(unsigned)(sum_g/sum_a+0.5),
                   (unsigned)(sum_b/sum_a+0.5));
}
AvpFilterPlan avp_filter_plan(const AvpGpuRgbImage *s,const AvpFilterImage *mips,
                              AvpFilterDerivatives dd,unsigned mode,unsigned aniso)
{
    AvpFilterPlan p={0u,1u,mode==AVP_TEXFILTER_BILINEAR,0.0,0.0};
    if(!s||s->width<=0||s->height<=0)return p;
    /* Most important fast path: bilinear filtering without AF does not
     * select a mip and needs NONE of the sqrt/log/derivative calculations.
     * The same is true for ordinary nearest and for missing mip chains. */
    if(aniso<=1u && (mode!=AVP_TEXFILTER_NEAREST_MIP ||
                    !mips || mips->levels<=1u))return p;
    if(aniso<=1u && mode==AVP_TEXFILTER_NEAREST_MIP){
        /* The original LOD rounds log2(max(hypot(dx),hypot(dy))) to
         * the nearest integer. Compare SQUARED footprint sizes against
         * 2^(2*k-1) instead: exact mip thresholds with no per-plan
         * sqrt, hypot or log2. This is the hot floor/ceiling mip mode. */
        const double ax=dd.dudx*s->width,ay=dd.dvdx*s->height;
        const double bx=dd.dudy*s->width,by=dd.dvdy*s->height;
        const double da=ax*ax+ay*ay,db=bx*bx+by*by;
        const double major2=da>db?da:db;
        if(!isfinite(major2))return p;
        double threshold=2.0;
        for(unsigned level=1u;level<mips->levels;level++){
            if(major2<threshold)break;
            p.level=level;
            threshold*=4.0;
        }
        return p;
    }
    /* R311: Ordinary AF plans do not need libm hypot() twice and log2().
     * Work in squared texel-footprint space, which preserves the same LOD
     * thresholds as the original round(log2) selector. For exceptionally
     * large/small derivatives and values within roundoff of a tap/LOD
     * boundary, keep the original hypot/log2 route below. In particular,
     * neither the number of AF taps nor the chosen source pixels change. */
    if(aniso>1u){
        const double ax=dd.dudx*s->width,ay=dd.dvdx*s->height;
        const double bx=dd.dudy*s->width,by=dd.dvdy*s->height;
        const double a2=ax*ax+ay*ay,b2=bx*bx+by*by;
        if(isfinite(a2)&&isfinite(b2)&&
           a2>=1.0e-260&&b2>=1.0e-260&&
           a2<=1.0e260&&b2<=1.0e260){
            const double a=sqrt(a2),b=sqrt(b2);
            const double major=a>b?a:b,minor=a<b?a:b;
            int use_legacy=0;
            if(fabs(major-(minor+0.0001))<=
               1.0e-10*fmax(1.0,major))use_legacy=1;
            if(!use_legacy && major>minor+0.0001 && major>1.0){
                double ratio=major/(minor>0.001?minor:0.001);
                if(isfinite(ratio)){
                    const double nearest=floor(ratio+0.5);
                    if(fabs(ratio-nearest)<=1.0e-10*fmax(1.0,ratio))
                        use_legacy=1;
                    else{
                        p.taps=ratio>=(double)aniso?aniso:(unsigned)ceil(ratio);
                        if(p.taps<1u)p.taps=1u;
                    }
                }else use_legacy=1;
            }
            if(!use_legacy && mips&&mips->levels>1u &&
               (mode==AVP_TEXFILTER_NEAREST_MIP||p.taps>1u)){
                const double footprint2=p.taps>1u?(a2<b2?a2:b2):
                                                   (a2>b2?a2:b2);
                if(footprint2>1.0){
                    double threshold=2.0;
                    for(unsigned level=1u;level<mips->levels;level++){
                        if(fabs(footprint2-threshold)<=1.0e-10*threshold){
                            use_legacy=1;break;
                        }
                        if(footprint2<threshold)break;
                        p.level=level;
                        threshold*=4.0;
                    }
                }
            }
            if(!use_legacy){
                if(p.taps>1u){
                    p.du=a>b?dd.dudx:dd.dudy;
                    p.dv=a>b?dd.dvdx:dd.dvdy;
                }
                return p;
            }
            /* Fall through on exact arithmetic boundaries, preserving the
             * R309/R310 values rather than approximating mip/AF quality. */
            p.level=0u;p.taps=1u;
        }
    }
    double a=hypot(dd.dudx*s->width,dd.dvdx*s->height);
    double b=hypot(dd.dudy*s->width,dd.dvdy*s->height);
    double major=a>b?a:b,minor=a<b?a:b;
    if(!isfinite(major)||!isfinite(minor)){major=1.0;minor=1.0;}
    if(aniso>1u && major>minor+0.0001 && major>1.0){
        double ratio=major/(minor>0.001?minor:0.001);
        /* Avoid casting out-of-range double to unsigned on unusual maps. */
        p.taps=ratio>=(double)aniso?aniso:(unsigned)ceil(ratio);
        if(p.taps<1u)p.taps=1u;
    }
    if(mips&&mips->levels>1u &&
       (mode==AVP_TEXFILTER_NEAREST_MIP||p.taps>1u)){
        double lod=p.taps>1u?minor:major;
        if(lod>1.0){
            int ilod=(int)floor(log2(lod)+0.5);
            if(ilod>0)p.level=(unsigned)ilod>=mips->levels?mips->levels-1u:(unsigned)ilod;
        }
    }
    if(p.taps>1u){
        p.du=a>b?dd.dudx:dd.dudy;
        p.dv=a>b?dd.dvdx:dd.dvdy;
    }
    return p;
}
/* R310: immutable anisotropic sampling positions. R309 recalculated an
 * expensive floating-point division separately for every tap at every
 * world pixel. The same 2..16 positions are used by all filtered surfaces;
 * retain every tap at the user-selected quality level. */
static const double avp_filter_tap_offsets[17][16]={
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, /* 0 */
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, /* 1 */
    {-0.25,0.25,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, /* 2 */
    {-0.33333333333333337,0,0.33333333333333337,0,0,0,0,0,0,0,0,0,0,0,0,0}, /* 3 */
    {-0.375,-0.125,0.125,0.375,0,0,0,0,0,0,0,0,0,0,0,0}, /* 4 */
    {-0.40000000000000002,-0.20000000000000001,0,0.19999999999999996,0.40000000000000002,0,0,0,0,0,0,0,0,0,0,0}, /* 5 */
    {-0.41666666666666669,-0.25,-0.083333333333333315,0.08333333333333337,0.25,0.41666666666666663,0,0,0,0,0,0,0,0,0,0}, /* 6 */
    {-0.4285714285714286,-0.2857142857142857,-0.14285714285714285,0,0.1428571428571429,0.2857142857142857,0.4285714285714286,0,0,0,0,0,0,0,0,0}, /* 7 */
    {-0.4375,-0.3125,-0.1875,-0.0625,0.0625,0.1875,0.3125,0.4375,0,0,0,0,0,0,0,0}, /* 8 */
    {-0.44444444444444442,-0.33333333333333337,-0.22222222222222221,-0.1111111111111111,0,0.11111111111111116,0.22222222222222221,0.33333333333333337,0.44444444444444442,0,0,0,0,0,0,0}, /* 9 */
    {-0.45000000000000001,-0.34999999999999998,-0.25,-0.15000000000000002,-0.049999999999999989,0.050000000000000044,0.15000000000000002,0.25,0.34999999999999998,0.44999999999999996,0,0,0,0,0,0}, /* 10 */
    {-0.45454545454545453,-0.36363636363636365,-0.27272727272727271,-0.18181818181818182,-0.090909090909090884,0,0.090909090909090939,0.18181818181818177,0.27272727272727271,0.36363636363636365,0.45454545454545459,0,0,0,0,0}, /* 11 */
    {-0.45833333333333331,-0.375,-0.29166666666666663,-0.20833333333333331,-0.125,-0.041666666666666685,0.04166666666666663,0.125,0.20833333333333337,0.29166666666666663,0.375,0.45833333333333337,0,0,0,0}, /* 12 */
    {-0.46153846153846156,-0.38461538461538458,-0.30769230769230771,-0.23076923076923078,-0.15384615384615385,-0.076923076923076927,0,0.076923076923076872,0.15384615384615385,0.23076923076923073,0.30769230769230771,0.38461538461538458,0.46153846153846156,0,0,0}, /* 13 */
    {-0.4642857142857143,-0.39285714285714285,-0.3214285714285714,-0.25,-0.17857142857142855,-0.10714285714285715,-0.035714285714285698,0.035714285714285698,0.1071428571428571,0.1785714285714286,0.25,0.3214285714285714,0.3928571428571429,0.4642857142857143,0,0}, /* 14 */
    {-0.46666666666666667,-0.40000000000000002,-0.33333333333333337,-0.26666666666666666,-0.20000000000000001,-0.13333333333333336,-0.066666666666666652,0,0.066666666666666652,0.1333333333333333,0.19999999999999996,0.26666666666666672,0.33333333333333337,0.40000000000000002,0.46666666666666667,0}, /* 15 */
    {-0.46875,-0.40625,-0.34375,-0.28125,-0.21875,-0.15625,-0.09375,-0.03125,0.03125,0.09375,0.15625,0.21875,0.28125,0.34375,0.40625,0.46875}, /* 16 */
};
uint32_t avp_filter_sample_prepared(const AvpGpuRgbImage *s,const AvpFilterImage *mips,
                                   double u,double v,AvpFilterPlan p,int wrap,int shade,
                                   const uint32_t *cry_rgb,unsigned *out_alpha)
{
    if(out_alpha)*out_alpha=255u;
    if(!s||!s->rgb||s->width<1||s->height<1||!cry_rgb||!out_alpha)return 0;
    if(p.taps<=1u)
        return sample_level(s,mips,p.level,u,v,wrap,shade,p.bilinear,cry_rgb,out_alpha);
    const double *tap_offsets=p.taps<=16u?avp_filter_tap_offsets[p.taps]:NULL;
    /* An opaque texture stays opaque at every mip level. Avoid 16 floating-
     * point alpha multiplications/divisions per channel in the AF path. */
    if(!s->alpha){
        unsigned rr=0u,gg=0u,bb=0u;
        for(unsigned i=0;i<p.taps;i++){
            const double offs=tap_offsets?tap_offsets[i]:
                ((double)i+0.5)/(double)p.taps-0.5;
            /* Opaque bilinear needs neither alpha sampling nor the generic
             * sample_level dispatch at every individual AF tap. */
            uint32_t c;
            if(p.bilinear){
                c=sample_opaque_bilinear(s,mips,p.level,
                    u+offs*p.du,v+offs*p.dv,wrap,shade,cry_rgb);
            }else{
                unsigned al=255u;
                c=sample_level(s,mips,p.level,
                    u+offs*p.du,v+offs*p.dv,wrap,shade,0u,cry_rgb,&al);
            }
            rr+=(c>>16)&255u;gg+=(c>>8)&255u;bb+=c&255u;
        }
        *out_alpha=255u;
        return packrgb((rr+p.taps/2u)/p.taps,
                       (gg+p.taps/2u)/p.taps,(bb+p.taps/2u)/p.taps);
    }
    double rr=0,gg=0,bb=0,aa=0;
    for(unsigned i=0;i<p.taps;i++){
        double offs=tap_offsets?tap_offsets[i]:
            ((double)i+0.5)/(double)p.taps-0.5;
        unsigned al=255u;
        uint32_t c=sample_level(s,mips,p.level,u+offs*p.du,v+offs*p.dv,wrap,shade,
                                p.bilinear,cry_rgb,&al);
        aa+=al;rr+=(double)((c>>16)&255u)*al;
        gg+=(double)((c>>8)&255u)*al;bb+=(double)(c&255u)*al;
    }
    *out_alpha=(unsigned)(aa/(double)p.taps+0.5);
    if(aa<0.01)return 0u;
    return packrgb((unsigned)(rr/aa+0.5),(unsigned)(gg/aa+0.5),(unsigned)(bb/aa+0.5));
}
/* R311: On a floor/ceiling scanline, the Jaguar fog shade is fixed for
 * every output pixel. Build this tiny table once per row and reuse it for
 * actual source CRY16 texel fetches. Transparent and non-bilinear surfaces
 * still take the same exact R310 sampling path. */
void avp_filter_make_shade_lut(uint8_t lut[256],int shade)
{
    if(!lut)return;
    for(unsigned i=0;i<256u;i++){
        const int v=(int)i+shade;
        lut[i]=(uint8_t)(v<0?0:v>255?255:v);
    }
}
uint32_t avp_filter_sample_prepared_shaded(const AvpGpuRgbImage *s,
                            const AvpFilterImage *mips,double u,double v,
                            AvpFilterPlan p,int wrap,int shade,
                            const uint32_t *cry_rgb,unsigned *out_alpha,
                            const uint8_t shade_lut[256])
{
    if(!shade_lut||!s||!s->rgb||s->alpha||!p.bilinear||
       s->width<1||s->height<1||!cry_rgb||!out_alpha)
        return avp_filter_sample_prepared(s,mips,u,v,p,wrap,shade,cry_rgb,out_alpha);
    if(p.taps<=1u){
        *out_alpha=255u;
        const unsigned tw=p.level?mips->mip[p.level].width:(unsigned)s->width;
        const unsigned th=p.level?mips->mip[p.level].height:(unsigned)s->height;
        const uint16_t *cry=p.level?mips->mip[p.level].cry:s->cry16;
        if(cry && tw && th){
            if(wrap && (tw&(tw-1u))==0u && (th&(th-1u))==0u)
                return sample_opaque_cry_pow2(cry,tw,th,u,v,shade_lut,cry_rgb);
            if(!wrap)return sample_opaque_cry_clamp(cry,tw,th,u,v,shade_lut,cry_rgb);
        }
        return sample_opaque_bilinear_lut(s,mips,p.level,u,v,wrap,shade,cry_rgb,shade_lut);
    }
    const double *tap_offsets=p.taps<=16u?avp_filter_tap_offsets[p.taps]:NULL;
    unsigned rr=0,gg=0,bb=0;
    /* Per-pixel (not per-tap) source-format/mip/wrap resolution. Unlike a
     * filtered final framebuffer, this samples all original source texels. */
    const unsigned tw=p.level?mips->mip[p.level].width:(unsigned)s->width;
    const unsigned th=p.level?mips->mip[p.level].height:(unsigned)s->height;
    const uint16_t *cry=p.level?mips->mip[p.level].cry:s->cry16;
    if(wrap && cry && tw && th &&
       (tw&(tw-1u))==0u && (th&(th-1u))==0u){
        for(unsigned i=0;i<p.taps;i++){
            const double offs=tap_offsets?tap_offsets[i]:
                ((double)i+0.5)/(double)p.taps-0.5;
            const uint32_t c=sample_opaque_cry_pow2(cry,tw,th,
                u+offs*p.du,v+offs*p.dv,shade_lut,cry_rgb);
            rr+=(c>>16u)&255u;gg+=(c>>8u)&255u;bb+=c&255u;
        }
    }else if(!wrap && cry && tw && th){
        for(unsigned i=0;i<p.taps;i++){
            const double offs=tap_offsets?tap_offsets[i]:
                ((double)i+0.5)/(double)p.taps-0.5;
            const uint32_t c=sample_opaque_cry_clamp(cry,tw,th,
                u+offs*p.du,v+offs*p.dv,shade_lut,cry_rgb);
            rr+=(c>>16u)&255u;gg+=(c>>8u)&255u;bb+=c&255u;
        }
    }else for(unsigned i=0;i<p.taps;i++){
        const double offs=tap_offsets?tap_offsets[i]:
            ((double)i+0.5)/(double)p.taps-0.5;
        const uint32_t c=sample_opaque_bilinear_lut(s,mips,p.level,
            u+offs*p.du,v+offs*p.dv,wrap,shade,cry_rgb,shade_lut);
        rr+=(c>>16u)&255u;gg+=(c>>8u)&255u;bb+=c&255u;
    }
    *out_alpha=255u;
    return packrgb((rr+p.taps/2u)/p.taps,
                   (gg+p.taps/2u)/p.taps,(bb+p.taps/2u)/p.taps);
}
/* R313: projected wall U and all anisotropic U offsets remain invariant for
 * each entire drawn column. The old opaque CRY16 clamp sampler resolves the
 * same 4 texels, 10-bit weights and shade lookup separately on every row.
 * Only move the exact repeated U computations out of that hot loop. */
int avp_filter_cry_wall_column_prepare(AvpFilterCryWallColumn *col,
    const AvpGpuRgbImage *s,const AvpFilterImage *mips,
    double u,AvpFilterPlan p,int wrap,
    const uint32_t *cry_rgb,const uint8_t shade_lut[256])
{
    if(!col||!s||!s->rgb||s->alpha||!p.bilinear||wrap||
       !cry_rgb||!shade_lut||p.taps<1u||p.taps>16u||
       (p.level && (!mips||p.level>=mips->levels)))return 0;
    const unsigned width=p.level?mips->mip[p.level].width:(unsigned)s->width;
    const unsigned height=p.level?mips->mip[p.level].height:(unsigned)s->height;
    const uint16_t *cry=p.level?mips->mip[p.level].cry:s->cry16;
    if(!cry||!width||!height)return 0;
    col->cry=cry;col->cry_rgb=cry_rgb;col->shade_lut=shade_lut;
    col->width=width;col->height=height;col->taps=p.taps;
    for(unsigned i=0;i<p.taps;i++){
        const double offs=p.taps<=1u?0.0:avp_filter_tap_offsets[p.taps][i];
        const double fx=(u+offs*p.du)*(double)width-0.5;
        const int x=(int)floor(fx);
        unsigned ax=(unsigned)((fx-(double)x)*1024.0+0.5);
        if(ax>1024u)ax=1024u;
        col->x0[i]=(unsigned)(x<0?0:x>=(int)width?(int)width-1:x);
        col->x1[i]=(unsigned)(x+1<0?0:x+1>=(int)width?(int)width-1:x+1);
        col->ax[i]=ax;
        col->v_offset[i]=offs*p.dv;
    }
    return 1;
}
uint32_t avp_filter_cry_wall_column_sample(const AvpFilterCryWallColumn *col,double v)
{
    const uint16_t *cry=col->cry;
    const uint32_t *cry_rgb=col->cry_rgb;
    const uint8_t *shade_lut=col->shade_lut;
    const unsigned width=col->width,height=col->height,taps=col->taps;
    unsigned rr=0u,gg=0u,bb=0u;
    for(unsigned i=0;i<taps;i++){
        const double fy=(v+col->v_offset[i])*(double)height-0.5;
        const int y=(int)floor(fy);
        unsigned ay=(unsigned)((fy-(double)y)*1024.0+0.5);
        if(ay>1024u)ay=1024u;
        const unsigned y0=(unsigned)(y<0?0:y>=(int)height?(int)height-1:y);
        const unsigned y1=(unsigned)(y+1<0?0:y+1>=(int)height?(int)height-1:y+1);
        const size_t row0=(size_t)y0*width,row1=(size_t)y1*width;
        const unsigned x0=col->x0[i],x1=col->x1[i],ax=col->ax[i];
        const uint16_t t00=cry[row0+x0],t10=cry[row0+x1];
        const uint16_t t01=cry[row1+x0],t11=cry[row1+x1];
        const uint32_t c00=cry_rgb[(t00&0xff00u)|shade_lut[t00&255u]];
        const uint32_t c10=cry_rgb[(t10&0xff00u)|shade_lut[t10&255u]];
        const uint32_t c01=cry_rgb[(t01&0xff00u)|shade_lut[t01&255u]];
        const uint32_t c11=cry_rgb[(t11&0xff00u)|shade_lut[t11&255u]];
        const unsigned w00=(1024u-ax)*(1024u-ay),w10=ax*(1024u-ay);
        const unsigned w01=(1024u-ax)*ay,w11=ax*ay;
        const unsigned r=((c00>>16u)*w00+(c10>>16u)*w10+
                          (c01>>16u)*w01+(c11>>16u)*w11+524288u)>>20u;
        const unsigned g=(((c00>>8u)&255u)*w00+((c10>>8u)&255u)*w10+
                          ((c01>>8u)&255u)*w01+((c11>>8u)&255u)*w11+524288u)>>20u;
        const unsigned b=((c00&255u)*w00+(c10&255u)*w10+
                          (c01&255u)*w01+(c11&255u)*w11+524288u)>>20u;
        rr+=r;gg+=g;bb+=b;
    }
    return packrgb((rr+taps/2u)/taps,(gg+taps/2u)/taps,(bb+taps/2u)/taps);
}
/* R314: specialize the common native CRY16 flat sampling case. The
 * prepared block contains no framebuffer state and is local to one row
 * worker, so no shared mutable state, allocations or locks are needed. */
int avp_filter_cry_floor_block_prepare(AvpFilterCryFloorBlock *block,
    const AvpGpuRgbImage *s,const AvpFilterImage *mips,
    AvpFilterPlan p,int wrap,const uint32_t *cry_rgb,
    const uint8_t shade_lut[256])
{
    if(!block||!s||!s->rgb||s->alpha||!p.bilinear||!wrap||
       !cry_rgb||!shade_lut||p.taps<1u||p.taps>16u||
       (p.level && (!mips||p.level>=mips->levels)))return 0;
    const unsigned width=p.level?mips->mip[p.level].width:(unsigned)s->width;
    const unsigned height=p.level?mips->mip[p.level].height:(unsigned)s->height;
    const uint16_t *cry=p.level?mips->mip[p.level].cry:s->cry16;
    if(!cry||!width||!height)return 0;
    block->cry=cry;block->width=width;block->height=height;
    block->power_of_two=!(width&(width-1u)) && !(height&(height-1u));
    block->cry_rgb=cry_rgb;block->shade_lut=shade_lut;block->taps=p.taps;
    for(unsigned i=0u;i<p.taps;i++){
        const double offs=p.taps<=1u?0.0:avp_filter_tap_offsets[p.taps][i];
        block->u_offset[i]=offs*p.du;
        block->v_offset[i]=offs*p.dv;
    }
    return 1;
}
uint32_t avp_filter_cry_floor_block_sample(
    const AvpFilterCryFloorBlock *block,double u,double v)
{
    const unsigned taps=block->taps;
    if(taps<=1u){
        if(block->power_of_two)return sample_opaque_cry_pow2(block->cry,block->width,
            block->height,u,v,block->shade_lut,block->cry_rgb);
        return sample_opaque_cry_npot(block->cry,block->width,
            block->height,u,v,block->shade_lut,block->cry_rgb);
    }
    unsigned rr=0u,gg=0u,bb=0u;
    if(block->power_of_two){
        for(unsigned i=0u;i<taps;i++){
            const uint32_t c=sample_opaque_cry_pow2(block->cry,block->width,
                block->height,u+block->u_offset[i],v+block->v_offset[i],
                block->shade_lut,block->cry_rgb);
            rr+=(c>>16u)&255u;gg+=(c>>8u)&255u;bb+=c&255u;
        }
    }else{
        for(unsigned i=0u;i<taps;i++){
            const uint32_t c=sample_opaque_cry_npot(block->cry,block->width,
                block->height,u+block->u_offset[i],v+block->v_offset[i],
                block->shade_lut,block->cry_rgb);
            rr+=(c>>16u)&255u;gg+=(c>>8u)&255u;bb+=c&255u;
        }
    }
    return packrgb((rr+taps/2u)/taps,(gg+taps/2u)/taps,
                   (bb+taps/2u)/taps);
}
uint32_t avp_filter_sample(const AvpGpuRgbImage *s,const AvpFilterImage *mips,
                            double u,double v,AvpFilterDerivatives dd,
                            unsigned mode,unsigned aniso,int wrap,int shade,
                            const uint32_t *cry_rgb,unsigned *out_alpha)
{
    AvpFilterPlan p=avp_filter_plan(s,mips,dd,mode,aniso);
    return avp_filter_sample_prepared(s,mips,u,v,p,wrap,shade,cry_rgb,out_alpha);
}
void avp_filter_native_overlay(uint32_t *host,unsigned w,unsigned h,
                               const uint32_t *before,const uint32_t *after)
{
    if(!host||!w||!h||!before||!after)return;
    /* First-person sprites normally occupy a small patch of the native
     * screen. R308's Bilinear path filtered EVERY host pixel, including
     * millions with no changed source sprite. Bound the same exact sampling
     * kernel to native dirty pixels plus its one-texel bilinear halo. */
    unsigned sx0=320u,sy0=228u,sx1=0u,sy1=0u;
    for(unsigned y=0;y<228u;y++)for(unsigned x=0;x<320u;x++){
        size_t at=(size_t)y*320u+x;
        if(before[at]==after[at])continue;
        if(x<sx0)sx0=x;
        if(x>sx1)sx1=x;
        if(y<sy0)sy0=y;
        if(y>sy1)sy1=y;
    }
    if(sx0==320u)return;
    unsigned x0=w,x1=0u,y0=h,y1=0u;
    for(unsigned x=0;x<w;x++){
        double tx=((double)x+0.5)*320.0/w-0.5;
        int ix=(int)floor(tx);
        if(ix+1<(int)sx0||ix>(int)sx1)continue;
        if(x<x0)x0=x;
        if(x+1u>x1)x1=x+1u;
    }
    for(unsigned y=0;y<h;y++){
        double ty=((double)y+0.5)*228.0/h-0.5;
        int iy=(int)floor(ty);
        if(iy+1<(int)sy0||iy>(int)sy1)continue;
        if(y<y0)y0=y;
        if(y+1u>y1)y1=y+1u;
    }
    if(x0>=x1||y0>=y1)return;
    for(unsigned y=y0;y<y1;y++)for(unsigned x=x0;x<x1;x++){
        double tx=((double)x+0.5)*320.0/w-0.5,ty=((double)y+0.5)*228.0/h-0.5;
        int nx=(int)floor(tx),ny=(int)floor(ty);
        double fx=tx-nx,fy=ty-ny;
        double ac=0,rc=0,gc=0,bc=0;
        for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
            int xx=nx+dx,yy=ny+dy;
            if(xx<0||yy<0||xx>=320||yy>=228)continue;
            size_t at=(size_t)yy*320u+(unsigned)xx;
            if(before[at]==after[at])continue;
            double wt=(dx?fx:1.0-fx)*(dy?fy:1.0-fy);
            uint32_t c=after[at];
            ac+=wt;rc+=wt*((c>>16)&255u);gc+=wt*((c>>8)&255u);bc+=wt*(c&255u);
        }
        if(ac>0.0){
            size_t at=(size_t)y*w+x;
            uint32_t bg=host[at];
            unsigned a=(unsigned)(ac*255.0+0.5);
            unsigned r=(unsigned)(rc/ac+0.5),g=(unsigned)(gc/ac+0.5),b=(unsigned)(bc/ac+0.5);
            unsigned pr=(r*a+((bg>>16)&255u)*(255u-a)+127u)/255u;
            unsigned pg=(g*a+((bg>>8)&255u)*(255u-a)+127u)/255u;
            unsigned pb=(b*a+(bg&255u)*(255u-a)+127u)/255u;
            host[at]=packrgb(pr,pg,pb);
        }
    }
}

typedef struct HudAxis {int base;double fraction;} HudAxis;
typedef struct HudReplaySpan {
    uint32_t *host;
    unsigned w,h,first,last,background_stride;
    const uint32_t *after;
    const AvpHudRmwOps *ops;
    AvpHudRmwApply apply;
    void *context;
    uint32_t *background_mask;
    const unsigned char *mode,*coverage;
    const uint16_t *single_ink;
    const HudAxis *xs,*ys;
    const unsigned *native_x_begin,*native_x_end; /* optional conservative HUD-row bounds */
} HudReplaySpan;
static void hud_replay_rows(const HudReplaySpan *task)
{
    uint32_t *host=task->host;
    const unsigned w=task->w,h=task->h;
    const uint32_t *after=task->after;
    const AvpHudRmwOps *ops=task->ops;
    const AvpHudRmwApply apply=task->apply;
    void *context=task->context;
    uint32_t *background_mask=task->background_mask;
    const unsigned background_stride=task->background_stride;
    const unsigned char *mode=task->mode,*coverage=task->coverage;
    const uint16_t *single_ink=task->single_ink;
    const HudAxis *xs=task->xs,*ys=task->ys;
    for(unsigned y=task->first;y<task->last;y++){
        const double ty=ys?0.0:((double)y+0.5)*228.0/h-0.5;
        const int ny=ys?ys[y].base:(int)floor(ty);
        const double fy=ys?ys[y].fraction:ty-ny;
        unsigned xbegin=0u,xend=w;
        if(task->native_x_begin && task->native_x_end){
            xbegin=w;xend=0u;
            for(int dy=0;dy<2;dy++){
                const int row=ny+dy;
                if(row>=0 && row<228){
                    if(task->native_x_begin[row]<xbegin)xbegin=task->native_x_begin[row];
                    if(task->native_x_end[row]>xend)xend=task->native_x_end[row];
                }
            }
            if(xbegin>=xend)continue;
        }
        for(unsigned x=xbegin;x<xend;x++){
            const double tx=xs?0.0:((double)x+0.5)*320.0/w-0.5;
            const int nx=xs?xs[x].base:(int)floor(tx);
            const double fx=xs?xs[x].fraction:tx-nx;
            const size_t host_at=(size_t)y*w+x;
            if(nx>=0&&nx<319&&ny>=0&&ny<227){
                const size_t i=(size_t)ny*320u+(unsigned)nx;
                if(!coverage[i])continue;
                const uint16_t ink=single_ink[i];
                if(ink){
                    host[host_at]=apply(context,host[host_at],ink);
                    if(background_mask)
                        background_mask[(size_t)y*background_stride+x]=1u;
                    continue;
                }
            }else{
                int needed=0;
                for(int dy=0;dy<2 && !needed;dy++){
                    const int yy=ny+dy;
                    if(yy<0||yy>=228)continue;
                    for(int dx=0;dx<2;dx++){
                        const int xx=nx+dx;
                        if(xx>=0&&xx<320&&mode[(size_t)yy*320u+(unsigned)xx]){
                            needed=1;break;
                        }
                    }
                }
                if(!needed)continue;
            }
            const uint32_t backdrop=host[host_at];
            double rr=0,gg=0,bb=0;
            int changed=0;
            for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
                const int xx=nx+dx,yy=ny+dy;
                const double weight=(dx?fx:1.0-fx)*(dy?fy:1.0-fy);
                uint32_t c=backdrop;
                if(xx>=0&&yy>=0&&xx<320&&yy<228){
                    const size_t i=(size_t)yy*320u+(unsigned)xx;
                    if(mode[i]==1u){
                        const AvpHudRmwOps *p=&ops[i];
                        for(unsigned z=0;z<p->count;z++)
                            c=apply(context,c,p->ink[z]);
                        changed=1;
                    }else if(mode[i]==2u){c=after[i];changed=1;}
                }
                rr+=weight*((c>>16)&255u);
                gg+=weight*((c>>8)&255u);
                bb+=weight*(c&255u);
            }
            if(changed){
                host[host_at]=packrgb((unsigned)(rr+0.5),(unsigned)(gg+0.5),(unsigned)(bb+0.5));
                if(background_mask)background_mask[(size_t)y*background_stride+x]=1u;
            }
        }
    }
}
#ifndef _WIN32
static void *hud_replay_thread(void *arg)
{
    hud_replay_rows((const HudReplaySpan*)arg);
    return NULL;
}
#else
static DWORD WINAPI hud_replay_thread(LPVOID arg)
{
    hud_replay_rows((const HudReplaySpan*)arg);
    return 0u;
}
#endif

/* R384: this high-resolution native-ink HUD replay was still starting
 * three fresh pthreads and allocating two coordinate arrays per frame. A
 * thread-local host pool reuses workers and source-axis tables at an unchanged
 * window size. It owns no Jaguar/game state, and each job only writes to its
 * assigned output rows. Failure falls back to the original implementation. */
#ifndef _WIN32
#define AVP_HUD_MAX_WORKERS 3u
typedef struct AvpHudWorkerPool AvpHudWorkerPool;
typedef struct AvpHudWorker {
    AvpHudWorkerPool *pool;
    unsigned index;
} AvpHudWorker;
struct AvpHudWorkerPool {
    pthread_mutex_t mutex;
    pthread_cond_t start,finished;
    pthread_t workers[AVP_HUD_MAX_WORKERS];
    AvpHudWorker workers_info[AVP_HUD_MAX_WORKERS];
    HudReplaySpan jobs[AVP_HUD_MAX_WORKERS];
    unsigned count,done,generation;
    int stop;
};
typedef struct AvpHudTls {
    HudAxis *xs,*ys;
    unsigned width,height;
    AvpHudWorkerPool *pool;
} AvpHudTls;
static pthread_key_t avp_hud_key;
static pthread_once_t avp_hud_once=PTHREAD_ONCE_INIT;
static int avp_hud_key_valid=0;
static void *avp_hud_worker_loop(void *arg)
{
    AvpHudWorker *w=(AvpHudWorker*)arg;
    AvpHudWorkerPool *pool=w->pool;
    unsigned seen=0;
    pthread_mutex_lock(&pool->mutex);
    for(;;){
        HudReplaySpan job;
        while(!pool->stop && pool->generation==seen)
            pthread_cond_wait(&pool->start,&pool->mutex);
        if(pool->stop)break;
        seen=pool->generation;
        job=pool->jobs[w->index];
        pthread_mutex_unlock(&pool->mutex);
        hud_replay_rows(&job);
        pthread_mutex_lock(&pool->mutex);
        pool->done++;
        if(pool->done==pool->count)
            pthread_cond_signal(&pool->finished);
    }
    pthread_mutex_unlock(&pool->mutex);
    return NULL;
}
static void avp_hud_pool_destroy(AvpHudWorkerPool *pool)
{
    if(!pool)return;
    pthread_mutex_lock(&pool->mutex);
    pool->stop=1;
    pthread_cond_broadcast(&pool->start);
    pthread_mutex_unlock(&pool->mutex);
    for(unsigned i=0;i<pool->count;i++)
        (void)pthread_join(pool->workers[i],NULL);
    pthread_cond_destroy(&pool->start);
    pthread_cond_destroy(&pool->finished);
    pthread_mutex_destroy(&pool->mutex);
    free(pool);
}
static void avp_hud_tls_destroy(void *data)
{
    AvpHudTls *tls=(AvpHudTls*)data;
    if(!tls)return;
    avp_hud_pool_destroy(tls->pool);
    free(tls->xs);free(tls->ys);free(tls);
}
static void avp_hud_make_key(void)
{
    avp_hud_key_valid=pthread_key_create(&avp_hud_key,avp_hud_tls_destroy)==0;
}
static AvpHudTls *avp_hud_tls_get(void)
{
    AvpHudTls *tls;
    if(pthread_once(&avp_hud_once,avp_hud_make_key)!=0||
       !avp_hud_key_valid)return NULL;
    tls=(AvpHudTls*)pthread_getspecific(avp_hud_key);
    if(tls)return tls;
    tls=(AvpHudTls*)calloc(1,sizeof(*tls));
    if(!tls)return NULL;
    if(pthread_setspecific(avp_hud_key,tls)!=0){free(tls);return NULL;}
    return tls;
}
static int avp_hud_tls_axes(AvpHudTls *tls,unsigned w,unsigned h)
{
    if(!tls||!w||!h)return 0;
    if(tls->width!=w){
        HudAxis *xs=(HudAxis*)realloc(tls->xs,(size_t)w*sizeof(*xs));
        if(!xs)return 0;
        tls->xs=xs;
        for(unsigned x=0;x<w;x++){
            double tx=((double)x+0.5)*320.0/w-0.5;
            xs[x].base=(int)floor(tx);xs[x].fraction=tx-xs[x].base;
        }
        tls->width=w;
    }
    if(tls->height!=h){
        HudAxis *ys=(HudAxis*)realloc(tls->ys,(size_t)h*sizeof(*ys));
        if(!ys)return 0;
        tls->ys=ys;
        for(unsigned y=0;y<h;y++){
            double ty=((double)y+0.5)*228.0/h-0.5;
            ys[y].base=(int)floor(ty);ys[y].fraction=ty-ys[y].base;
        }
        tls->height=h;
    }
    return 1;
}
static AvpHudWorkerPool *avp_hud_tls_pool(AvpHudTls *tls,unsigned n)
{
    AvpHudWorkerPool *pool;
    if(!tls||n<2u||n>4u)return NULL;
    if(tls->pool)return tls->pool;
    pool=(AvpHudWorkerPool*)calloc(1,sizeof(*pool));
    if(!pool)return NULL;
    if(pthread_mutex_init(&pool->mutex,NULL)!=0){free(pool);return NULL;}
    if(pthread_cond_init(&pool->start,NULL)!=0){
        pthread_mutex_destroy(&pool->mutex);free(pool);return NULL;
    }
    if(pthread_cond_init(&pool->finished,NULL)!=0){
        pthread_cond_destroy(&pool->start);
        pthread_mutex_destroy(&pool->mutex);free(pool);return NULL;
    }
    for(unsigned i=0;i<n-1u;i++){
        pool->workers_info[i].pool=pool;
        pool->workers_info[i].index=i;
        if(pthread_create(&pool->workers[i],NULL,avp_hud_worker_loop,
                          &pool->workers_info[i])!=0)break;
        pool->count++;
    }
    if(!pool->count){avp_hud_pool_destroy(pool);return NULL;}
    tls->pool=pool;
    return pool;
}
static int avp_hud_tls_parallel(AvpHudTls *tls,const HudReplaySpan *base,
                                unsigned requested)
{
    AvpHudWorkerPool *pool=avp_hud_tls_pool(tls,requested);
    if(!pool)return 0;
    unsigned n=pool->count+1u;
    HudReplaySpan caller=*base;
    pthread_mutex_lock(&pool->mutex);
    pool->done=0;
    for(unsigned i=0;i<pool->count;i++){
        pool->jobs[i]=*base;
        pool->jobs[i].first=(unsigned)(((uint64_t)base->h*i)/n);
        pool->jobs[i].last=(unsigned)(((uint64_t)base->h*(i+1u))/n);
    }
    caller.first=(unsigned)(((uint64_t)base->h*(n-1u))/n);
    caller.last=base->h;
    pool->generation++;
    pthread_cond_broadcast(&pool->start);
    pthread_mutex_unlock(&pool->mutex);
    hud_replay_rows(&caller);
    pthread_mutex_lock(&pool->mutex);
    while(pool->done<pool->count)
        pthread_cond_wait(&pool->finished,&pool->mutex);
    pthread_mutex_unlock(&pool->mutex);
    return 1;
}
#endif

/* Bilinear filtering in *native HUD ink space*. For each screen sample the
 * original Jaguar RMW operations are replayed against that actual host pixel,
 * before the four neighboring ink results are blended. This avoids reducing
 * the transparent scene back to 320x228 and prevents dark texture fringes. */
void avp_filter_hud_replay(uint32_t *host,unsigned w,unsigned h,
                           const uint32_t *before,const uint32_t *after,
                           const AvpHudRmwOps *ops,AvpHudRmwApply apply,void *context,
                           uint32_t *background_mask,unsigned background_stride)
{
    if(!host||!w||!h||!before||!after||!ops||!apply)return;
    /* Validate the original native RMW command stream ONCE per source HUD
     * pixel, not once per enlarged output pixel (R308 multiplied that work
     * by four samples per output pixel). This preserves the exact native ink
     * replay on the actual high-resolution scene pixel and its source order.
     * mode 0: unchanged, 1: valid native ink replay, 2: original fallback. */
    unsigned char mode[320u*228u];
    unsigned row_first[228],row_last[228];
    unsigned changed_native=0u;
    for(unsigned y=0;y<228u;y++){row_first[y]=320u;row_last[y]=0u;}
    for(size_t i=0;i<320u*228u;i++){
        const AvpHudRmwOps *p=&ops[i];
        unsigned char m=0u;
        if(p->count>0u && p->count<=AVP_HUD_REPLAY_MAX_INKS){
            uint32_t simulated=before[i];
            for(unsigned z=0;z<p->count;z++)
                simulated=apply(context,simulated,p->ink[z]);
            if(simulated==after[i])m=1u;
        }
        if(!m && after[i]!=before[i])m=2u;
        mode[i]=m;
        if(m){
            changed_native++;
            const unsigned ny=(unsigned)(i/320u),nx=(unsigned)(i%320u);
            if(nx<row_first[ny])row_first[ny]=nx;
            if(nx+1u>row_last[ny])row_last[ny]=nx+1u;
        }
    }
    /* No native brush operation changed a pixel: a full-resolution scan,
     * axis setup and worker dispatch would necessarily do nothing. */
    if(!changed_native)return;
    /* Both 2x2 source coverage and common single-ink groups depend only
     * on the native HUD image, never on window resolution or the backdrop.
     * Prepare once before the high-resolution scanline. */
    unsigned char coverage[320u*228u]={0};
    uint16_t single_ink[320u*228u]={0};
    for(unsigned y=0;y<227u;y++)for(unsigned x=0;x<319u;x++){
        size_t i=(size_t)y*320u+x;
        coverage[i]=(unsigned char)(mode[i]||mode[i+1u]||
                                      mode[i+320u]||mode[i+321u]);
        if(mode[i]==1u&&mode[i+1u]==1u&&
           mode[i+320u]==1u&&mode[i+321u]==1u&&
           ops[i].count==1u&&ops[i+1u].count==1u&&
           ops[i+320u].count==1u&&ops[i+321u].count==1u){
            const uint16_t ink=ops[i].ink[0];
            if(ink&&ops[i+1u].ink[0]==ink&&ops[i+320u].ink[0]==ink&&
               ops[i+321u].ink[0]==ink)single_ink[i]=ink;
        }
    }
    /* Source-coordinate floor/fraction terms depend on output COLUMN or
     * ROW, not both; calculate each only once per presentation. */
    HudAxis *xs=NULL,*ys=NULL;
    int owns_axes=1;
#ifndef _WIN32
    AvpHudTls *tls=avp_hud_tls_get();
    if(tls && avp_hud_tls_axes(tls,w,h)){
        xs=tls->xs;ys=tls->ys;owns_axes=0;
    }
#endif
    if(owns_axes){
        xs=(HudAxis*)malloc((size_t)w*sizeof(*xs));
        ys=(HudAxis*)malloc((size_t)h*sizeof(*ys));
        if(xs)for(unsigned x=0;x<w;x++){
            double tx=((double)x+0.5)*320.0/w-0.5;
            xs[x].base=(int)floor(tx);xs[x].fraction=tx-xs[x].base;
        }
        if(ys)for(unsigned y=0;y<h;y++){
            double ty=((double)y+0.5)*228.0/h-0.5;
            ys[y].base=(int)floor(ty);ys[y].fraction=ty-ys[y].base;
        }
    }
    /* Filtering occurs in host-presentation space. Each row is independent
     * after native coverage validation; use separate disjoint row spans on
     * large windows. No game simulation, original pixels or mutable cache
     * is touched by the worker threads. */
    HudReplaySpan base={0};
    base.host=host;base.w=w;base.h=h;base.after=after;base.ops=ops;
    base.apply=apply;base.context=context;
    base.background_mask=background_mask;base.background_stride=background_stride;
    base.mode=mode;base.coverage=coverage;base.single_ink=single_ink;
    base.xs=xs;base.ys=ys;
    /* The original inner loop tests every host pixel, even when that native
     * HUD row has no ink. Build an exact conservative host-X span for each
     * changed source row. A source pixel can affect the destination where it
     * is either of the two bilinear neighbors (nx or nx+1). Non-HUD pixels
     * never enter the expensive sample/ink loop. The developer reference
     * switch leaves the original full scan available for differential tests.
     * Low-resolution input retains that original path as well. */
    unsigned native_x_begin[228],native_x_end[228];
    if(w>320u && h>228u && !getenv("AVP_HUD_REPLAY_FULL_SCAN")){
        unsigned first[320],last[320];
        for(unsigned k=0;k<320u;k++){first[k]=w;last[k]=0u;}
        for(unsigned x=0;x<w;x++){
            const double tx=xs?0.0:((double)x+0.5)*320.0/w-0.5;
            const int nx=xs?xs[x].base:(int)floor(tx);
            for(int dx=0;dx<2;dx++){
                const int source_x=nx+dx;
                if(source_x>=0 && source_x<320){
                    if(x<first[source_x])first[source_x]=x;
                    last[source_x]=x+1u;
                }
            }
        }
        for(unsigned y=0;y<228u;y++){
            native_x_begin[y]=w;native_x_end[y]=0u;
            /* The first and last modified texels conservatively bound all
             * changed native X values because the source mapping is ordered. */
            if(row_first[y]<row_last[y]){
                native_x_begin[y]=first[row_first[y]];
                native_x_end[y]=last[row_last[y]-1u];
            }
        }
        base.native_x_begin=native_x_begin;
        base.native_x_end=native_x_end;
    }
    unsigned n=(w>=1280u&&h>=720u)?4u:1u;
#ifndef _WIN32
    if(n>1u){long ncpu=sysconf(_SC_NPROCESSORS_ONLN);
        if(ncpu<2)n=1u;
        else if((unsigned long)ncpu<n)n=(unsigned)ncpu;}
#else
    if(n>1u){SYSTEM_INFO si;GetSystemInfo(&si);
        if(si.dwNumberOfProcessors<2u)n=1u;
        else if(si.dwNumberOfProcessors<n)n=si.dwNumberOfProcessors;}
#endif
    if(n==1u){base.first=0u;base.last=h;hud_replay_rows(&base);}
#ifndef _WIN32
    else if(tls && avp_hud_tls_parallel(tls,&base,n)){
        /* Pooled workers execute the same exact disjoint row spans. */
    }
#endif
    else{
        HudReplaySpan spans[4];
#ifndef _WIN32
        pthread_t threads[3];
#else
        HANDLE threads[3];
#endif
        unsigned launched=0u;
        for(unsigned i=0;i<n;i++){
            spans[i]=base;
            spans[i].first=(unsigned)(((uint64_t)h*i)/n);
            spans[i].last=(unsigned)(((uint64_t)h*(i+1u))/n);
        }
        for(unsigned i=0;i+1u<n;i++){
#ifndef _WIN32
            if(pthread_create(&threads[launched],NULL,hud_replay_thread,&spans[i])==0)
                launched++;
            else hud_replay_rows(&spans[i]);
#else
            HANDLE handle=CreateThread(NULL,0,hud_replay_thread,&spans[i],0,NULL);
            if(handle)threads[launched++]=handle;
            else hud_replay_rows(&spans[i]);
#endif
        }
        hud_replay_rows(&spans[n-1u]);
        for(unsigned i=0;i<launched;i++){
#ifndef _WIN32
            (void)pthread_join(threads[i],NULL);
#else
            WaitForSingleObject(threads[i],INFINITE);
            CloseHandle(threads[i]);
#endif
        }
    }
    if(owns_axes){free(xs);free(ys);}
}
