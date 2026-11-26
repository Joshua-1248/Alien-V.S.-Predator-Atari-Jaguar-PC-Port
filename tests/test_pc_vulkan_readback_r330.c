#include "pc/pc_vulkan_readback_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(uint32_t format)
{
    const uint32_t w=319u,h=257u;
    size_t n=0u;
    uint8_t *a;
    uint32_t mismatch=UINT32_MAX;
    assert(avp_vk_readback_spec(w,h,format,&n));
    assert(n==(size_t)w*h*4u);
    a=malloc(n);assert(a);
    for(uint32_t y=0u;y<h;y++) for(uint32_t x=0u;x<w;x++) {
        uint32_t v=((x^y)&1u)?0x00336699u:0x00bb8855u;
        size_t i=((size_t)y*w+x)*4u;
        a[i+0u]=(uint8_t)(v>>(format==AVP_VK_READBACK_BGRA8_UNORM?0u:16u));
        a[i+1u]=(uint8_t)(v>>8u);
        a[i+2u]=(uint8_t)(v>>(format==AVP_VK_READBACK_BGRA8_UNORM?16u:0u));
        a[i+3u]=255u;
    }
    assert(avp_vk_readback_check(a,n,w,h,format,&mismatch)&&mismatch==0u);
    a[(100u*w+28u)*4u]^=1u;
    assert(!avp_vk_readback_check(a,n,w,h,format,&mismatch)&&mismatch==1u);
    a[0u]^=4u;
    assert(!avp_vk_readback_check(a,n,w,h,format,&mismatch)&&mismatch==2u);
    assert(!avp_vk_readback_check(a,n-1u,w,h,format,&mismatch));
    free(a);
}
int main(void)
{
    size_t n=0u;
    assert(!avp_vk_readback_spec(0u,1u,AVP_VK_READBACK_BGRA8_UNORM,&n));
    assert(!avp_vk_readback_spec(8192u,8192u,AVP_VK_READBACK_BGRA8_UNORM,&n));
    assert(!avp_vk_readback_spec(1u,1u,50u,&n));
    assert(!avp_vk_readback_spec(1u,1u,AVP_VK_READBACK_BGRA8_UNORM,NULL));
    check(AVP_VK_READBACK_BGRA8_UNORM);
    check(AVP_VK_READBACK_RGBA8_UNORM);
    puts("R330 Vulkan output readback: 2 channel layouts, exact full-frame parity and mismatch detection PASS");
    return 0;
}
