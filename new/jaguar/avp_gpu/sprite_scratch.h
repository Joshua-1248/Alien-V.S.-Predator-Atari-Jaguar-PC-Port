#ifndef AVP_GPU_SPRITE_SCRATCH_H
#define AVP_GPU_SPRITE_SCRATCH_H

#include "jaguar/avp_gpu/texture_filter.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* R389: viewport-owned, reusable decode memory for filtered original AAF
 * sprites. Not a decoded-frame cache: all RGB/CRY/alpha pixels are cleared
 * for EVERY frame to preserve transparent-column and mipmap semantics.
 * Three replacement allocations commit atomically, so allocation failures
 * retain any previously valid workspace. */
typedef struct AvpGpuSpriteScratch {
    uint8_t *rgb;
    uint16_t *cry16;
    uint8_t *alpha;
    size_t capacity;
    /* Owned by this viewport; not shared across local-MP or map-maker copies.
     * Scratch RGB/CRY/alpha are overwritten for each actor, and every mip
     * texel is fully recomputed, while the allocated mip storage persists. */
    AvpFilterImage mip_chain;
    /* Exact per-row V samples for tall projected sprites. */
    double *sample_v;
    size_t sample_capacity;
} AvpGpuSpriteScratch;

static inline void avp_gpu_sprite_scratch_detach(AvpGpuSpriteScratch *s)
{
    memset(s,0,sizeof(*s));
}
static inline void avp_gpu_sprite_scratch_destroy(AvpGpuSpriteScratch *s)
{
    /* Inline ownership teardown keeps the scratch contract independent of
     * whether a particular standalone tool links texture_filter.c. */
    for(unsigned l=1u;l<AVP_FILTER_MIP_MAX;l++){
        free(s->mip_chain.mip[l].rgba);
        free(s->mip_chain.mip[l].cry);
    }
    free(s->rgb);free(s->cry16);free(s->alpha);
    free(s->sample_v);
    avp_gpu_sprite_scratch_detach(s);
}
/* Optional cache; on OOM the raster retains its exact per-pixel formula. */
static inline double *avp_gpu_sprite_scratch_rows(AvpGpuSpriteScratch *s,
    size_t count)
{
    if(!s||!count||count>32768u)return NULL;
    if(s->sample_capacity<count){
        double *rows=(double*)malloc(count*sizeof(double));
        if(!rows)return NULL;
        free(s->sample_v);
        s->sample_v=rows;
        s->sample_capacity=count;
    }
    return s->sample_v;
}
static inline int avp_gpu_sprite_scratch_prepare(AvpGpuSpriteScratch *s,
    size_t count)
{
    if(!s || !count || count>256u*256u)return 0;
    if(s->capacity<count){
        uint8_t *rgb=(uint8_t*)malloc(count*3u);
        uint16_t *cry=(uint16_t*)malloc(count*sizeof(*cry));
        uint8_t *alpha=(uint8_t*)malloc(count);
        if(!rgb||!cry||!alpha){
            free(rgb);free(cry);free(alpha);return 0;
        }
        free(s->rgb);free(s->cry16);free(s->alpha);
        s->rgb=rgb;s->cry16=cry;s->alpha=alpha;s->capacity=count;
    }
    memset(s->rgb,0,count*3u);
    memset(s->cry16,0,count*sizeof(*s->cry16));
    memset(s->alpha,0,count);
    return 1;
}
#endif
