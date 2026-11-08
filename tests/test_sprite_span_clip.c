/* Host-only source-texel rectangle equivalence checks. No retail assets. */
#include "jaguar/avp_gpu/sprite_sampling.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Draws the same rectangles and inverse-sampling checks as
 * software_renderer.c::draw_amp_sprite, using synthetic AAF pixels and a
 * deterministic synthetic occlusion test. Only the scan bounds differ. */
static void paint(uint32_t *pixels,int w,int h,double cx,double centre_y,
                  double scale,int optimized,int seed)
{
    const int left=-9,top=-12,ncols=18,npix=17;
    const double yoffset=(double)((seed%9)-4);
    const double projected_y=centre_y+yoffset*scale;
    int ci;
    for(ci=0;ci<ncols;ci++){
        int x0=(int)floor(cx+((double)left+(double)ci)*scale);
        int x1=(int)ceil(cx+((double)left+(double)ci+1.0)*scale);
        int sx,syi;
        if(x1<=x0)x1=x0+1;
        if(optimized && !avp_gpu_sprite_clip_span(&x0,&x1,w))continue;
        for(sx=x0;sx<x1;sx++){
            if(sx<0||sx>=w)continue; /* original inner-loop clipping */
            if(avp_gpu_sprite_sample_coord(sx,cx,scale)!=left+ci)continue;
            if((sx+17)%31==0)continue; /* opaque wall z-buffer */
            for(syi=0;syi<npix;syi++){
                int y0=(int)floor(centre_y+((double)top+(double)syi+yoffset)*scale);
                int y1=(int)ceil(centre_y+((double)top+(double)syi+1.0+yoffset)*scale);
                int yy;
                uint32_t col;
                if((ci+syi+seed)%5==0)continue; /* zero/transparent CRY16 */
                col=0xff000000u|((uint32_t)(ci+11)<<12)|((uint32_t)(syi+3)<<4)|(uint32_t)(seed&15);
                if(y1<=y0)y1=y0+1;
                if(optimized && !avp_gpu_sprite_clip_span(&y0,&y1,h))continue;
                for(yy=y0;yy<y1;yy++){
                    if(yy<0||yy>=h)continue; /* original inner-loop clipping */
                    if(avp_gpu_sprite_sample_coord(yy,projected_y,scale)!=top+syi)continue;
                    if(((sx*13+yy*7+seed)&15)==3)continue; /* per-pixel occlusion */
                    pixels[(size_t)yy*(size_t)w+(size_t)sx]=col;
                }
            }
        }
    }
}

int main(void)
{
    static const struct { int w,h; } sizes[]={
        {160,114},{320,228},{427,240},{640,480},{960,684},
        {1280,720},{1280,912},{1920,1080},{2560,1440},
        {3440,1440},{3840,2160}
    };
    static const struct { double scale,xf,yf; } scenes[]={
        {0.24,0.5,0.5},{0.5,0.5,0.5},{1.0,0.5,0.5},
        {1.5,0.5,0.5},{2.75,0.5,0.5},{8.0,0.5,0.5},
        {28.5,0.5,0.5},{52.75,0.0,0.0},
        {16.75,-0.25,0.1},{16.75,1.25,0.9},
        {3.125,0.5,-0.4},{3.125,0.5,1.4},
        {40.0,-1.2,-1.2},{40.0,2.2,2.2},
        {0.749,0.7,0.33},{1.33333333,0.31,0.69}
    };
    unsigned i,j;
    for(i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
        const int w=sizes[i].w,h=sizes[i].h;
        const size_t bytes=(size_t)w*(size_t)h*sizeof(uint32_t);
        uint32_t *reference=(uint32_t*)malloc(bytes);
        uint32_t *clipped=(uint32_t*)malloc(bytes);
        assert(reference&&clipped);
        for(j=0;j<sizeof(scenes)/sizeof(scenes[0]);j++){
            size_t p;
            const double cx=scenes[j].xf*(double)w;
            const double cy=scenes[j].yf*(double)h;
            for(p=0;p<(size_t)w*(size_t)h;p++)
                reference[p]=0x10400000u+(uint32_t)(p*79u);
            memcpy(clipped,reference,bytes);
            paint(reference,w,h,cx,cy,scenes[j].scale,0,(int)j);
            paint(clipped,w,h,cx,cy,scenes[j].scale,1,(int)j);
            assert(memcmp(reference,clipped,bytes)==0);
        }
        free(reference);free(clipped);
    }
    return 0;
}
