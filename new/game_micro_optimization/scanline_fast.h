#ifndef AVP_MICRO_SCANLINE_FAST_H
#define AVP_MICRO_SCANLINE_FAST_H
#include "../pc_host/include/pc/pc_scanlines.h"
/* SSE2 is part of the x86-64 ABI. Other targets retain the scalar renderer.
 * All coefficients are exact integers in [0,256]; unsigned 16-bit products
 * fit without overflow. Alpha is copied verbatim, including transparent RGB. */
#if defined(__SSE2__) && (defined(__x86_64__) || defined(_M_X64))
#include <emmintrin.h>
static inline void avp_micro_scanline_apply(const AvpPcScanlines *s,uint32_t *pixels,
    unsigned pitch,unsigned width,unsigned height)
{
    if(!avp_scanline_active(s)||!pixels||!width||!height||pitch<width||
       (size_t)height>SIZE_MAX/sizeof(*pixels)/pitch)return;
    if(s->style==AVP_SCAN_HORIZONTAL||s->style==AVP_SCAN_HORIZONTAL_SOFT||width<4u){
        avp_scanline_apply(s,pixels,pitch,width,height);return;
    }
    const unsigned period=s->thickness+s->spacing;
    unsigned weights[32][32];
    __m128i pair[32][32];
    unsigned next[32];
    /* Every output row/column repeats within this period. Prepare a pair
     * of pixel coefficients for every phase. Reuse them across the frame. */
    for(unsigned x=0;x<period;x++)next[x]=(x+4u)%period;
    for(unsigned y=0;y<period;y++){
        for(unsigned x=0;x<period;x++)weights[y][x]=avp_scanline_weight(s,x,y);
        for(unsigned x=0;x<period;x++){
            short a=(short)weights[y][x],b=(short)weights[y][(x+1u)%period];
            pair[y][x]=_mm_set_epi16(256,b,b,b,256,a,a,a);
        }
    }
    const __m128i zero=_mm_setzero_si128();
    unsigned yp=0;
    for(unsigned y=0;y<height;y++){
        uint32_t *row=pixels+(size_t)y*pitch;
        const __m128i *p=pair[yp];
        const unsigned *w=weights[yp];
        unsigned xp=0,x=0;
        for(;x<=width-4u;x+=4u){
            __m128i v=_mm_loadu_si128((const __m128i*)(const void*)(row+x));
            __m128i lo=_mm_mullo_epi16(_mm_unpacklo_epi8(v,zero),p[xp]);
            unsigned hi=xp+2u;if(hi>=period)hi-=period;
            __m128i up=_mm_mullo_epi16(_mm_unpackhi_epi8(v,zero),p[hi]);
            __m128i out=_mm_packus_epi16(_mm_srli_epi16(lo,8),_mm_srli_epi16(up,8));
            _mm_storeu_si128((__m128i*)(void*)(row+x),out);
            xp=next[xp];
        }
        for(;x<width;x++){
            row[x]=avp_scanline_darken(row[x],w[xp]);
            if(++xp==period)xp=0;
        }
        if(++yp==period)yp=0;
    }
}
#else
#define avp_micro_scanline_apply avp_scanline_apply
#endif
#endif
