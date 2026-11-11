#include "pc/pc_vr_preview.h"
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
int main(void)
{
    uint32_t x0=0,y0=0,x1=0,y1=0;
    const uint32_t img[4]={0x11u,0x22u,0x33u,0x44u};
    uint32_t out[16]={0};
    avp_pc_vr_preview_eye(10u<<16,10u<<16,0u,1,0.064f,0u,&x0,&y0);
    avp_pc_vr_preview_eye(10u<<16,10u<<16,0u,1,0.064f,1u,&x1,&y1);
    assert(x0==x1);assert(y0<10u<<16);assert(y1>10u<<16);
    assert(fabs((double)(y1-y0)/65536.0-0.064)<0.0001);
    avp_pc_vr_preview_eye(10u<<16,10u<<16,0x40000000u,1,0.064f,0u,&x0,&y0);
    avp_pc_vr_preview_eye(10u<<16,10u<<16,0x40000000u,1,0.064f,1u,&x1,&y1);
    assert(y0==y1);assert(x0<x1);
    avp_pc_vr_preview_eye(10u<<16,10u<<16,0u,1,-1.0f,0u,&x0,&y0);
    assert(x0==10u<<16 && y0==10u<<16);
    avp_pc_vr_preview_blit_eye(img,2,2,out,4,4,0u);
    assert(out[0]==0x11u&&out[1]==0x22u&&out[8]==0x33u&&out[9]==0x44u);
    assert(out[2]==0u&&out[3]==0u);
    avp_pc_vr_preview_blit_eye(img,2,2,out,4,4,1u);
    assert(out[2]==0x11u&&out[3]==0x22u&&out[10]==0x33u&&out[11]==0x44u);
    /* Symmetric off-axis projection intersects on the zero-parallax plane. */
    double l=avp_pc_vr_preview_projection_shift(0.064f,4.0f,0u);
    double r=avp_pc_vr_preview_projection_shift(0.064f,4.0f,1u);
    assert(fabs(l+r)<1e-12);
    assert(fabs(l-0.008)<1e-7);
    assert(fabs((0.032/4.0)-l)<1e-7);
    assert(avp_pc_vr_preview_projection_shift(0.064f,0.0f,0u)==0.0);
    assert(avp_pc_vr_preview_projection_shift(-1.0f,4.0f,1u)==0.0);
    assert(avp_pc_vr_preview_projection_shift(0.064f,4.0f,2u)==0.0);
    assert(avp_pc_vr_preview_look_angle(0u,0)==0u);
    assert(avp_pc_vr_preview_look_angle(0u,90)==avp_pc_vr_preview_look_angle(0u,60));
    assert(avp_pc_vr_preview_look_angle(0u,60)==0x2aaaaaabu);
    assert(avp_pc_vr_preview_look_angle(0u,-60)==0xd5555555u);
    {AvpPcVrRect v=avp_pc_vr_preview_rect(640,456,0,1);
     assert(v.x==0u&&v.y==108u&&v.w==320u&&v.h==240u);
     v=avp_pc_vr_preview_rect(640,456,1,2);
     assert(v.x==320u&&v.y==68u&&v.w==320u&&v.h==320u);
     v=avp_pc_vr_preview_rect(801,300,1,1);
     assert(v.x==401u&&v.y==0u&&v.w==400u&&v.h==300u);
     v=avp_pc_vr_preview_rect(801,300,0,1);
     assert(v.x==0u&&v.y==0u&&v.w==400u&&v.h==300u);
     v=avp_pc_vr_preview_rect(640,456,0,0);
     assert(v.x==0u&&v.y==0u&&v.w==320u&&v.h==456u);}
    {const uint32_t source[4]={0x112233u,0x445566u,0x778899u,0xaabbccu};
     uint32_t dst[8u*8u]={0};
     avp_pc_vr_preview_blit_fit(source,2,2,dst,8,8,0u,2u);
     assert(dst[0]==0u&&dst[1]==0u&&dst[1u*8u]==0u);
     assert(dst[2u*8u]==0x112233u&&dst[2u*8u+3u]==0x445566u);
     assert(dst[5u*8u]==0x778899u&&dst[5u*8u+3u]==0xaabbccu);
     avp_pc_vr_preview_blit_fit(source,2,2,dst,8,8,1u,2u);
     assert(dst[2u*8u+4u]==0x112233u&&dst[5u*8u+7u]==0xaabbccu);
     /* File export is RGB bytes, not host-endian XRGB words. */
     char path[96];snprintf(path,sizeof path,"/tmp/avp-r76-stereo-%ld.ppm",(long)getpid());
     assert(avp_pc_vr_preview_write_ppm(path,source,2,2,0,0,2,2));
     FILE *f=fopen(path,"rb");assert(f);
     char header[12];assert(fread(header,1,11,f)==11u);
     assert(!memcmp(header,"P6\n2 2\n255\n",11u));
     unsigned char rgb[12];assert(fread(rgb,1,12,f)==12u);
     assert(rgb[0]==0x11&&rgb[1]==0x22&&rgb[2]==0x33);
     assert(rgb[9]==0xaa&&rgb[10]==0xbb&&rgb[11]==0xcc);
     assert(fclose(f)==0);assert(remove(path)==0);
     assert(!avp_pc_vr_preview_write_ppm(path,source,2,2,1,0,2,2));
    }
    puts("VR stereo R76: PASS (off-axis geometry, yaw, fit, PPM, original SBS)");
    return 0;
}
