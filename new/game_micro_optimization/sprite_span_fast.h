#ifndef AVP_MICRO_SPRITE_SPAN_FAST_H
#define AVP_MICRO_SPRITE_SPAN_FAST_H
#include <stdint.h>
#include <stddef.h>
#include <float.h>
#if defined(__SSE2__)
#include <emmintrin.h>
#endif
/* AAF source texel spans own disjoint destination samples. Caller clips both
 * axes and splits the X span at solid column occlusion. Depth is read-only. */
static inline void avp_micro_sprite_span(uint32_t *pixels,const float *depth,
    unsigned stride,int x0,int x1,int y0,int y1,uint32_t color,float object_depth)
{
#if defined(__SSE2__)
    const __m128 od=_mm_set1_ps(object_depth), sentinel=_mm_set1_ps(FLT_MAX);
    const __m128i ink=_mm_set1_epi32((int32_t)color);
#endif
    for(int y=y0;y<y1;++y){
        size_t at=(size_t)y*stride+(unsigned)x0;
        uint32_t *p=pixels+at;
        const float *d=depth?depth+at:NULL;
        int x=0,n=x1-x0;
#if defined(__SSE2__)
        for(;x+4<=n;x+=4){
            int blocked=0;
            if(d){
                const __m128 z=_mm_loadu_ps(d+x);
                blocked=_mm_movemask_ps(_mm_and_ps(_mm_cmpneq_ps(z,sentinel),_mm_cmpge_ps(od,z)));
            }
            if(!blocked)_mm_storeu_si128((__m128i*)(p+x),ink);
            else if(blocked!=15)for(int i=0;i<4;++i)if(!(blocked&(1<<i)))p[x+i]=color;
        }
#endif
        for(;x<n;++x)if(!d||!(d[x]!=FLT_MAX&&object_depth>=d[x]))p[x]=color;
    }
}
#endif
