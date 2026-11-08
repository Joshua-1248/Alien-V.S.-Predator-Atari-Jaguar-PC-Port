/* R280: retail CROSS ($01DF2C GPU) stage arithmetic is a packed sum of
 * masked right-shifts, not a proportional per-channel blend.  Hand-derived
 * values from the kernel table ($F03170..$F0342A); the ROM-executed check
 * is tools/audit_retail_cross_host_rgb16_parity_r234.py. */
#include "jaguar/avp_gpu/frontend_fade.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    /* Stage 10: base=new, -(new>>2&$39CF) -(new>>3&$18C7) +(old>>2..)+(old>>3..) */
    assert(avp_gpu_rgb16_scene_cross(0x0000u,0xFFFFu,10u)==(u16)(0xFFFFu-0x39CFu-0x18C7u));
    /* Stage 7: base=new=0, +(old>>1&$7BDF) +(old>>4&$0843) */
    assert(avp_gpu_rgb16_scene_cross(0xFFFFu,0x0000u,7u)==(u16)(0x7BDFu+0x0843u));
    /* Stage 1: base=old, one 1/16 term. */
    assert(avp_gpu_rgb16_scene_cross(0x0000u,0xFFFFu,1u)==0x0843u);
    /* Endpoints are exact copies. */
    assert(avp_gpu_rgb16_scene_cross(0x1234u,0xABCDu,0u)==0x1234u);
    assert(avp_gpu_rgb16_scene_cross(0x1234u,0xABCDu,16u)==0xABCDu);
    /* The proportional helper truncates differently at stage 10. */
    {
        unsigned diff=0u;
        for(unsigned a=0u;a<0x10000u;a+=0x0101u)for(unsigned b=0u;b<0x10000u;b+=0x0703u)
            diff+=avp_gpu_rgb16_scene_cross((u16)a,(u16)b,10u)!=avp_gpu_rgb16_cross((u16)a,(u16)b,10u);
        assert(diff>0u);
    }
    puts("R280 retail scene CROSS stage kernels PASS");
    return 0;
}
