#include "pc/pc_vulkan_descriptor_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    assert(avp_vk_descriptor_pair_dimensions(128u,128u,256u,256u));
    assert(avp_vk_descriptor_pair_dimensions(1254u,1254u,256u,256u));
    assert(!avp_vk_descriptor_pair_dimensions(0u,1254u,256u,256u));
    assert(!avp_vk_descriptor_pair_dimensions(1254u,0u,256u,256u));
    assert(!avp_vk_descriptor_pair_dimensions(1254u,1254u,128u,256u));
    assert(!avp_vk_descriptor_pair_dimensions(1254u,1254u,256u,128u));
    assert(!avp_vk_descriptor_pair_dimensions(UINT32_MAX,1254u,256u,256u));
    for (uint32_t n=1u;n<=100000u;n++) {
        uint32_t w=(n*739u)%20000u,h=(n*1283u)%20000u;
        int expected=(w>0u&&h>0u&&w<=16384u&&h<=16384u);
        assert(avp_vk_descriptor_pair_dimensions(w,h,256u,256u)==expected);
    }
    puts("R326 descriptor input policy 100,000 cases PASS");
    return 0;
}
