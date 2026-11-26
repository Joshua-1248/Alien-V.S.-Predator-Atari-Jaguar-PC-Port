/* R365 SDK-neutral proof that the actual GPU color output must equal the
 * complete original Software frame before the first Vulkan present. */
#include "pc/pc_hybrid_gpu_parity.h"
#include "pc/pc_vulkan_readback_policy.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"R365 GPU first-present parity test failed at %d: %s\n",__LINE__,#x);return 1;} }while(0)
static void make_reference(uint8_t *dst,uint32_t width,uint32_t height,
    uint32_t fmt,const AvpHybridViewport *v,const AvpHybridOverlay *a)
{
    for(uint32_t y=0u;y<height;y++)for(uint32_t x=0u;x<width;x++){
        uint32_t sx=0u,sy=0u,rgb=0u;
        if(avp_hybrid_viewport_source_pixel(v,x,y,&sx,&sy)){
            size_t i=(size_t)sy*a->width+sx;
            rgb=a->foreground[i]>>24u?a->foreground[i]&0xffffffu:
                 a->world[i]&0xffffffu;
        }
        size_t o=((size_t)y*width+x)*4u;
        dst[o+0u]=(uint8_t)(fmt==AVP_VK_READBACK_BGRA8_UNORM?rgb:rgb>>16u);
        dst[o+1u]=(uint8_t)(rgb>>8u);
        dst[o+2u]=(uint8_t)(fmt==AVP_VK_READBACK_BGRA8_UNORM?rgb>>16u:rgb);
        dst[o+3u]=255u;
    }
}
int main(void)
{
    /* R381: new first-visible image does not trigger an unconditional
     * full-screen GPU readback. Strict diagnostics still force it. */
    CHECK(avp_hybrid_gpu_reproof_for_texture_change(0,0,0)==1);
    CHECK(avp_hybrid_gpu_reproof_for_texture_change(0,1,0)==1);
    CHECK(avp_hybrid_gpu_reproof_for_texture_change(1,0,0)==0);
    CHECK(avp_hybrid_gpu_reproof_for_texture_change(1,1,0)==0);
    CHECK(avp_hybrid_gpu_reproof_for_texture_change(1,1,1)==1);
    CHECK(avp_hybrid_gpu_reproof_for_texture_change(1,0,1)==0);
    uint32_t w=11u,h=9u,m=UINT32_MAX;
    uint32_t world[4u*3u],fg[4u*3u]={0};
    uint8_t pixels[11u*9u*4u];
    AvpHybridViewport v={0};
    AvpHybridOverlay overlay={0};
    CHECK(avp_hybrid_viewport_make(w,h,7u,5u,4u,3u,&v));
    CHECK(v.x==2u&&v.y==2u);
    for(unsigned i=0;i<12u;i++)world[i]=0x00112233u+i*0x000c0103u;
    fg[0]=0xffaabbccu;fg[11]=0xffddee12u;
    overlay.world=world;overlay.foreground=fg;
    overlay.width=4u;overlay.height=3u;overlay.valid=1;
    for(unsigned format=0u;format<2u;format++){
        uint32_t f=format?AVP_VK_READBACK_BGRA8_UNORM:AVP_VK_READBACK_RGBA8_UNORM;
        make_reference(pixels,w,h,f,&v,&overlay);
        CHECK(avp_hybrid_gpu_parity_compare(pixels,sizeof(pixels),w,h,f,&v,&overlay,&m));
        CHECK(m==0u);
        pixels[((size_t)3u*w+4u)*4u+1u]^=1u;
        CHECK(!avp_hybrid_gpu_parity_compare(pixels,sizeof(pixels),w,h,f,&v,&overlay,&m));
        CHECK(m==1u);
        make_reference(pixels,w,h,f,&v,&overlay);
        pixels[3u]=0u; /* transparent letterbox is not acceptable */
        CHECK(!avp_hybrid_gpu_parity_compare(pixels,sizeof(pixels),w,h,f,&v,&overlay,&m));
        CHECK(m==1u);
        CHECK(!avp_hybrid_gpu_parity_compare(pixels,sizeof(pixels)-1u,w,h,f,&v,&overlay,&m));
        CHECK(!avp_hybrid_gpu_parity_compare(pixels,sizeof(pixels),w,h,100u,&v,&overlay,&m));
    }
    make_reference(pixels,w,h,AVP_VK_READBACK_BGRA8_UNORM,&v,&overlay);
    fg[2u]=0x80010203u;
    CHECK(!avp_hybrid_gpu_parity_compare(pixels,sizeof(pixels),w,h,
        AVP_VK_READBACK_BGRA8_UNORM,&v,&overlay,&m));
    fg[2u]=0u;
    overlay.valid=0;
    CHECK(!avp_hybrid_gpu_parity_compare(pixels,sizeof(pixels),w,h,
        AVP_VK_READBACK_BGRA8_UNORM,&v,&overlay,&m));
    overlay.valid=1;
    CHECK(!avp_hybrid_gpu_parity_compare(NULL,sizeof(pixels),w,h,
        AVP_VK_READBACK_BGRA8_UNORM,&v,&overlay,&m));
    puts("R365 first-Vulkan-present full Software parity oracle PASS");
    return 0;
}
