#include "jaguar/avp_gpu/tracker_brush.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    u8 ring[8u*16u*60u];
    memset(ring,0,sizeof(ring));
    ring[0]=0x80u;
    ring[7u*16u*60u+59u*16u+15u]=0x01u;
    assert(avp_gpu_tracker_ring_bit(ring,sizeof(ring),0,0,0)==1);
    assert(avp_gpu_tracker_ring_bit(ring,sizeof(ring),0,1,0)==0);
    assert(avp_gpu_tracker_ring_bit(ring,sizeof(ring),1,0,0)==0);
    assert(avp_gpu_tracker_ring_bit(ring,sizeof(ring),7,127,59)==1);
    assert(avp_gpu_tracker_ring_bit(ring,sizeof(ring)-1u,7,127,59)==0);
    assert(avp_gpu_tracker_ring_bit(ring,sizeof(ring),8,0,0)==0);
    assert(avp_gpu_tracker_ring_bit(ring,sizeof(ring),0,128,0)==0);
    assert(avp_gpu_tracker_ring_bit(ring,sizeof(ring),0,0,60)==0);
    assert(avp_gpu_tracker_ring_bit(NULL,0,0,0,0)==0);
    /* Original dot selector: abs(signed camera x) OR abs(signed camera z),
     * then mask to the eight 8-pixel BLIPS source cells. */
    assert(avp_gpu_tracker_blip_index(0,0)==0u);
    assert(avp_gpu_tracker_blip_index(5,2)==7u);
    assert(avp_gpu_tracker_blip_index(-5,-2)==7u);
    assert(avp_gpu_tracker_blip_index(8,16)==0u);
    assert(avp_gpu_tracker_blip_index(-32768,1)==1u);
    puts("Jaguar allscope ring and camera-space blip tests passed");
    return 0;
}
