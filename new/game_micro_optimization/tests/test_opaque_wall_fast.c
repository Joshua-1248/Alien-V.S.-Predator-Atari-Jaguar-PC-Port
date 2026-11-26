#include "../opaque_wall_fast.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
static uint32_t seed=419;static uint32_t next(void){seed=seed*1664525u+1013904223u;return seed;}
static uint32_t rgb[65536],a[1440*19],b[1440*19];static float da[1440*19],db[1440*19];static uint16_t tex[128*128];
int main(void){
 for(unsigned i=0;i<65536;i++)rgb[i]=next();for(unsigned i=0;i<128*128;i++)tex[i]=(uint16_t)(next()>>16);
 unsigned long long count=0;
 for(unsigned k=0;k<16000;k++){
  unsigned w=1+next()%19,x=next()%w,tx=next()%128;int h=128+(int)(next()%8000),y0=(int)(next()%1440),y1=y0+1+(int)(next()%(1440-y0));
  int base=y0+(int)(next()%10000)-5000,shift=(int)(next()%257)-128,shade=(int)(next()%511)-255;
  float depth=(float)(next()%1000)/16.0f;
  for(unsigned i=0;i<w*1440;i++){a[i]=b[i]=next();da[i]=db[i]=(next()%4)?(float)(next()%1000)/16.0f:FLT_MAX;}
  for(int y=y0;y<y1;y++){
   size_t at=(size_t)y*w+x;if(da[at]<=depth)continue;da[at]=depth;
   int ty=(int)(((int64_t)(y-base)*2+1)*128/((int64_t)h*2))+shift;
   if(ty<0)ty=0;if(ty>127)ty=127;uint16_t c=tex[ty*128+tx];int v=(int)(c&255u)+shade;if(v<0)v=0;if(v>255)v=255;a[at]=rgb[(c&0xff00u)|(unsigned)v];
  }
  avp_micro_opaque_wall128(b,db,w,x,y0,y1,base,h,tex,tx,shift,shade,rgb,depth);
  assert(!memcmp(a,b,w*1440*sizeof *a));assert(!memcmp(da,db,w*1440*sizeof *da));count+=(unsigned)(y1-y0);
 }
 printf("PASS: %llu wall samples, complete pixel and depth buffers compared\n",count);
}
