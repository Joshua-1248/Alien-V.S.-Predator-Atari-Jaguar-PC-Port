#ifndef AVP_MICRO_FRAME_COPY_FAST_H
#define AVP_MICRO_FRAME_COPY_FAST_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Source and destination are separate, tightly packed host buffers.
 * Synchronous worker dispatch must complete before the X11 upload begins. */
typedef struct AvpMicroFrameCopy {
    const uint32_t *src;
    uint32_t *dst;
    unsigned width;
} AvpMicroFrameCopy;
static inline void avp_micro_frame_copy_rows(void *context,unsigned first,unsigned last)
{
    const AvpMicroFrameCopy *p=(const AvpMicroFrameCopy*)context;
    size_t offset=(size_t)first*p->width;
    memcpy(p->dst+offset,p->src+offset,(size_t)(last-first)*p->width*sizeof(uint32_t));
}
/* R448: each worker fences streaming stores before completing. */
#if defined(__linux__) && defined(__SSE2__)
#include <emmintrin.h>
static inline void avp_r448_copy_stream(void *context,unsigned first,unsigned last)
{
    const AvpMicroFrameCopy *p=(const AvpMicroFrameCopy*)context;
    size_t off=(size_t)first*p->width,n=(size_t)(last-first)*p->width;
    const uint32_t *s=p->src+off;uint32_t *d=p->dst+off;
    while(n && ((uintptr_t)d&15u)){*d++=*s++;--n;}
    while(n>=16u){
        _mm_stream_si128((__m128i*)(d+0),_mm_loadu_si128((const __m128i*)(s+0)));
        _mm_stream_si128((__m128i*)(d+4),_mm_loadu_si128((const __m128i*)(s+4)));
        _mm_stream_si128((__m128i*)(d+8),_mm_loadu_si128((const __m128i*)(s+8)));
        _mm_stream_si128((__m128i*)(d+12),_mm_loadu_si128((const __m128i*)(s+12)));
        d+=16;s+=16;n-=16;
    }
    while(n){*d++=*s++;--n;}
    _mm_sfence();
}
#endif
/* Copy a centered HUD crop (or restore it) without touching margins. */
typedef struct AvpMicroStridedCopy {
    const uint32_t *src;
    uint32_t *dst;
    unsigned width,src_stride,dst_stride;
} AvpMicroStridedCopy;
static inline void avp_micro_strided_copy_rows(void *context,unsigned first,unsigned last)
{
    const AvpMicroStridedCopy *p=(const AvpMicroStridedCopy*)context;
    for(unsigned y=first;y<last;y++)
        memcpy(p->dst+(size_t)y*p->dst_stride,p->src+(size_t)y*p->src_stride,
               (size_t)p->width*sizeof(uint32_t));
}
#ifndef _WIN32
void avp_micro_frame_copy_parallel(const uint32_t *src,uint32_t *dst,unsigned w,unsigned h);
#endif
#endif
