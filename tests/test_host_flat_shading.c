#include "jaguar/avp_gpu/host_flat_shading.h"
#ifdef NDEBUG
#undef NDEBUG /* This executable must verify parity in Release builds too. */
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Independent reference for the retail CRY Y-only saturating operation,
 * including no-CRY RGB fallback. Texture coordinates are produced by the
 * SAME fraction and rotation rules as the actual floor renderer, but the
 * reference never calls the optimized intensity map or sample routine. */
static uint32_t reference(const AvpGpuRgbImage *t,unsigned x,unsigned y,
                          int delta,const uint32_t rgb_table[65536])
{
    const size_t i=(size_t)y*(size_t)t->width+x;
    if(t->cry16){
        const uint16_t c=t->cry16[i];
        int intensity=(c&255u)+delta;
        if(intensity<0)intensity=0;
        if(intensity>255)intensity=255;
        return rgb_table[(c&0xff00u)|(uint16_t)intensity];
    }
    const uint8_t *p=t->rgb+i*3u;
    return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
}

int main(void)
{
    enum { W=128,H=128 };
    uint8_t *rgb=malloc(W*H*3u);
    uint16_t *cry=malloc(W*H*sizeof(uint16_t));
    uint32_t *lut=malloc(65536u*sizeof(uint32_t));
    uint32_t seed=0x145a13e9u;
    AvpGpuRgbImage t={W,H,rgb,cry};
    const int deltas[]={-512,-255,-254,-129,-128,-50,-1,0,1,27,128,254,255,256,512};
    unsigned long long checks=0;
    assert(rgb && cry && lut);
    for(unsigned i=0;i<65536u;i++)lut[i]=0x12000000u^(i*0x7654321u);
    for(size_t i=0;i<W*H;i++){
        seed=seed*1664525u+1013904223u;
        cry[i]=(uint16_t)seed;
        rgb[i*3u]=(uint8_t)seed;
        rgb[i*3u+1u]=(uint8_t)(seed>>9);
        rgb[i*3u+2u]=(uint8_t)(seed>>17);
    }
    for(unsigned no_cry=0;no_cry<2u;no_cry++){
        t.cry16=no_cry?NULL:cry;
        for(unsigned d=0;d<sizeof(deltas)/sizeof(deltas[0]);d++){
            uint8_t shades[256];
            avp_gpu_host_flat_row_shades(shades,deltas[d]);
            for(unsigned v=0;v<256u;v++){
                int expected=(int)v+deltas[d];
                if(expected<0)expected=0;
                if(expected>255)expected=255;
                assert(shades[v]==(uint8_t)expected);
            }
            for(unsigned rot=0;rot<4;rot++){
                for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++){
                    unsigned tx=x,ty=y;
                    switch(rot){
                    case 1:tx=y;ty=H-1u-x;break;
                    case 2:tx=W-1u-x;ty=H-1u-y;break;
                    case 3:tx=W-1u-y;ty=x;break;
                    default:break;
                    }
                    const uint32_t actual=avp_gpu_host_flat_sample_inbounds(
                        &t,tx,ty,shades,lut);
                    assert(actual==reference(&t,tx,ty,deltas[d],lut));
                    checks++;
                }
            }
        }
    }
    /* Direct exhaustive CRY sample of all 65536 bit patterns. */
    t.cry16=cry;
    for(unsigned c=0;c<65536u;c++){
        cry[0]=(uint16_t)c;
        const uint8_t row[256]={0};
        assert(avp_gpu_host_flat_sample_inbounds(&t,0,0,row,lut)==
               lut[c&0xff00u]);
        checks++;
    }
    free(rgb);free(cry);free(lut);
    printf("Host flat shading: %llu exact sample comparisons PASS\n",checks);
    return 0;
}
