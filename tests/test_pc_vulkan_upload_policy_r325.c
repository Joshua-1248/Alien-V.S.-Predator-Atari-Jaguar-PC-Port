#include "pc/pc_vulkan_upload_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
static uint32_t seed=0x96b612e9u;
static uint32_t random_u32(void)
{seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void)
{
    uint32_t flags[32]={0},index=99u;
    assert(avp_vk_upload_dimensions(128,128,65536,8192));
    assert(avp_vk_upload_dimensions(1254,1254,(size_t)1254*1254*4,8192));
    assert(avp_vk_upload_dimensions(8192,8192,(size_t)8192*8192*4,8192));
    assert(!avp_vk_upload_dimensions(8193,32,(size_t)8193*32*4,16384));
    assert(!avp_vk_upload_dimensions(0,32,0,16384));
    assert(!avp_vk_upload_dimensions(64,64,64*64*4-1,8192));
    assert(!avp_vk_upload_dimensions(64,64,64*64*4,63));
    flags[0]=1u;flags[1]=2u;flags[2]=3u;
    assert(avp_vk_upload_choose_memory(7u,3u,flags,3u,&index)&&index==2u);
    assert(avp_vk_upload_choose_memory(2u,2u,flags,3u,&index)&&index==1u);
    assert(!avp_vk_upload_choose_memory(1u,2u,flags,3u,&index));
    assert(!avp_vk_upload_choose_memory(7u,3u,flags,33u,&index));
    for (uint32_t n=0;n<200000u;n++) {
        uint32_t mask=random_u32(),needed=random_u32()&7u;
        uint32_t count=random_u32()%33u;
        uint32_t expected=UINT32_MAX;
        for(uint32_t i=0;i<count;i++)flags[i]=random_u32()&7u;
        for(uint32_t i=0;i<count;i++)
            if ((mask&(1u<<i)) && (flags[i]&needed)==needed) {
                expected=i;break;
            }
        index=99u;
        int found=avp_vk_upload_choose_memory(mask,needed,flags,count,&index);
        assert(found==(expected!=UINT32_MAX));
        if(found)assert(index==expected);
        else assert(index==99u);
    }
    puts("R325 Vulkan memory and upload dimensions: 200000 deterministic cases PASS");
    return 0;
}
