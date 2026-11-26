#define _POSIX_C_SOURCE 200809L
#include "../fp_wide_fast.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
static uint32_t seed=427;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed^(seed>>16);}
static double now(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec*1e-9;}
static uint8_t sprite[320*240],pal[208][3];
static void fill(int sw,int sh,int four,int mode){unsigned stride=four?(sw+1)/2:sw;for(unsigned y=0;y<(unsigned)sh;y++)for(unsigned x=0;x<stride;x++){unsigned z=rnd();sprite[y*stride+x]=mode==0?z:mode==1?((x/12+y/10)%4?z:0):mode==2?(z%4==0?z:0):z|0x11;}}
int main(int argc,char **argv){(void)argv;for(unsigned i=0;i<208;i++)for(int j=0;j<3;j++)pal[i][j]=rnd();
 if(argc>1){unsigned w=2560,h=1440;size_t n=(size_t)w*h;uint32_t *a=calloc(n,4),*b=calloc(n,4);assert(a&&b);for(int mode=0;mode<4;mode++)for(int four=0;four<2;four++){fill(160,120,four,mode);double total[2]={0};for(int k=0;k<4;k++)for(int q=0;q<2;q++){int opt=(k+q)%2;double t=now();for(int i=0;i<80;i++){if(opt)avp_micro_fp_wide_blit(b,w,h,sprite,sizeof sprite,160,120,pal,four?16:208,100,108,four);else avp_gpu_fp_wide_blit(a,w,h,sprite,sizeof sprite,160,120,pal,four?16:208,100,108,four);}total[opt]+=now()-t;}assert(!memcmp(a,b,n*4));printf("mode=%d four=%d baseline %.3f ms candidate %.3f ms speedup %.3fx\n",mode,four,total[0]*1000/320,total[1]*1000/320,total[0]/total[1]);}free(a);free(b);return 0;}
 const unsigned dims[][2]={{1,1},{320,228},{640,480},{641,457},{1280,720},{1920,1080},{2560,1440},{2571,1439},{3840,2160}};unsigned long long count=0;for(unsigned d=0;d<sizeof dims/sizeof dims[0];d++){unsigned w=dims[d][0],h=dims[d][1];size_t n=(size_t)w*h;uint32_t *a=malloc((n+16)*4),*b=malloc((n+16)*4);assert(a&&b);for(int k=0;k<20;k++){int sw=1+rnd()%320,sh=1+rnd()%240,four=k&1,x=(int)(rnd()%700)-250,y=(int)(rnd()%500)-200;unsigned pc=k%5==0?0:k%5==1?4:k%5==2?16:k%5==3?208:250;fill(sw,sh,four,k%4);for(size_t i=0;i<n+16;i++)a[i]=b[i]=rnd();avp_gpu_fp_wide_blit(a+8,w,h,sprite,sizeof sprite,sw,sh,pal,pc,x,y,four);avp_micro_fp_wide_blit(b+8,w,h,sprite,sizeof sprite,sw,sh,pal,pc,x,y,four);assert(!memcmp(a,b,(n+16)*4));count+=n;}free(a);free(b);}printf("PASS %llu exact output pixels and guard regions\n",count);}
