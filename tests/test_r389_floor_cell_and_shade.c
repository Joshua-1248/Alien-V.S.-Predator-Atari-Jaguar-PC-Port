/* R389 exact-output oracle: no asserts that disappear in Release builds. */
#include "jaguar/avp_gpu/floor_row_cell_cache.h"
#include "jaguar/avp_gpu/wall_shade_cache.h"
#include "jaguar/avp_gpu/host_flat_shading.h"
#include "jaguar/avp_gpu/floor_cell_sampling.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do{if(!(e)){fprintf(stderr,"R389 FAIL line %d: %s\n",__LINE__,#e);return 1;}}while(0)
static uint32_t random_state=0xa389e01du;
static uint32_t rnd(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
int main(void)
{
    enum{MW=64,MH=64};
    uint8_t *maze=(uint8_t*)malloc(MW*MH*8u);
    CHECK(maze);
    for(unsigned i=0;i<MW*MH*8u;i++)maze[i]=(uint8_t)rnd();
    uint64_t values=0,hits=0,changes=0,shades=0;
    for(unsigned ceiling=0;ceiling<2u;ceiling++){
        for(unsigned widx=0;widx<4;widx++){
            static const unsigned widths[]={320u,853u,1920u,2560u};
            const unsigned width=widths[widx];
            for(unsigned row=0;row<160u;row++){
                AvpFloorRowCellCache cache;
                avp_floor_row_cell_cache_init(&cache,maze,MW,ceiling);
                const double origin_x=-12.0+(double)(rnd()%74000u)/1000.0;
                const double origin_y=-5.0+(double)(rnd()%65000u)/1000.0;
                const double step_x=(double)((int)(rnd()%1601u)-800)/18000.0;
                const double step_y=(double)((int)(rnd()%1601u)-800)/18000.0;
                double wx=origin_x,wy=origin_y;
                for(unsigned x=0;x<width;x++,wx+=step_x,wy+=step_y){
                    int cx,cy;
                    if(!avp_gpu_floor_valid_cell(wx,wy,MW,MH,&cx,&cy))continue;
                    const uint8_t expected=maze[((size_t)cy*MW+(size_t)cx)*8u+(ceiling?4u:5u)];
                    unsigned changed=avp_floor_row_cell_cache_update(&cache,cx,cy);
                    CHECK(cache.flat==expected);
                    CHECK(cache.cx==cx && cache.cy==cy);
                    changes+=changed;hits+=!changed;values++;
                }
            }
        }
    }
    CHECK(values>100000u && hits>changes);
    /* Cache invalidation at a new row, including repeated/identical coords. */
    {
        AvpFloorRowCellCache cache;
        avp_floor_row_cell_cache_init(&cache,maze,MW,0);
        CHECK(avp_floor_row_cell_cache_update(&cache,7,6));
        CHECK(!avp_floor_row_cell_cache_update(&cache,7,6));
        uint8_t old=cache.flat;
        maze[(6u*MW+7u)*8u+5u]=(uint8_t)(old^0xffu);
        CHECK(!avp_floor_row_cell_cache_update(&cache,7,6));
        avp_floor_row_cell_cache_init(&cache,maze,MW,0);
        CHECK(avp_floor_row_cell_cache_update(&cache,7,6));
        CHECK(cache.flat==(uint8_t)(old^0xffu));
    }
    AvpWallShadeCache shade_cache[8]={0};
    for(unsigned i=0;i<12000u;i++){
        int shade=(int)(rnd()%913u)-456;
        /* Include repeated values, slot collisions, negative and positive
         * saturation; each worker cache is independent and reusable. */
        if((i&7u)==0u)shade=0;
        unsigned worker=i&7u;
        uint8_t reference[256];
        avp_gpu_host_flat_row_shades(reference,shade);
        const uint8_t *cached=avp_wall_shade_cache_get(&shade_cache[worker],shade);
        CHECK(memcmp(cached,reference,256u)==0);
        shades+=256u;
    }
    free(maze);
    printf("R389 floor cache: %llu exact pixels (%llu reused), %llu exact shades PASS\n",
        (unsigned long long)values,(unsigned long long)hits,(unsigned long long)shades);
    return 0;
}
