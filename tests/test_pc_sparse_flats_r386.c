/* Every reachable flat lookup must see exactly the binding supplied by
 * the former unconditional 256-entry preheat (4, 16, 256 flat-code maps). */
#include "jaguar/avp_gpu/flat_usage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t seed=0x386c17au;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void)
{
    enum { W=64,H=64 };
    uint8_t *maze=calloc((size_t)W*H,8u);
    assert(maze);
    const unsigned counts[]={4u,16u,256u};
    uint64_t compared=0;
    for(unsigned mode=0;mode<3u;mode++){
        unsigned n=counts[mode];uint8_t used[256];
        uint32_t reference[256], sparse[256]={0};
        for(unsigned i=0;i<256;i++)reference[i]=rnd();
        for(size_t i=0;i<(size_t)W*H;i++){
            maze[8u*i+4u]=(uint8_t)(rnd()%n);
            maze[8u*i+5u]=(uint8_t)(rnd()%n);
        }
        avp_gpu_collect_used_flats(maze,W,H,used);
        for(unsigned f=0;f<256u;f++)if(used[f])sparse[f]=reference[f];
        for(unsigned i=0;i<500000u;i++){
            size_t cell=(size_t)(rnd()%((unsigned)W*H));
            unsigned flat=maze[8u*cell+4u+(rnd()&1u)];
            assert(used[flat]);
            assert(reference[flat]==sparse[flat]);
            compared++;
        }
        unsigned count=0;
        for(unsigned f=0;f<256u;f++)count+=used[f];
        assert(count==n);
    }
    uint8_t used[256];
    avp_gpu_collect_used_flats(NULL,0,0,used);
    for(unsigned i=0;i<256u;i++)assert(used[i]);
    free(maze);
    printf("R386 sparse flats: %llu equivalent sampled floor/ceiling bindings PASS\n",
           (unsigned long long)compared);
    return 0;
}
