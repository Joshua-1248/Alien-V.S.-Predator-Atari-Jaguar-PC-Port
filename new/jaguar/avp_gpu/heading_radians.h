#ifndef AVP_JAGUAR_AVP_GPU_HEADING_RADIANS_H
#define AVP_JAGUAR_AVP_GPU_HEADING_RADIANS_H
#include <stdint.h>
/* The retail visual angle is a 16-bit word.  Only Enhanced Analog Controls
 * allows the PC rasterizer to consume the full 16.16 angle transported by
 * the legacy game core.  This never changes the Jaguar CPU angle format. */
static inline double avp_gpu_heading_radians(uint32_t angle,int continuous)
{
    const double two_pi=6.283185307179586476925286766559;
    return continuous
        ?-(double)angle*(two_pi/4294967296.0)
        :-(double)(uint16_t)(angle>>16)*(two_pi/65536.0);
}
#endif
