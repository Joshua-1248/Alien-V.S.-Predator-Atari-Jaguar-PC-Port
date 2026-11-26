#ifndef AVP_JAGUAR_AVP_GPU_HIRES_ALPHA_BLEND_H
#define AVP_JAGUAR_AVP_GPU_HIRES_ALPHA_BLEND_H
#include <stdint.h>
/* PC-only PNG coverage after original CRY shading. The source is straight RGB
 * (never premultiplied against black), and zero coverage keeps background. */
static inline uint32_t avp_gpu_hires_alpha_blend(uint32_t bg,uint32_t fg,unsigned a)
{
    unsigned b;
    if(a>=255u)return fg;
    if(a==0u)return bg;
    b=255u-a;
    return (((((fg>>16)&255u)*a+((bg>>16)&255u)*b+127u)/255u)<<16) |
           (((((fg>>8)&255u)*a+((bg>>8)&255u)*b+127u)/255u)<<8) |
           (((fg&255u)*a+(bg&255u)*b+127u)/255u);
}
#endif
