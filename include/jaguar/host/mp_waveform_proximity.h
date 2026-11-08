#ifndef AVP_JAGUAR_HOST_MP_WAVEFORM_PROXIMITY_H
#define AVP_JAGUAR_HOST_MP_WAVEFORM_PROXIMITY_H
#include <stdint.h>
/* PC-only visual analyzer: sounds a character generates are local to that
 * player's own trace; the same recorded PCM reaches another trace only if
 * the owners share an original level and are within twelve map cells.
 * Chebyshev distance matches the native game's usual square range tests.
 * Original stereo output and enemy hearing are NOT altered. */
static inline unsigned avp_mp_waveform_gain(unsigned owner,unsigned listener,
    int source_level,int listener_level,int32_t sx,int32_t sy,int32_t lx,int32_t ly)
{
    const uint64_t radius=12ull<<16;
    uint64_t dx,dy,d;
    int64_t x=(int64_t)sx-(int64_t)lx;
    int64_t y=(int64_t)sy-(int64_t)ly;
    if(owner>15u || listener>3u || source_level<1 || source_level>15 ||
       source_level!=listener_level)return 0u;
    if(owner==listener)return 256u;
    dx=(uint64_t)(x<0?-x:x);dy=(uint64_t)(y<0?-y:y);
    d=dx>dy?dx:dy;
    if(d>=radius)return 0u;
    return (unsigned)(((radius-d)*256u)/radius);
}
#endif
