#define _POSIX_C_SOURCE 200809L
#include "../sprite_span_fast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "jaguar/avp_gpu/maze2_occlusion.h"
static uint32_t rng=12345; static uint32_t next(void){rng=rng*1664525u+1013904223u;return rng;}
static void ref(uint32_t *p,const AvpMaze2Occlusion *o,unsigned w,int x0,int x1,int y0,int y1,uint32_t c,float z){
    for(int x=x0;x<x1;x++)for(int y=y0;y<y1;y++)if(!o||!avp_gpu_maze2_occlusion_blocks(o,x,y,z))p[(size_t)y*w+x]=c;
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv){(void)argv;uint64_t checked=0;const unsigned widths[]={17,127,319,1280,2560,3841};
    for(unsigned k=0;k<sizeof(widths)/sizeof(*widths);k++){
        unsigned w=widths[k],h=97;size_t n=(size_t)w*h;uint32_t *a=malloc(n*4),*b=malloc(n*4);float *d=malloc(n*4);if(!a||!b||!d)return 2;
        AvpMaze2Occlusion o={w,h,d};
        for(unsigned t=0;t<180;t++){
            for(size_t i=0;i<n;i++){a[i]=next();switch(next()%7){case 0:d[i]=NAN;break;case 1:d[i]=FLT_MAX;break;case 2:d[i]=INFINITY;break;case 3:d[i]=-INFINITY;break;default:d[i]=(float)(next()%200)*0.125f;}}
            memcpy(b,a,n*4);int x=next()%w,y=next()%h,x1=x+1+next()%100,y1=y+1+next()%40;if(x1>(int)w)x1=w;if(y1>(int)h)y1=h;
            float z=(t%17==0)?NAN:(t%19==0)?FLT_MAX:(float)(next()%200)*0.125f;uint32_t c=next();
            ref(a,t%3?&o:NULL,w,x,x1,y,y1,c,z);avp_micro_sprite_span(b,t%3?d:NULL,w,x,x1,y,y1,c,z);
            if(memcmp(a,b,n*4)){fprintf(stderr,"Mismatch %u %u\n",w,t);return 1;}checked+=n;
        }
        free(a);free(b);free(d);
    }
    printf("PASS %llu full-buffer pixel comparisons\n",(unsigned long long)checked);
    if(argc>1){unsigned w=2560,h=1440;size_t n=(size_t)w*h;uint32_t *p=calloc(n,4);float *d=malloc(n*4);if(!p||!d)return 2;AvpMaze2Occlusion o={w,h,d};
        for(int mix=0;mix<3;mix++){
            for(size_t i=0;i<n;i++)d[i]=mix==0?FLT_MAX:mix==1?(next()%16?FLT_MAX:0.5f):(next()%2?FLT_MAX:0.5f);
            for(int span=4;span<=32;span*=2){double ts[2]={0,0};uint64_t sum=0;for(int rep=0;rep<4;rep++){int mode=rep%2;double t=now();for(int j=0;j<16000;j++){int x=(j*13)%(w-span),y=(j*17)%(h-24);if(mode)avp_micro_sprite_span(p,d,w,x,x+span,y,y+24,(uint32_t)j,1.f);else ref(p,&o,w,x,x+span,y,y+24,(uint32_t)j,1.f);}ts[mode]+=now()-t;sum+=p[(rep+1)*w+13];}printf("mix=%d span=%d ref=%.6f fast=%.6f ratio=%.3f checksum=%llu\n",mix,span,ts[0],ts[1],ts[0]/ts[1],(unsigned long long)sum);}
        }free(p);free(d);
    }return 0;
}
