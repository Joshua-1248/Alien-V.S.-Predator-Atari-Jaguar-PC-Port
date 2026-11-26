#define _POSIX_C_SOURCE 200809L
#include "../point_floor_row.h"
#undef AVP_MICRO_POINT_FLOOR_ROW_H
#define avp_micro_point_floor_row_impl r431_point_floor_row_impl
#define avp_micro_point_floor_row r431_point_floor_row
#define avp_micro_floor_defer_color_clear r431_floor_defer_color_clear
#include "point_floor_row_r431.h"
#undef avp_micro_point_floor_row_impl
#undef avp_micro_point_floor_row
#undef avp_micro_floor_defer_color_clear
#include <assert.h>
#include <stdio.h>
#include <time.h>
static u32 rgb[65536],a[3840],b[3840];
static u16 tex[4][16384];static u8 maze[64*64*8],shades[256];
static const AvpGpuRgbImage *textures[256];static unsigned rotations[256];
static AvpGpuRgbImage imgs[4];static unsigned rng=0x432ac171u;
static unsigned next(void){return rng=rng*1664525u+1013904223u;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static unsigned long long count;
static void check(unsigned w,double wx,double wy,double dx,double dy,unsigned c,int clear){
 for(unsigned i=0;i<w;i++)a[i]=b[i]=0x12345678;
 r431_point_floor_row_impl(a,w,maze,64,64,c,wx,wy,dx,dy,textures,rotations,shades,rgb,clear);
 avp_micro_point_floor_row_impl(b,w,maze,64,64,c,wx,wy,dx,dy,textures,rotations,shades,rgb,clear);
 assert(!memcmp(a,b,w*sizeof *a));count+=w;
}
typedef void (*Row)(u32 *,unsigned,const u8 *,unsigned,unsigned,unsigned,double,double,double,double,const AvpGpuRgbImage *const *,const unsigned *,const u8 *,const u32 *,int);
static volatile u32 sink;
static double bench(Row row,unsigned axis,int sign){
 double t=now();u32 sum=0;
 for(unsigned k=0;k<20000;k++){
  double q=(double)(1+k%250)*0.00003125*(double)sign;
  double wx=31.125+(k%53)*0.01,wy=32.25+(k%47)*0.01;
  row(b,2560,maze,64,64,k&1,wx,wy,axis?0:q,axis?q:0,textures,rotations,shades,rgb,1);
  sum^=b[k%2560];
 }sink^=sum;return now()-t;
}
int main(int argc,char **argv){
 for(unsigned i=0;i<65536;i++)rgb[i]=next();
 for(unsigned i=0;i<4;i++){imgs[i].width=imgs[i].height=128;imgs[i].cry16=tex[i];for(unsigned j=0;j<16384;j++)tex[i][j]=(u16)(next()>>16);}
 for(unsigned i=0;i<sizeof maze;i++)maze[i]=(u8)(next()>>24);
 for(unsigned i=0;i<256;i++)textures[i]=i%13?&imgs[i%4]:NULL;
 for(unsigned i=0;i<256;i++)shades[i]=(u8)(next()>>24);
 for(unsigned k=0;k<24000;k++){
  unsigned w=1+(next()>>16)%3840,axis=k&1u;
  double wx=(next()%64)+(next()>>8)/16777216.0,wy=(next()%64)+(next()>>8)/16777216.0;
  double q=((double)(next()>>8)-8388608.0)/71353651.0;
  if(k%3==0)q/=128;if(k%11==0)q=0x1p-33;if(k%13==0)q=-0x1p-32;
  if(k%17==0)q=0x1p24;if(k%19==0)q=-0x1p24;
  if(k%23==0)q=nextafter(0.0,1.0);if(k%29==0)q=0.0;
  if(k%31==0)wx=nextafter(floor(wx),-INFINITY);if(k%37==0)wy=nextafter(floor(wy),INFINITY);
  check(w,wx,wy,axis?0:q,axis?q:0,(k>>1)&1,(k>>2)&1);
 }
 for(unsigned t=0;t<8192;t++)for(unsigned j=0;j<3;j++)for(unsigned axis=0;axis<2;axis++){
  double q=t/128.0;if(j==0)q=nextafter(q,-INFINITY);if(j==2)q=nextafter(q,INFINITY);
  check(63,axis?31.75:q,axis?q:31.75,axis?0:1.0/4096.0,axis?-1.0/4096.0:0,axis,1);
 }
 printf("PASS: %llu R431 parity pixels\n",count);
 if(argc>1){for(unsigned axis=0;axis<2;axis++)for(int sign=-1;sign<=1;sign+=2){double old1=bench(r431_point_floor_row_impl,axis,sign),new1=bench(avp_micro_point_floor_row_impl,axis,sign),new2=bench(avp_micro_point_floor_row_impl,axis,sign),old2=bench(r431_point_floor_row_impl,axis,sign);printf("axis=%u sign=%d old=%.6f/%.6f new=%.6f/%.6f ratio=%.3fx\n",axis,sign,old1,old2,new1,new2,(old1+old2)/(new1+new2));}}
 (void)argv;return 0;
}
