#include "pc/pc_vr_preview.h"
#include "jaguar/avp_gpu/heading_radians.h"
#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
static uint32_t clamped_position(double value)
{
    if(value<=0.0)return 0u;
    if(value>=4294967295.0)return UINT32_MAX;
    return (uint32_t)llround(value);
}
void avp_pc_vr_preview_eye(uint32_t x,uint32_t y,uint32_t heading,int continuous,
    float separation,unsigned eye,uint32_t *out_x,uint32_t *out_y)
{
    double a,off;
    if(!out_x||!out_y)return;
    *out_x=x;*out_y=y;
    if(eye>1u||!isfinite(separation)||separation<0.0f||separation>0.20f)return;
    a=avp_gpu_heading_radians(heading,continuous);
    off=((eye==0u)?-0.5:0.5)*(double)separation*65536.0;
    *out_x=clamped_position((double)x-sin(a)*off);
    *out_y=clamped_position((double)y+cos(a)*off);
}
void avp_pc_vr_preview_blit_eye(const uint32_t *src,unsigned sw,unsigned sh,
    uint32_t *dst,unsigned dw,unsigned dh,unsigned eye)
{
    unsigned from,to,w;
    if(!src||!dst||!sw||!sh||!dw||!dh||eye>1u)return;
    from=eye==0u?0u:(dw+1u)/2u;to=eye==0u?(dw+1u)/2u:dw;
    w=to-from;
    if(!w)return;
    for(unsigned y=0;y<dh;y++){
        unsigned sy=(unsigned)((uint64_t)y*sh/dh);
        for(unsigned x=0;x<w;x++){
            unsigned sx=(unsigned)((uint64_t)x*sw/w);
            dst[(size_t)y*dw+from+x]=src[(size_t)sy*sw+sx];
        }
    }
}

AvpPcVrRect avp_pc_vr_preview_rect(unsigned dw,unsigned dh,unsigned eye,unsigned aspect)
{
    AvpPcVrRect r={0,0,0,0};
    unsigned half,first;
    if(!dw||!dh||eye>1u)return r;
    first=eye==0u?0u:(dw+1u)/2u;
    half=eye==0u?(dw+1u)/2u:dw-(dw+1u)/2u;
    if(!half)return r;
    r.x=first;r.y=0;r.w=half;r.h=dh;
    if(aspect==1u||aspect==2u){
        unsigned num=aspect==1u?4u:1u,den=aspect==1u?3u:1u;
        /* Use uint64_t: a user can choose an arbitrarily large host window. */
        if((uint64_t)half*den>(uint64_t)dh*num){
            unsigned wanted=(unsigned)((uint64_t)dh*num/den);
            if(!wanted)wanted=1u;
            r.x+=(half-wanted)/2u;r.w=wanted;
        }else{
            unsigned wanted=(unsigned)((uint64_t)half*den/num);
            if(!wanted)wanted=1u;
            r.y+=(dh-wanted)/2u;r.h=wanted;
        }
    }
    return r;
}
void avp_pc_vr_preview_blit_fit(const uint32_t *src,unsigned sw,unsigned sh,
    uint32_t *dst,unsigned dw,unsigned dh,unsigned eye,unsigned aspect)
{
    AvpPcVrRect r=avp_pc_vr_preview_rect(dw,dh,eye,aspect);
    if(!src||!dst||!sw||!sh||!r.w||!r.h)return;
    for(unsigned y=0;y<r.h;y++){
        unsigned sy=(unsigned)((uint64_t)y*sh/r.h);
        for(unsigned x=0;x<r.w;x++){
            unsigned sx=(unsigned)((uint64_t)x*sw/r.w);
            dst[(size_t)(r.y+y)*dw+r.x+x]=src[(size_t)sy*sw+sx];
        }
    }
}
double avp_pc_vr_preview_projection_shift(float separation,float convergence,unsigned eye)
{
    if(eye>1u||!isfinite(separation)||separation<0.0f||separation>0.20f||
       !isfinite(convergence)||convergence<0.5f||convergence>64.0f)return 0.0;
    /* Negative eye position requires a positive ray displacement. */
    return (eye==0u?1.0:-1.0)*(double)separation/(2.0*(double)convergence);
}
uint32_t avp_pc_vr_preview_look_angle(uint32_t game_angle,int degrees)
{
    if(degrees>60)degrees=60;
    if(degrees< -60)degrees= -60;
    /* 2^32 fractional turn; unsigned addition gives well-defined wrap. */
    return game_angle+(uint32_t)llround((double)degrees*(4294967296.0/360.0));
}
int avp_pc_vr_preview_write_ppm(const char *path,const uint32_t *src,
    unsigned width,unsigned height,unsigned x,unsigned y,unsigned w,unsigned h)
{
    FILE *f;
    if(!path||!src||!width||!height||!w||!h||x>=width||y>=height||
       w>width-x||h>height-y||w>8192u||h>8192u)return 0;
    f=fopen(path,"wb");
    if(!f)return 0;
    if(fprintf(f,"P6\n%u %u\n255\n",w,h)<0){fclose(f);return 0;}
    /* A buffered RGB row avoids millions of tiny writes at 1440p/4K. */
    unsigned char *rowrgb=(unsigned char*)malloc((size_t)w*3u);
    if(!rowrgb){fclose(f);return 0;}
    for(unsigned row=0;row<h;row++){
        for(unsigned col=0;col<w;col++){
            uint32_t p=src[(size_t)(y+row)*width+x+col];
            rowrgb[(size_t)col*3u]=(unsigned char)(p>>16);
            rowrgb[(size_t)col*3u+1u]=(unsigned char)(p>>8);
            rowrgb[(size_t)col*3u+2u]=(unsigned char)p;
        }
        if(fwrite(rowrgb,3u,w,f)!=w){free(rowrgb);fclose(f);return 0;}
    }
    free(rowrgb);
    if(fclose(f)!=0)return 0;
    return 1;
}
