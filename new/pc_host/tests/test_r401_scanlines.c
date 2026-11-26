#include "../include/pc/pc_scanlines.h"
#include "pc/pc_hybrid_gpu_parity.h"
#include "pc/pc_vulkan_readback_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t ref_pixel(uint32_t p,unsigned w){
    return (p&0xff000000u)|((((p>>16)&255u)*w/256u)<<16)|((((p>>8)&255u)*w/256u)<<8)|((p&255u)*w/256u);
}
/* Independent signed-coordinate reference, including negative diagonals. */
static unsigned axis_ref(AvpPcScanlines s,int coordinate,int soft){
    int t=(int)s.thickness,period=t+(int)s.spacing;
    int p=(coordinate+(int)s.phase)%period;
    if(p<0)p+=period;
    if(p>=t)return 256u;
    unsigned a=(s.strength*256u+50u)/100u;
    if(soft){int edge=p<t-1-p?p:t-1-p;unsigned peak=(unsigned)(2*((t-1)/2)+1);
        a=(a*(unsigned)(2*edge+1)+peak/2u)/peak;}
    return 256u-a;
}
static unsigned weight_ref(AvpPcScanlines s,int x,int y){
    switch(s.style){
    case 0:return axis_ref(s,y,0);case 1:return axis_ref(s,y,1);
    case 2:return axis_ref(s,x,0);case 3:return axis_ref(s,x,1);
    case 4:return axis_ref(s,x,0)*axis_ref(s,y,0)/256u;
    case 5:return axis_ref(s,x-y,0);case 6:return axis_ref(s,x-y,1);
    case 7:return axis_ref(s,x+y,0);case 8:return axis_ref(s,x+y,1);
    case 9:return axis_ref(s,x-y,0)*axis_ref(s,x+y,0)/256u;
    case 10:return axis_ref(s,x-y,1)*axis_ref(s,x+y,1)/256u;
    case 11:return axis_ref(s,x,1)*axis_ref(s,y,1)/256u;
    default:assert(0);return 256u;
    }
}
int main(void){
    enum{W=37,H=35,P=41};uint32_t input[H*P],out[H*P];
    for(unsigned i=0;i<H*P;i++)input[i]=0xab000000u|((i*0x107d19u)&0xffffffu);
    AvpPcScanlines s=avp_scanline_defaults();
    memcpy(out,input,sizeof out);avp_scanline_apply(&s,out,P,W,H);assert(!memcmp(input,out,sizeof out));
    s.enabled=1;s.strength=100;s.thickness=1;s.spacing=2;s.phase=0;
    assert(avp_scanline_weight(&s,1,0)==0&&avp_scanline_weight(&s,0,1)==256&&avp_scanline_weight(&s,0,3)==0);
    s.style=AVP_SCAN_HORIZONTAL_SOFT;s.thickness=5;s.spacing=1;
    assert(avp_scanline_weight(&s,0,0)==205&&avp_scanline_weight(&s,0,2)==0&&avp_scanline_weight(&s,0,4)==205);
    s.thickness=1;s.spacing=2;s.phase=0;s.style=AVP_SCAN_DIAGONAL_DOWN;
    assert(avp_scanline_weight(&s,0,0)==0&&avp_scanline_weight(&s,1,1)==0&&avp_scanline_weight(&s,0,1)==256);
    s.style=AVP_SCAN_DIAGONAL_UP;
    assert(avp_scanline_weight(&s,1,2)==0&&avp_scanline_weight(&s,1,1)==256);
    s.style=AVP_SCAN_DIAGONAL_GRID;
    assert(avp_scanline_weight(&s,1,1)==0&&avp_scanline_weight(&s,1,2)==0&&avp_scanline_weight(&s,0,1)==256);
    for(s.style=0;s.style<AVP_SCAN_STYLE_COUNT;s.style++)for(s.thickness=1;s.thickness<=16;s.thickness++)for(s.spacing=1;s.spacing<=16;s.spacing++)for(s.phase=0;s.phase<=31;s.phase+=7)for(s.strength=1;s.strength<=100;s.strength+=33){
      memcpy(out,input,sizeof out);avp_scanline_apply(&s,out,P,W,H);
      for(unsigned y=0;y<H;y++)for(unsigned x=0;x<P;x++)assert(out[y*P+x]==(x<W?ref_pixel(input[y*P+x],weight_ref(s,(int)x,(int)y)):input[y*P+x]));
    }
    uint32_t world[]={0x123456,0xabcdef,0xffffff,0xa0b0c0},fg[]={0,0xff999999,0,0xff112233};
    AvpHybridOverlay o={0};o.world=world;o.foreground=fg;o.width=2;o.height=2;o.valid=1;
    AvpHybridViewport v={0};assert(avp_hybrid_viewport_make(11,9,7,5,2,2,&v));
    for(unsigned fmt=0;fmt<2;fmt++)for(unsigned style=0;style<AVP_SCAN_STYLE_COUNT;style++){
       uint32_t frame[7*5];uint8_t readback[11*9*4];uint32_t m;
       s=avp_scanline_defaults();s.enabled=1;s.style=style;s.thickness=3;s.spacing=2;s.phase=1;
       for(unsigned y=0;y<5;y++)for(unsigned x=0;x<7;x++){
         unsigned i=(y*2/5)*2+x*2/7;frame[y*7+x]=fg[i]?fg[i]:world[i];}
       avp_scanline_apply(&s,frame,7,7,5);
       for(unsigned y=0;y<9;y++)for(unsigned x=0;x<11;x++){
         uint32_t p=(x>=v.x&&x<v.x+7&&y>=v.y&&y<v.y+5)?frame[(y-v.y)*7+x-v.x]:0;
         unsigned at=(y*11+x)*4;readback[at]=(uint8_t)(fmt?p:p>>16);readback[at+1]=(uint8_t)(p>>8);readback[at+2]=(uint8_t)(fmt?p>>16:p);readback[at+3]=255;}
       uint32_t format=fmt?AVP_VK_READBACK_BGRA8_UNORM:AVP_VK_READBACK_RGBA8_UNORM;
       assert(avp_hybrid_gpu_parity_compare_scanlines(readback,sizeof readback,11,9,format,&v,&o,&s,&m)&&m==0);
       readback[(v.y*11+v.x)*4+1]^=1;assert(!avp_hybrid_gpu_parity_compare_scanlines(readback,sizeof readback,11,9,format,&v,&o,&s,&m)&&m==1);
    }
    puts("R402 12 scanline styles / settings ranges / pitched output / GPU parity oracle PASS");return 0;
}
