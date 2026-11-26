/* R388: compare the new filtered-wall shade cache with the exact R387
 * saturating shade transform, including adversarial cache collisions.
 * Each thread has an independent cache instance as in the renderer. */
#include "jaguar/avp_gpu/wall_shade_cache.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void old_lut(uint8_t *lut,int shade)
{
    for(unsigned i=0;i<256u;i++){
        int v=(int)i+shade;
        lut[i]=(uint8_t)(v<0?0:v>255?255:v);
    }
}
static void *worker(void *arg)
{
    uint32_t seed=(uint32_t)(uintptr_t)arg+0x9e3779b9u;
    AvpWallShadeCache cache={0};
    uint8_t expected[256];
    for(unsigned k=0;k<180000u;k++){
        seed=seed*1664525u+1013904223u;
        /* Include repeated shades, a full signed range, and collisions. */
        int shade=(k%7u==0u)?(int)(seed%2049u)-1024:
                  (k%5u==0u)?(int)((seed>>8u)&127u)-64:
                  (int)((seed>>16u)&3u)-2;
        const uint8_t *found=avp_wall_shade_cache_get(&cache,shade);
        old_lut(expected,shade);
        if(memcmp(found,expected,256u)!=0){
            fprintf(stderr,"R388 mismatch shade=%d case=%u\n",shade,k);abort();
        }
    }
    return NULL;
}
int main(void)
{
    pthread_t threads[7];
    for(uintptr_t i=0;i<7;i++)if(pthread_create(&threads[i],NULL,worker,(void*)(i+1u))!=0)abort();
    worker((void*)(uintptr_t)9u);
    for(unsigned i=0;i<7;i++)if(pthread_join(threads[i],NULL)!=0)abort();
    puts("R388 wall shade cache: 1,440,000 exact shade lookups across 8 workers PASS");
    return 0;
}
