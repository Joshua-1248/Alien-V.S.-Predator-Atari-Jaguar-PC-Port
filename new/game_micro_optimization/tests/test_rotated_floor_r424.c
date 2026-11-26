#define AVP_MICRO_TEST_ROTATED_FLOOR 1
#include "../point_floor_row.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
static u32 rgb[65536],a[3840],b[3840];
static u16 tex[128*128];static u8 maze[64*64*8],shades[256];
static const AvpGpuRgbImage *textures[256];static unsigned rotations[256];
static AvpGpuRgbImage img;
static unsigned rng=0x1234abcdu;
static unsigned next(void){return rng=rng*1664525u+1013904223u;}
static void ref(u32 *row,unsigned w,double wx,double wy,double dx,double dy,unsigned ceiling,int clear){
    for(unsigned x=0;x<w;x++,wx+=dx,wy+=dy){
        int cx,cy;if(!avp_gpu_floor_valid_cell(wx,wy,64,64,&cx,&cy)){if(clear)row[x]=0;continue;}
        const unsigned f=maze[((size_t)cy*64u+(unsigned)cx)*8u+(ceiling?4u:5u)];
        unsigned tx=(unsigned)((wx-cx)*128.0),ty=(unsigned)((wy-cy)*128.0),ox=tx,oy=ty;
        switch(rotations[f]){case 1:tx=oy;ty=127u-ox;break;case 2:tx=127u-ox;ty=127u-oy;break;case 3:tx=127u-oy;ty=ox;break;}
        row[x]=avp_gpu_host_flat_sample_inbounds(&img,tx,ty,shades,rgb);
    }
}
static void compare(unsigned w,double wx,double wy,double dx,double dy,unsigned ceiling,int clear){
    for(unsigned i=0;i<w;i++)a[i]=b[i]=0xdeadbeef;
    ref(a,w,wx,wy,dx,dy,ceiling,clear);
    avp_micro_point_floor_row_impl(b,w,maze,64,64,ceiling,wx,wy,dx,dy,textures,rotations,shades,rgb,clear);
    assert(!memcmp(a,b,w*sizeof *a));
}
int main(void){
    img.width=img.height=128;img.cry16=tex;
    for(unsigned i=0;i<65536;i++)rgb[i]=next();
    for(unsigned i=0;i<128*128;i++)tex[i]=(u16)(next()>>16);
    for(unsigned i=0;i<sizeof maze;i++)maze[i]=(u8)(next()>>24);
    for(unsigned i=0;i<256;i++){textures[i]=&img;rotations[i]=1+i%3;}
    unsigned long long pixels=0;
    for(unsigned k=0;k<20000;k++){
        unsigned w=1+(next()>>16)%3840;double wx=(next()%64)+(next()>>8)/16777216.0,wy=(next()%64)+(next()>>8)/16777216.0;
        double dx=((double)(next()>>8)-8388608.0)/71353651.0,dy=((double)(next()>>8)-8388608.0)/91376511.0;
        if(k%3==0)dx/=64,dy/=64;
        if(k%7==0)dx=0;
        if(k%11==0)dy=-0.0;
        if(k%13==0)wx=nextafter(floor(wx),-INFINITY);
        if(k%17==0)wy=nextafter(floor(wy),INFINITY);
        if(k%211==0)dx=NAN;
        if(k%223==0)dy=INFINITY;
        avp_gpu_host_flat_row_shades(shades,(int)(next()>>24)-128);
        compare(w,wx,wy,dx,dy,k&1u,(k>>1)&1u);pixels+=w;
    }
    for(unsigned rot=1;rot<=3;rot++){
        for(unsigned i=0;i<256;i++)rotations[i]=rot;
        for(unsigned t=0;t<8192;t++)for(unsigned j=0;j<3;j++)for(unsigned axis=0;axis<2;axis++){
            double q=t/128.0;if(j==0)q=nextafter(q,-INFINITY);if(j==2)q=nextafter(q,INFINITY);
            compare(17,axis?31.75:q,axis?q:31.75,axis?0:1.0/4096.0,axis?-1.0/4096.0:0,axis,1);pixels+=17;
        }
    }
    printf("PASS: %llu exact rotated floor pixels\n",pixels);
    return 0;
}
