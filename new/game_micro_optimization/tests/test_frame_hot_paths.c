#include "../cry_rmw_fast.h"
#include "../point_floor_row.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static u32 rng=0x4102026u;
static u32 next(void){rng=rng*1664525u+1013904223u;return rng;}
static u32 rgb[65536];static u8 q[131072],map[64*64*8];
static u16 tex[8][64*64];static u8 texrgb[8][64*64*3];
static AvpGpuRgbImage img[8];static const AvpGpuRgbImage *textures[256];static unsigned rotations[256];
static void oldrow(u32 *row,unsigned width,unsigned mw,unsigned mh,unsigned ceiling,double wx,double wy,double dx,double dy,const u8 shade[256]){
    for(unsigned x=0;x<width;x++,wx+=dx,wy+=dy){int cx,cy;
        if(!avp_gpu_floor_valid_cell(wx,wy,mw,mh,&cx,&cy))continue;
        unsigned f=map[((size_t)cy*mw+(unsigned)cx)*8+(ceiling?4:5)];const AvpGpuRgbImage *t=textures[f];
        if(!t)continue;
        if(t->width<=0||t->height<=0){row[x]=0x005a462d;continue;}
        int tx=(int)((wx-(double)cx)*t->width),ty=(int)((wy-(double)cy)*t->height),ox=tx,oy=ty;
        if(t->width==t->height)switch(rotations[f]){case 1:tx=oy;ty=t->height-1-ox;break;case 2:tx=t->width-1-ox;ty=t->height-1-oy;break;case 3:tx=t->width-1-oy;ty=ox;break;default:break;}
        row[x]=avp_gpu_host_flat_sample_inbounds(t,(unsigned)tx,(unsigned)ty,shade,rgb);
    }
}
int main(void){
    for(unsigned i=0;i<65536;i++){u8 r,g,b;avp_jaguar_cry16_to_rgb((u16)i,&r,&g,&b);rgb[i]=((u32)r<<16)|((u32)g<<8)|b;}
    for(unsigned i=0;i<sizeof q;i++)q[i]=(u8)(next()>>24);
    unsigned count=0;
    for(unsigned i=0;i<4000000;i++){u32 dst=next();u16 src=(u16)(next()>>16);assert(avp_micro_cry_rmw(dst,src,q,sizeof q,rgb)==avp_jaguar_rmw_xrgb8888(dst,src,q,sizeof q));count++;}
    for(unsigned y=0;y<256;y++)for(unsigned c=0;c<256;c++)for(unsigned i=0;i<3;i++){u32 dst=i==0?(y<<16)|(y<<8)|c:i==1?(c<<16)|(y<<8)|y:(y<<16)|(c<<8)|y;u16 src=(u16)(next()>>16);assert(avp_micro_cry_rmw(dst,src,q,sizeof q,rgb)==avp_jaguar_rmw_xrgb8888(dst,src,q,sizeof q));count++;}
    assert(avp_micro_cry_rmw(0xfedcba98,0,NULL,0,rgb)==0xfedcba98);assert(avp_micro_cry_rmw(0xfedcba98,123,q,5,rgb)==0xfedcba98);
    for(unsigned i=0;i<8;i++){img[i].width=64;img[i].height=(i==6?32:64);img[i].rgb=texrgb[i];img[i].cry16=(i%2==0?tex[i]:NULL);for(unsigned j=0;j<4096;j++)tex[i][j]=(u16)(next()>>16);for(unsigned j=0;j<sizeof texrgb[i];j++)texrgb[i][j]=(u8)(next()>>24);}
    img[7].width=0;
    for(unsigned i=0;i<256;i++){textures[i]=i%9==0?NULL:&img[i%8];rotations[i]=(i/8)%4;}
    u32 a[3840],b[3840];u8 shades[256];unsigned long long pixels=0;
    for(unsigned k=0;k<16000;k++){unsigned mw=1+(next()>>24)%64,mh=1+(next()>>24)%64,w=1+(next()>>16)%2048,ceiling=next()>>31;for(unsigned i=0;i<mw*mh*8;i++)map[i]=(u8)(next()>>24);double wx=((int)(next()>>16)-4096)/1024.,wy=((int)(next()>>16)-4096)/1024.,dx=((int)(next()>>16)-32768)/131072.,dy=((int)(next()>>16)-32768)/131072.;if(k%3==0){wx=next()%mw;wy=next()%mh;dx/=16;dy/=16;}if(k%17==0)dx=0;if(k%19==0)dy=0;avp_gpu_host_flat_row_shades(shades,(int)(next()>>24)-128);for(unsigned i=0;i<w;i++)a[i]=b[i]=0x12345678;oldrow(a,w,mw,mh,ceiling,wx,wy,dx,dy,shades);avp_micro_point_floor_row(b,w,map,mw,mh,ceiling,wx,wy,dx,dy,textures,rotations,shades,rgb);assert(!memcmp(a,b,w*sizeof *a));pixels+=w;}
    /* Direction-specialized spans: all quadrants, fractional non-dyadic
     * increments, exact and adjacent representable map/cell boundaries,
     * stationary axes, and nonfinite coordinates/steps. Exercise CRY spans
     * rather than relying on random texture IDs to select that fast path. */
    for(unsigned i=0;i<256;i++){textures[i]=&img[0];rotations[i]=0;}
    for(unsigned k=0;k<12000;k++){
        const unsigned mw=1+(next()>>24)%64,mh=1+(next()>>24)%64,w=1+(next()>>16)%2048;
        double wx=(double)(next()%mw),wy=(double)(next()%mh);
        double dx=((double)(next()>>8)-8388608.0)/71353651.0;
        double dy=((double)(next()>>8)-8388608.0)/91376511.0;
        if(k%5==0)wx=nextafter(wx,-INFINITY);
        if(k%7==0)wy=nextafter(wy,INFINITY);
        if(k%11==0)wx=(double)mw;
        if(k%13==0)wy=(double)mh;
        if(k%17==0)dx=0.0;
        if(k%19==0)dy=-0.0;
        if(k%211==0)dx=NAN;
        if(k%223==0)dy=INFINITY;
        if(k%227==0)wx=-INFINITY;
        if(k%229==0)wy=NAN;
        avp_gpu_host_flat_row_shades(shades,(int)(next()>>24)-128);
        for(unsigned i=0;i<w;i++)a[i]=b[i]=0x12345678;
        oldrow(a,w,mw,mh,k&1u,wx,wy,dx,dy,shades);
        avp_micro_point_floor_row(b,w,map,mw,mh,k&1u,wx,wy,dx,dy,textures,rotations,shades,rgb);
        assert(!memcmp(a,b,w*sizeof *a));pixels+=w;
    }
    /* Retail 128x128 CRY textures: all 8192 texel boundaries on each axis,
     * exact and immediately neighboring doubles, plus non-dyadic rays. */
    static u16 retail_tex[128*128];
    static AvpGpuRgbImage retail_image;
    retail_image.width=128;retail_image.height=128;retail_image.cry16=retail_tex;
    for(unsigned i=0;i<128*128;i++)retail_tex[i]=(u16)(next()>>16);
    for(unsigned i=0;i<256;i++){textures[i]=&retail_image;rotations[i]=0;}
    for(unsigned cell=0;cell<64;cell++)for(unsigned texel=0;texel<128;texel++){
        const double boundary=(double)cell+(double)texel/128.0;
        const double points[3]={nextafter(boundary,-INFINITY),boundary,nextafter(boundary,INFINITY)};
        for(unsigned j=0;j<3;j++)for(unsigned axis=0;axis<2;axis++){
            const double wx=axis?31.75:points[j],wy=axis?points[j]:31.75;
            avp_gpu_host_flat_row_shades(shades,(int)(next()>>24)-128);
            for(unsigned i=0;i<17;i++)a[i]=b[i]=0x12345678;
            const double dx=axis?0.0:1.0/4096.0,dy=axis?-1.0/4096.0:0.0;
            oldrow(a,17,64,64,axis,wx,wy,dx,dy,shades);
            avp_micro_point_floor_row(b,17,map,64,64,axis,wx,wy,dx,dy,textures,rotations,shades,rgb);
            assert(!memcmp(a,b,17*sizeof *a));pixels+=17;
        }
    }
    for(unsigned k=0;k<8000;k++){
        const unsigned mw=1+(next()>>24)%64,mh=1+(next()>>24)%64,w=1+(next()>>16)%2048;
        double wx=(next()%mw)+(double)(next()>>8)/16777216.0;
        double wy=(next()%mh)+(double)(next()>>8)/16777216.0;
        double dx=((double)(next()>>8)-8388608.0)/71353651.0;
        double dy=((double)(next()>>8)-8388608.0)/91376511.0;
        if(k%13==0)dx=0.0;
        if(k%17==0)dy=-0.0;
        if(k%19==0){dx/=32.0;dy/=32.0;}
        avp_gpu_host_flat_row_shades(shades,(int)(next()>>24)-128);
        for(unsigned i=0;i<w;i++)a[i]=b[i]=0x12345678;
        oldrow(a,w,mw,mh,k&1u,wx,wy,dx,dy,shades);
        avp_micro_point_floor_row(b,w,map,mw,mh,k&1u,wx,wy,dx,dy,textures,rotations,shades,rgb);
        assert(!memcmp(a,b,w*sizeof *a));pixels+=w;
    }
    /* Long cardinal spans, both directions, signed zero, stationary rays,
     * multiple texture IDs and map exits. Include arbitrary initial fractions. */
    for(unsigned k=0;k<10000;k++){
        const unsigned mw=64,mh=64,w=1+(next()>>16)%2048;
        double wx=(next()%mw)+(double)(next()>>8)/16777216.0;
        double wy=(next()%mh)+(double)(next()>>8)/16777216.0;
        double step=((double)(next()>>8)-8388608.0)/713536510.0;
        if(k%7==0)step/=32.0;
        const double zero=k&4u?-0.0:0.0;
        double dx=k%3==0?step:zero,dy=k%3==1?step:zero;
        if(k%11==0)wx=nextafter(floor(wx),-INFINITY);
        if(k%13==0)wy=nextafter(floor(wy),INFINITY);
        avp_gpu_host_flat_row_shades(shades,(int)(next()>>24)-128);
        for(unsigned i=0;i<w;i++)a[i]=b[i]=0x12345678;
        oldrow(a,w,mw,mh,k&1u,wx,wy,dx,dy,shades);
        avp_micro_point_floor_row(b,w,map,mw,mh,k&1u,wx,wy,dx,dy,textures,rotations,shades,rgb);
        assert(!memcmp(a,b,w*sizeof *a));pixels+=w;
    }
    /* R416 mixed retail and fallback textures at 1440p and 4K widths. */
    unsigned long long initialized=0;
    for(unsigned i=0;i<256;i++){
        textures[i]=i%8<5?&retail_image:(i%8==5?&img[0]:(i%8==6?&img[1]:NULL));
        rotations[i]=i%16==0?1:0;
    }
    for(unsigned k=0;k<5000;k++){
        unsigned w=k%2?2560:3840;
        double wx=(next()%64)+(double)(next()>>8)/16777216.0;
        double wy=(next()%64)+(double)(next()>>8)/16777216.0;
        double dx=((double)(next()>>8)-8388608.0)/713536510.0;
        double dy=((double)(next()>>8)-8388608.0)/913765110.0;
        if(k%7==0)wx=nextafter(floor(wx),-INFINITY);
        if(k%11==0)wy=nextafter(floor(wy),INFINITY);
        if(k%13==0){wx=nextafter(0.0,INFINITY);dx=nextafter(0.0,INFINITY);}
        if(k%17==0){wy=nextafter(0.0,INFINITY);dy=nextafter(0.0,INFINITY);}
        avp_gpu_host_flat_row_shades(shades,(int)(next()>>24)-128);
        for(unsigned i=0;i<w;i++)a[i]=b[i]=0x12345678;
        oldrow(a,w,64,64,k&1u,wx,wy,dx,dy,shades);
        avp_micro_point_floor_row(b,w,map,64,64,k&1u,wx,wy,dx,dy,textures,rotations,shades,rgb);
        assert(!memcmp(a,b,w*sizeof *a));pixels+=w;
        /* Original renderer clears the entire row before sampling. The
         * optimized row starts dirty, including gaps and the off-map tail. */
        memset(a,0,w*sizeof *a);
        for(unsigned i=0;i<w;i++)b[i]=next();
        oldrow(a,w,64,64,k&1u,wx,wy,dx,dy,shades);
        avp_micro_point_floor_row_impl(b,w,map,64,64,k&1u,wx,wy,dx,dy,textures,rotations,shades,rgb,1);
        assert(!memcmp(a,b,w*sizeof *a));initialized+=w;
    }
    for(unsigned flags=0;flags<8;flags++){
        assert(avp_micro_floor_defer_color_clear(2560,1440,flags&1,flags&2,flags&4)==(flags==3));
        assert(!avp_micro_floor_defer_color_clear(1279,1440,flags&1,flags&2,flags&4));
        assert(!avp_micro_floor_defer_color_clear(2560,719,flags&1,flags&2,flags&4));
    }
    printf("PASS: %llu initialized floor/ceiling output pixels\n",initialized);
    printf("PASS: %u exact CRY RMW cases; %llu floor/ceiling output pixels\n",count,pixels);return 0;
}
