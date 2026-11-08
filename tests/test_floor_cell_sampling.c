/* Compare only the host floor/ceiling cell traversal optimization against
 * the preceding source implementation. Synthetic maps/pixels only. */
#include "jaguar/avp_gpu/floor_cell_sampling.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int original_cell(double x,double y,unsigned mw,unsigned mh,int *cx,int *cy)
{
    int a=(int)floor(x),b=(int)floor(y);
    if(a<0||b<0||a>=(int)mw||b>=(int)mh)return 0;
    *cx=a;*cy=b;return 1;
}

static uint32_t shade_pixel(int cellx,int celly,double wx,double wy,int ceiling)
{
    /* Synthetic maze/texel selection: preserves exact fractional inputs and
     * makes a one-cell difference unmistakable in full-frame comparison. */
    uint32_t tex=(uint32_t)(((cellx*19+celly*37+ceiling*13)&63)+1);
    unsigned tx=(unsigned)((wx-(double)cellx)*64.0);
    unsigned ty=(unsigned)((wy-(double)celly)*64.0);
    return 0x0a030001u+(tex<<18)+(tx<<8)+ty;
}

static void scene(uint32_t *out,unsigned w,unsigned h,
                  double px,double py,double angle,unsigned mw,unsigned mh,
                  int optimized)
{
    double fx=cos(angle),fy=sin(angle),rx=-fy,ry=fx;
    double focal=(double)w*0.5;
    for(unsigned y=0;y<h;y++){
        double retail_row=((double)y+0.5)*228.0/(double)h;
        int ceiling=retail_row<114.0;
        double off=fabs(retail_row-114.0);
        double perp,lefttan,step,wx,wy,wxstep,wystep;
        if(retail_row>=106.0&&retail_row<122.0)continue;
        if(off<0.5)off=0.5;
        perp=64.0/off;if(perp>8.0)perp=8.0;
        lefttan=(0.5-(double)w*0.5)/focal;step=1.0/focal;
        wx=px+(fx+rx*lefttan)*perp;
        wy=py+(fy+ry*lefttan)*perp;
        wxstep=rx*step*perp;wystep=ry*step*perp;
        for(unsigned x=0;x<w;x++,wx+=wxstep,wy+=wystep){
            int cx,cy;
            int good=optimized
                ?avp_gpu_floor_valid_cell(wx,wy,mw,mh,&cx,&cy)
                :original_cell(wx,wy,mw,mh,&cx,&cy);
            if(!good)continue;
            out[(size_t)y*w+x]=shade_pixel(cx,cy,wx,wy,ceiling);
        }
    }
}

int main(void)
{
    static const double values[]={-128.25,-64.0,-1.000001,-1.0,-0.999999,
        -0.000000001,-0.0,0.0,0.000000001,0.25,0.999999,1.0,
        1.000001,31.5,63.999999,64.0,64.000001,99.0,65535.0};
    static const struct {unsigned w,h;} sizes[]={
        {160,114},{320,228},{853,480},{1280,720},{1920,1080},
        {2560,1440},{3440,1440},{3840,2160}
    };
    static const struct {double px,py,angle;unsigned mw,mh;} cameras[]={
        {0.0,0.0,0.0,64,64},{0.25,63.75,0.55,64,64},
        {31.5,31.5,1.57,64,64},{63.9999,0.0001,3.14,64,64},
        {-0.125,64.125,2.2,64,64},{3.0,1.0,-0.47,7,5},
        {2.5,2.5,1.1,1,1},{12.5,12.5,0.0,0,0}
    };
    for(unsigned mw=0;mw<=64;mw+=(mw<2?1:31)){
        for(unsigned mh=0;mh<=64;mh+=(mh<2?1:31)){
            for(size_t i=0;i<sizeof(values)/sizeof(values[0]);i++)
            for(size_t j=0;j<sizeof(values)/sizeof(values[0]);j++){
                int a=-1,b=-1,c=-1,d=-1;
                int r=original_cell(values[i],values[j],mw,mh,&a,&b);
                int q=avp_gpu_floor_valid_cell(values[i],values[j],mw,mh,&c,&d);
                assert(r==q);
                if(q)assert(a==c&&b==d);
            }
        }
    }
    for(size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
        unsigned w=sizes[i].w,h=sizes[i].h;
        size_t n=(size_t)w*h,bytes=n*sizeof(uint32_t);
        uint32_t *base=(uint32_t*)malloc(bytes);
        uint32_t *now=(uint32_t*)malloc(bytes);
        assert(base&&now);
        /* Use both interior and out-of-map cases at every size, with
         * camera directions that cross both positive and negative axes. */
        for(size_t j=0;j<sizeof(cameras)/sizeof(cameras[0]);j++){
            for(size_t k=0;k<n;k++)base[k]=0x003f009eu+(uint32_t)(k*71u);
            memcpy(now,base,bytes);
            scene(base,w,h,cameras[j].px,cameras[j].py,cameras[j].angle,
                  cameras[j].mw,cameras[j].mh,0);
            scene(now,w,h,cameras[j].px,cameras[j].py,cameras[j].angle,
                  cameras[j].mw,cameras[j].mh,1);
            assert(memcmp(base,now,bytes)==0);
        }
        free(base);free(now);
    }
    return 0;
}
