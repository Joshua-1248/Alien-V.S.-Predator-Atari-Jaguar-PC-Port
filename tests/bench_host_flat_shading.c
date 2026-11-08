#define _POSIX_C_SOURCE 200809L
#include "jaguar/avp_gpu/host_flat_shading.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double seconds(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec*1e-9;}
/* R6 arithmetic, without the redundant out-of-bounds modulo branch; this
 * isolates the row-shade optimization rather than crediting both changes. */
static inline uint32_t original(const AvpGpuRgbImage *t,unsigned x,unsigned y,
                                int delta,const uint32_t table[65536])
{
    const size_t i=(size_t)y*(size_t)t->width+x;
    if(t->cry16){
        const u16 cry=t->cry16[i];
        int intensity=(cry&255u)+delta;
        if(intensity<0)intensity=0;
        if(intensity>255)intensity=255;
        return table[(cry&0xff00u)|(unsigned)intensity];
    }
    const u8 *p=t->rgb+i*3u;
    return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
}
int main(int argc,char **argv)
{
    const unsigned width=argc>1?(unsigned)atoi(argv[1]):2560u;
    const unsigned height=argc>2?(unsigned)atoi(argv[2]):1440u;
    const unsigned rounds=12u;
    uint32_t *table=malloc(65536u*sizeof(uint32_t));
    u16 *tex=malloc(128u*128u*sizeof(u16));
    u8 *rgb=malloc(128u*128u*3u);
    AvpGpuRgbImage t={128,128,rgb,tex};
    if(!table||!tex||!rgb||!width||!height||width>8192u||height>8192u)return 2;
    for(unsigned i=0;i<65536u;i++)table[i]=0x34211u^(i*75901u);
    for(unsigned i=0;i<128u*128u;i++)tex[i]=(u16)(i*71717u);
    for(unsigned mode=0;mode<2u;mode++){
        double start=seconds();uint64_t sum=0;
        for(unsigned n=0;n<rounds;n++)for(unsigned y=0;y<height;y++){
            const int delta=(int)(y%241u)-120;
            u8 shade[256];
            if(mode)avp_gpu_host_flat_row_shades(shade,delta);
            for(unsigned x=0;x<width;x++){
                const unsigned tx=(x*7919u+y*13u)&127u;
                const unsigned ty=(y*13u+x*17u)&127u;
                sum+=mode?avp_gpu_host_flat_sample_inbounds(&t,tx,ty,shade,table)
                         :original(&t,tx,ty,delta,table);
            }
        }
        printf("%s %ux%u: %.3f ms/scan-frame checksum=%llu\n",
               mode?"R7 row lookup":"R6 reference",width,height,
               1000.0*(seconds()-start)/rounds,(unsigned long long)sum);
    }
    free(table);free(tex);free(rgb);
    return 0;
}
