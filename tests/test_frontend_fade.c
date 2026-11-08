#include "jaguar/avp_gpu/frontend_fade.h"
#include <assert.h>

static u16 ref_fraction(u16 p,unsigned k)
{
    unsigned v=p,o=0;
    if(k>=16u)return p;
    if(k&8u)o+=(v>>1)&0x7bdfu;
    if(k&4u)o+=(v>>2)&0x39cfu;
    if(k&2u)o+=(v>>3)&0x18c7u;
    if(k&1u)o+=(v>>4)&0x0843u;
    return (u16)o;
}

int main(void)
{
    unsigned p,k;
    u8 r,g,b;
    /* Exhaust every RGB16 value and every one of the 17 retail handlers. */
    for(p=0;p<65536u;p++){
        for(k=0;k<=16u;k++)assert(avp_gpu_rgb16_fraction((u16)p,k)==ref_fraction((u16)p,k));
        assert(avp_gpu_rgb16_cross((u16)p,(u16)p,0u)==(u16)p);
        assert(avp_gpu_rgb16_cross((u16)p,(u16)p,16u)==(u16)p);
    }
    assert(avp_gpu_rgb16_fraction(0xffffu,0u)==0u);
    assert(avp_gpu_rgb16_fraction(0xffffu,16u)==0xffffu);
    assert(avp_gpu_rgb16_cross(0x0000u,0xffffu,16u)==0xffffu);
    assert(avp_gpu_rgb16_cross(0xffffu,0x0000u,16u)==0x0000u);

    assert(avp_gpu_rgb16_pack(255u,255u,255u)==0xffffu);
    assert(avp_gpu_rgb16_pack(248u,252u,248u)==0xffffu);
    avp_gpu_rgb16_unpack(0xffffu,&r,&g,&b);
    assert(r==248u&&g==252u&&b==248u);
    avp_gpu_rgb16_unpack(0xf800u,&r,&g,&b);assert(r==248u&&g==0u&&b==0u);
    avp_gpu_rgb16_unpack(0x07c0u,&r,&g,&b);assert(r==0u&&g==0u&&b==248u);
    avp_gpu_rgb16_unpack(0x003fu,&r,&g,&b);assert(r==0u&&g==252u&&b==0u);

    assert(avp_gpu_crossx_seed(0u)==-15);
    assert(avp_gpu_crossx_seed(40u)==-2);
    assert(avp_gpu_crossx_seed(79u)==-15);
    assert(avp_gpu_crossx_dispatch_phase(-15)==0u);
    assert(avp_gpu_crossx_dispatch_phase(0)==0u);
    assert(avp_gpu_crossx_dispatch_phase(7)==7u);
    assert(avp_gpu_crossx_dispatch_phase(8)==7u);
    assert(avp_gpu_crossx_dispatch_phase(15)==0u);
    assert(avp_gpu_crossx_dispatch_phase(16)==0u);
    assert(avp_gpu_crossx_dispatch_phase(24)==8u);
    assert(avp_gpu_crossx_dispatch_phase(32)==16u);
    assert(avp_gpu_crossx_dispatch_phase(99)==16u);
    return 0;
}
