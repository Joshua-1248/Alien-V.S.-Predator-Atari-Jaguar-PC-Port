#ifndef AVP_GPU_HOST_FLAT_SHADING_H
#define AVP_GPU_HOST_FLAT_SHADING_H

#include "jaguar/avp_gpu/bridge.h"
#include <stdint.h>

/* Host-resolution floor/ceiling shading only. The Jaguar changes the CRY
 * intensity Y, never its two chroma nibbles. For a given projected row the
 * intensity delta is invariant, so clamp its 256 possible input values once
 * instead of on every output pixel. The original CRY->RGB table is reused. */
static inline void avp_gpu_host_flat_row_shades(u8 shades[256],int delta)
{
    for(unsigned v=0;v<256u;v++){
        int intensity=(int)v+delta;
        if(intensity<0)intensity=0;
        if(intensity>255)intensity=255;
        shades[v]=(u8)intensity;
    }
}

/* Preconditions established by avp_gpu_floor_valid_cell and the source
 * texture dimensions: 0<=x<width, 0<=y<height, rgb is loaded. Rotation
 * mappings also preserve these bounds. The wall/shading fallback retains
 * its original generic modulo-wrapped sampler. */
static inline uint32_t avp_gpu_host_flat_sample_inbounds(
    const AvpGpuRgbImage *t,unsigned x,unsigned y,
    const u8 shades[256],const uint32_t cry_rgb[65536])
{
    const size_t i=(size_t)y*(size_t)t->width+(size_t)x;
    if(t->cry16){
        const u16 cry=t->cry16[i];
        return cry_rgb[(cry&0xff00u)|(unsigned)shades[cry&0xffu]];
    }
    const u8 *rgb=t->rgb+i*3u;
    return ((uint32_t)rgb[0]<<16)|((uint32_t)rgb[1]<<8)|rgb[2];
}

#endif
