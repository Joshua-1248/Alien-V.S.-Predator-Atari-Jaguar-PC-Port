/* R321: GPU-independent Vulkan WSI sizing and device policy regression. */
#include "pc/pc_vulkan_wsi_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    AvpWsiExtent requested = {1920u,1080u}, current = {UINT32_MAX,UINT32_MAX};
    AvpWsiExtent lo = {320u,200u}, hi = {3840u,2160u}, out = {0u,0u};
    assert(avp_wsi_device_rank(1u)==20u);
    assert(avp_wsi_device_rank(2u)==30u);
    assert(avp_wsi_device_rank(3u)==10u);
    assert(avp_wsi_device_rank(4u)==0u); /* CPU */
    assert(avp_wsi_device_rank(0u)==0u); /* OTHER */
    assert(avp_wsi_image_count(1u,0u)==2u); /* unlimited max */
    assert(avp_wsi_image_count(1u,1u)==1u);
    assert(avp_wsi_image_count(3u,4u)==3u);
    assert(avp_wsi_image_count(3u,2u)==0u); /* invalid caps */
    assert(avp_wsi_composite_alpha(1u)==1u);
    assert(avp_wsi_composite_alpha(4u)==4u);
    assert(avp_wsi_composite_alpha(12u)==4u);
    assert(avp_wsi_composite_alpha(0u)==0u);
    /* R381: the normal Hardware renderer must honor V-Sync Off; retain
     * universally supported FIFO if the surface exposes no alternative. */
    assert(avp_wsi_present_mode(0,(1u<<0)|(1u<<1)|(1u<<2))==0u);
    assert(avp_wsi_present_mode(0,(1u<<1)|(1u<<2))==1u);
    assert(avp_wsi_present_mode(0,(1u<<2))==2u);
    assert(avp_wsi_present_mode(1,(1u<<0)|(1u<<1)|(1u<<2))==2u);
    assert(avp_wsi_present_mode(1,(1u<<2))==2u);
    assert(avp_wsi_extent(requested,current,lo,hi,&out));
    assert(out.width==1920u&&out.height==1080u);
    current.width=1280u;current.height=720u;
    assert(avp_wsi_extent(requested,current,lo,hi,&out));
    assert(out.width==1280u&&out.height==720u);
    current.width=0u;
    assert(!avp_wsi_extent(requested,current,lo,hi,&out));
    current.width=UINT32_MAX;
    requested.width=10000u;requested.height=10000u;
    assert(avp_wsi_extent(requested,current,lo,hi,&out));
    assert(out.width==3840u&&out.height==2160u);
    requested.width=1u;requested.height=1u;
    assert(avp_wsi_extent(requested,current,lo,hi,&out));
    assert(out.width==320u&&out.height==200u);
    requested.width=0u;
    assert(!avp_wsi_extent(requested,current,lo,hi,&out));
    requested.width=1920u;
    lo.width=4096u;
    assert(!avp_wsi_extent(requested,current,lo,hi,&out));
    lo.width=320u;
    assert(!avp_wsi_extent(requested,current,lo,hi,NULL));
    for(uint32_t i=0u;i<100000u;i++){
        uint32_t min=(i%7u)+1u,max=min+(i%17u);
        uint32_t count=avp_wsi_image_count(min,max);
        assert(count>=min&&count<=max);
        uint32_t flags=((i|1u)<<1u); /* least-bit result must be supported */
        uint32_t a=avp_wsi_composite_alpha(flags);
        assert(a && (flags&a) && !(a&(a-1u)));
    }
    puts("R321 WSI policy: 100000 deterministic limit/alpha checks PASS");
    return 0;
}
