/* R315: the host flat's 256-entry shade table is byte-for-byte the same
 * table that the filtered CRY16 sampler constructed independently in R314.
 * Also verify the point/rotated texel coordinate arithmetic did not change
 * when moved into the nearest-neighbour branch. */
#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/avp_gpu/host_flat_shading.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *q,size_t n)
{(void)q;(void)n;return (u16)(((unsigned)r<<8u) | ((unsigned)g^b));}
static unsigned state=0x315d39ab;
static unsigned rand32(void)
{state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static void old_rotate(unsigned rot,int w,int h,double ux,double uy,int *tx,int *ty)
{
    *tx=(int)(ux*w);*ty=(int)(uy*h);
    if(rot && w==h){
        int oldx=*tx,oldy=*ty;
        switch(rot){
        case 1: *tx=oldy;*ty=h-1-oldx;break;
        case 2: *tx=w-1-oldx;*ty=h-1-oldy;break;
        case 3: *tx=w-1-oldy;*ty=oldx;break;
        }
    }
}
static void new_rotate(unsigned rot,int w,int h,double ux,double uy,int *tx,int *ty)
{
    int x=(int)(ux*w),y=(int)(uy*h);
    if(rot && w==h){
        const int oldx=x,oldy=y;
        switch(rot){
        case 1u:x=oldy;y=h-1-oldx;break;
        case 2u:x=w-1-oldx;y=h-1-oldy;break;
        case 3u:x=w-1-oldy;y=oldx;break;
        default:break;
        }
    }
    *tx=x;*ty=y;
}
int main(void)
{
    unsigned long long shade_checks=0,point_checks=0,pixel_checks=0;
    for(int shade=-1024;shade<=1024;shade++){
        uint8_t original[256],shared[256];
        avp_filter_make_shade_lut(original,shade);
        avp_gpu_host_flat_row_shades(shared,shade);
        assert(memcmp(original,shared,256u)==0);
        shade_checks+=256;
    }
    const int dims[][2]={{64,64},{1254,1254},{127,239}};
    for(unsigned d=0;d<3;d++){
        const int w=dims[d][0],h=dims[d][1];
        uint16_t *cry=malloc((size_t)w*h*sizeof(uint16_t));
        uint8_t *rgb=malloc((size_t)w*h*3u);
        uint32_t *cry_rgb=malloc(65536u*sizeof(uint32_t));
        assert(cry&&rgb&&cry_rgb);
        for(int i=0;i<w*h;i++)cry[i]=(uint16_t)rand32();
        for(unsigned i=0;i<65536u;i++)cry_rgb[i]=rand32()&0xffffffu;
        memset(rgb,0,(size_t)w*h*3u);
        AvpGpuRgbImage source={0};
        source.rgb=rgb;source.cry16=cry;source.width=w;source.height=h;
        for(unsigned rotation=0;rotation<4;rotation++){
            for(unsigned i=0;i<25000u;i++){
                const double ux=(double)(rand32()&0xffffu)/65536.0;
                const double uy=(double)(rand32()&0xffffu)/65536.0;
                int x0,y0,x1,y1;
                old_rotate(rotation,w,h,ux,uy,&x0,&y0);
                new_rotate(rotation,w,h,ux,uy,&x1,&y1);
                assert(x0==x1 && y0==y1);
                point_checks++;
            }
        }
        for(int sh=-130;sh<=130;sh+=65){
            uint8_t original[256],shared[256];
            avp_filter_make_shade_lut(original,sh);
            avp_gpu_host_flat_row_shades(shared,sh);
            for(unsigned taps=1;taps<=16;taps*=2){
                AvpFilterPlan plan={0u,taps,1u,0.06,0.028};
                for(unsigned i=0;i<1000;i++){
                    unsigned a=0,b=0;
                    double u=(double)(rand32()&0xffffu)/65536.0;
                    double v=(double)(rand32()&0xffffu)/65536.0;
                    uint32_t x=avp_filter_sample_prepared_shaded(&source,NULL,u,v,plan,1,sh,cry_rgb,&a,original);
                    uint32_t y=avp_filter_sample_prepared_shaded(&source,NULL,u,v,plan,1,sh,cry_rgb,&b,shared);
                    assert(x==y && a==b);
                    pixel_checks++;
                }
            }
        }
        free(cry);free(rgb);free(cry_rgb);
    }
    printf("R315: %llu shade LUT bytes, %llu nearest coordinate checks, %llu filtered pixel-equivalent checks PASS\n",
            shade_checks,point_checks,pixel_checks);
    return 0;
}
