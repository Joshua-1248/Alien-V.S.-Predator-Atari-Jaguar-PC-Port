#include "../opaque_wall_fast.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint32_t seed=428; static uint32_t next(void){seed=seed*1664525u+1013904223u;return seed;}
static uint32_t rgb[65536];static uint16_t tex[128*128];
static double clock_s(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void clear(float *d,size_t n){for(size_t i=0;i<n;i++)d[i]=FLT_MAX;}
static void run(int fresh,uint32_t *p,float *d,unsigned w,unsigned h){
 for(unsigned x=0;x<w;x++){
  int a=(int)(x%71),b=(int)h-a,wh=(int)h+128;
  if(fresh)avp_micro_opaque_wall128_fresh(p,d,w,x,a,b,-64,wh,tex,x%128,0,-35,rgb,1.5f);
  else avp_micro_opaque_wall128(p,d,w,x,a,b,-64,wh,tex,x%128,0,-35,rgb,1.5f);
 }
}
int main(int argc,char **argv){
 for(unsigned i=0;i<65536;i++)rgb[i]=next();for(unsigned i=0;i<128*128;i++)tex[i]=(uint16_t)(next()>>16);
 unsigned w=31,h=1440;size_t n=(size_t)w*h;uint32_t *a=malloc(n*4),*b=malloc(n*4);float *da=malloc(n*4),*db=malloc(n*4);assert(a&&b&&da&&db);
 unsigned long long samples=0;
 for(unsigned k=0;k<6000;k++){
  unsigned x=next()%w,tx=next()%128;int wh=128+(int)(next()%8000),y0=(int)(next()%h),y1=y0+1+(int)(next()%(h-y0));int base=y0+(int)(next()%10000)-5000,shift=(int)(next()%257)-128,shade=(int)(next()%511)-255;float depth=(float)(next()%1000)/16.0f;
  for(size_t i=0;i<n;i++){a[i]=b[i]=next();da[i]=db[i]=(float)(next()%1000);}
  for(unsigned y=0;y<h;y++)da[(size_t)y*w+x]=db[(size_t)y*w+x]=FLT_MAX;
  avp_micro_opaque_wall128(a,da,w,x,y0,y1,base,wh,tex,tx,shift,shade,rgb,depth);
  avp_micro_opaque_wall128_fresh(b,db,w,x,y0,y1,base,wh,tex,tx,shift,shade,rgb,depth);
  assert(!memcmp(a,b,n*4));assert(!memcmp(da,db,n*4));samples+=y1-y0;
 }
 printf("PASS: %llu wall pixels and %llu full buffer entries per color/depth comparison\n",samples,(unsigned long long)n*6000);
 free(a);free(b);free(da);free(db);
 if(argc<2)return 0;
 w=2560;h=1440;n=(size_t)w*h;a=calloc(n,4);b=calloc(n,4);da=malloc(n*4);db=malloc(n*4);assert(a&&b&&da&&db);
 clear(da,n);clear(db,n);run(0,a,da,w,h);run(1,b,db,w,h);assert(!memcmp(a,b,n*4));assert(!memcmp(da,db,n*4));
 for(unsigned pair=0;pair<4;pair++){
  double t[2]={0,0};for(unsigned j=0;j<2;j++){unsigned mode=(j+pair)%2;for(unsigned i=0;i<12;i++){clear(da,n);double start=clock_s();run(mode,a,da,w,h);t[mode]+=clock_s()-start;}}
  printf("pair %u base %.3f ms fresh %.3f ms speedup %.3fx\n",pair,t[0]*1000/12,t[1]*1000/12,t[0]/t[1]);
 }
 printf("checksum %u\n",a[n/2]);free(a);free(b);free(da);free(db);return 0;
}
