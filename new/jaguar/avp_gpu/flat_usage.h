#ifndef AVP_JAGUAR_AVP_GPU_FLAT_USAGE_H
#define AVP_JAGUAR_AVP_GPU_FLAT_USAGE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* Host-only floor mip preheat: every cell contains a ceiling flat at byte 4
 * and floor flat at byte 5. Visit the full maze so turning the camera cannot
 * suddenly need a previously skipped mip chain. No gameplay state changes. */
static inline void avp_gpu_collect_used_flats(const uint8_t *maze,
    uint16_t mw,uint16_t mh,uint8_t used[256])
{
    memset(used,0,256u);
    if(!maze || !mw || !mh){memset(used,1,256u);return;}
    for(size_t i=0,n=(size_t)mw*(size_t)mh;i<n;i++){
        const uint8_t *cell=maze+i*8u;
        used[cell[4]]=1u;
        used[cell[5]]=1u;
    }
}
#endif
