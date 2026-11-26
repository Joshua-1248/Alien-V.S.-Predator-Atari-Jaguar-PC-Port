#include "jaguar/avp_gpu/notice_composite.h"
#include <stdlib.h>
#include <string.h>

static void blit(AvpGpuRgbImage *dst,const AvpGpuRgbImage *src,int yo)
{
    int x0=(dst->width-src->width)/2,y0=(dst->height-src->height)/2+yo;
    if(!src||!src->rgb)return;
    for(int y=0;y<src->height;y++)for(int x=0;x<src->width;x++){
        int dx=x0+x,dy=y0+y;
        size_t di,si;
        if(dx<0||dy<0||dx>=dst->width||dy>=dst->height)continue;
        di=(size_t)dy*(size_t)dst->width+(unsigned)dx;
        si=(size_t)y*(size_t)src->width+(unsigned)x;
        memcpy(dst->rgb+3u*di,src->rgb+3u*si,3u);
        if(dst->cry16&&src->cry16)dst->cry16[di]=src->cry16[si];
    }
}
int avp_gpu_notice_composite(const AvpGpuRgbImage *base,
                             const AvpGpuRgbImage *lettering,
                             const AvpGpuRgbImage *number,
                             int yoffset,AvpGpuRgbImage *out)
{
    size_t n;
    if(!base||!lettering||!out||!base->rgb||!lettering->rgb||
       base->width!=128||base->height!=128||
       lettering->width<=0||lettering->height<=0)return 0;
    n=128u*128u;
    memset(out,0,sizeof(*out));
    out->rgb=(u8*)malloc(n*3u);
    if(!out->rgb)return 0;
    memcpy(out->rgb,base->rgb,n*3u);
    if(base->cry16){
        out->cry16=(u16*)malloc(n*sizeof(u16));
        if(!out->cry16){free(out->rgb);memset(out,0,sizeof(*out));return 0;}
        memcpy(out->cry16,base->cry16,n*sizeof(u16));
    }
    out->width=out->height=128;
    blit(out,lettering,yoffset);
    if(number&&number->rgb)blit(out,number,16);
    return 1;
}
