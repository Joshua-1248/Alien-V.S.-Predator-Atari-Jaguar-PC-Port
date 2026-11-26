#ifndef AVP_GPU_WALL_SHADE_CACHE_H
#define AVP_GPU_WALL_SHADE_CACHE_H

#include <stdint.h>

/* PC host-only filtered-wall shade cache. Each raster worker owns one
 * instance: no shared writes, allocations, or lock traffic. Wall and portal
 * spans tend to reuse the same fog shade across consecutive columns. Four
 * direct-mapped entries also retain nearby shade values without allocating
 * a 256x256 global table. The fallback/oracle is the original saturating
 * CRY intensity transform, with the same 256 entries and signed shade. */
typedef struct AvpWallShadeCache {
    uint8_t valid[4];
    int key[4];
    uint8_t lut[4][256];
} AvpWallShadeCache;

static inline const uint8_t *avp_wall_shade_cache_get(AvpWallShadeCache *cache,
                                                      int shade)
{
    const unsigned slot=((unsigned)shade)&3u;
    if(!cache->valid[slot] || cache->key[slot]!=shade){
        for(unsigned i=0;i<256u;i++){
            const int v=(int)i+shade;
            cache->lut[slot][i]=(uint8_t)(v<0?0:v>255?255:v);
        }
        cache->key[slot]=shade;
        cache->valid[slot]=1u;
    }
    return cache->lut[slot];
}

#endif
