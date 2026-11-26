#define _POSIX_C_SOURCE 200809L
#include "../scanline_fast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static uint32_t rng=0x129fa3bc;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(int argc,char **argv){
    if(argc>1){
        const unsigned w=2560,h=1440;size_t n=(size_t)w*h;uint32_t *a=malloc(n*4),*b=malloc(n*4);assert(a&&b);
        for(size_t i=0;i<n;i++)a[i]=rnd();
        for(unsigned style=2;style<12;style++){
            AvpPcScanlines s={1,style,3,4,65,5};double old=0,fast=0;
            for(unsigned pass=0;pass<4;pass++){
                memcpy(b,a,n*4);double t=now();
                for(int k=0;k<20;k++)if(pass==0||pass==3)avp_scanline_apply(&s,b,w,w,h);else avp_micro_scanline_apply(&s,b,w,w,h);
                double d=now()-t;if(pass==0||pass==3)old+=d;else fast+=d;
            }
            printf("style=%u scalar=%.3f ms SIMD=%.3f ms speedup=%.2fx checksum=%08x\n",style,old*25,fast*25,old/fast,b[n/2]);
        } free(a);free(b);return 0;
    }
    (void)argv;
    enum {P=79,H=41}; uint32_t a[P*H+8],b[P*H+8],src[P*H+8];
    for(unsigned i=0;i<P*H+8;i++)src[i]=rnd();
    size_t pixels=0,cases=0;
    for(unsigned style=0;style<12;style++)for(unsigned t=1;t<=16;t++)for(unsigned sp=1;sp<=16;sp++)for(unsigned phase=0;phase<=31;phase+=7)for(unsigned strength=1;strength<=100;strength+=33){
        AvpPcScanlines s={1,style,t,sp,strength,phase};
        unsigned width=1+rnd()%P,height=1+rnd()%H,off=rnd()%4;
        memcpy(a,src,sizeof(a));memcpy(b,src,sizeof(b));
        avp_scanline_apply(&s,a+off,P,width,height);avp_micro_scanline_apply(&s,b+off,P,width,height);
        assert(memcmp(a,b,sizeof(a))==0);pixels+=(size_t)width*height;cases++;
    }
    for(unsigned invalid=0;invalid<6;invalid++){
        AvpPcScanlines s={1,2,1,2,35,0};
        if(invalid==0)s.enabled=0;
        if(invalid==1)s.style=12;
        if(invalid==2)s.thickness=0;
        if(invalid==3)s.spacing=17;
        if(invalid==4)s.strength=101;
        if(invalid==5)s.phase=32;
        memcpy(b,src,sizeof b);avp_micro_scanline_apply(&s,b,P,72,H);assert(!memcmp(b,src,sizeof b));
    }
    printf("PASS: %zu cases / %zu rendered pixels, alpha, guard margins, unaligned pointers, odd widths, all styles\n",cases,pixels);return 0;
}
