#include "pc/pc_vignette.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define CHECK(q) do{if(!(q)){fprintf(stderr,"vignette R390 FAIL line %d: %s\n",__LINE__,#q);return 1;}}while(0)
static uint32_t rng=0x390beefu;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void old_apply(const AvpPcVignetteMask *m,uint32_t *px,unsigned pitch,
    unsigned x,unsigned y,unsigned w,unsigned h,unsigned strength){
    for(unsigned row=0;row<h;row++){
        uint32_t *out=px+(size_t)(y+row)*pitch+x;
        const uint8_t *mask=m->weights+(size_t)row*w;
        for(unsigned col=0;col<w;col++){
            const unsigned opacity=(mask[col]*strength+50u)/100u;
            if(opacity){
                const uint32_t color=out[col];
                const unsigned brightness=256u-opacity;
                out[col]=((color&0x00ff00ffu)*brightness>>8&0x00ff00ffu)|
                         ((color&0x0000ff00u)*brightness>>8&0x0000ff00u);
            }
        }
    }
}
int main(void){
    static const unsigned shapes[][2]={{320,228},{853,480},{1920,1080},
        {2560,1440},{640,360},{128,640},{11,7},{1,1},{2,3}};
    uint64_t compared=0,skipped=0;
    for(unsigned size=0;size<sizeof(shapes)/sizeof(shapes[0]);size++){
        unsigned w=shapes[size][0],h=shapes[size][1];
        unsigned pitch=w+13u,whole=h+9u;
        size_t n=(size_t)pitch*whole;
        uint32_t *a=(uint32_t*)malloc(n*sizeof(uint32_t));
        uint32_t *b=(uint32_t*)malloc(n*sizeof(uint32_t));
        CHECK(a&&b);
        AvpPcVignetteMask m={0};
        CHECK(avp_pc_vignette_prepare(&m,w,h));
        for(unsigned ti=0;ti<(w<=320u?100u:10u);ti++){
            unsigned strength=w<=320u?ti+1u:(ti+1u)*10u;
            for(size_t i=0;i<n;i++)a[i]=b[i]=rnd()&0x00ffffffu;
            old_apply(&m,a,pitch,6u,4u,w,h,strength);
            avp_pc_vignette_apply(&m,b,pitch,pitch,whole,6u,4u,w,h,strength);
            CHECK(memcmp(a,b,n*sizeof(uint32_t))==0);
            compared+=(uint64_t)w*h;
            if(m.span_valid){
                for(unsigned row=0;row<h;row++)
                    skipped+=(uint64_t)w-m.left_count[row]-m.right_count[row];
            }
            /* Same intensity on next frame must retain the cached bounds. */
            const unsigned *old=m.left_count;
            avp_pc_vignette_apply(&m,b,pitch,pitch,whole,6u,4u,w,h,strength);
            CHECK(m.left_count==old && m.span_strength==strength);
        }
        avp_pc_vignette_destroy(&m);
        CHECK(!m.weights&&!m.left_count&&!m.right_count);
        free(a);free(b);
    }
    printf("R390 vignette: %llu exact pixels, %llu skipped inner pixel visits PASS\n",
      (unsigned long long)compared,(unsigned long long)skipped);
    return 0;
}
