#include "../frame_copy_fast.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int main(void){
 const unsigned dims[][3]={{5,3,3},{321,227,319},{1920,1080,1440},{2560,1440,1920},{2561,1441,1921},{3840,2160,2880}};
 size_t count=0;
 for(unsigned k=0;k<sizeof(dims)/sizeof(*dims);k++){
  unsigned w=dims[k][0],h=dims[k][1],c=dims[k][2],left=(w-c)/2;
  size_t n=(size_t)w*h,m=(size_t)c*h;
  uint32_t *world=malloc((n+2)*4),*copy=malloc((n+2)*4),*crop=malloc((m+2)*4);assert(world&&copy&&crop);
  for(size_t i=0;i<n+2;i++)world[i]=copy[i]=(uint32_t)i*0x9e3779b9u;
  crop[0]=crop[m+1]=0xdeadbeef;
  AvpMicroStridedCopy in={world+1+left,crop+1,c,w,c};
  for(unsigned j=7;j>0;j--)avp_micro_strided_copy_rows(&in,h*(j-1)/7,h*j/7);
  for(unsigned y=0;y<h;y++)assert(!memcmp(crop+1+(size_t)y*c,world+1+(size_t)y*w+left,c*4));
  assert(crop[0]==0xdeadbeef&&crop[m+1]==0xdeadbeef);
  for(size_t i=1;i<=m;i++)crop[i]^=0x12345678u;
  AvpMicroStridedCopy out={crop+1,world+1+left,c,c,w};
  for(unsigned j=0;j<7;j++)avp_micro_strided_copy_rows(&out,h*j/7,h*(j+1)/7);
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)assert(world[1+(size_t)y*w+x]==(copy[1+(size_t)y*w+x]^((x>=left&&x<left+c)?0x12345678u:0u)));
  assert(world[0]==copy[0]&&world[n+1]==copy[n+1]);count+=m+n;free(world);free(copy);free(crop);
 }
 printf("PASS: %zu crop/world pixels including odd widths and guard cells\n",count);
 return 0;
}
