#include "platform_pc_scale.h"
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* High-resolution framebuffer + stale native frontend pixels.  On PSVO the
 * PC bridge must first transfer the latest displayed world at native size,
 * before the unchanged Jaguar pause-OP snapshot is taken. */
int main(void)
{
    const unsigned sw=640,sh=456,dw=320,dh=228;
    uint32_t *world=malloc((size_t)sw*sh*4),*native=malloc((size_t)dw*dh*4);
    AvpJaguarPauseOpState state;
    assert(world&&native);
    for(unsigned y=0;y<sh;y++)for(unsigned x=0;x<sw;x++)
        world[(size_t)y*sw+x]=0x00400000u|((y&255u)<<8)|(x&255u);
    for(unsigned i=0;i<dw*dh;i++)native[i]=0x00abcdefu; /* stale MJP */
    avp_jaguar_pause_op_init(&state);
    avp_pc_scale_nearest32(world,sw,sh,native,dw,dh);
    avp_jaguar_pause_op_event(&state,0x5053564fu,0,0,0,native,dw,dh);
    memset(native,0,(size_t)dw*dh*4);
    avp_jaguar_pause_op_prepare_frame(&state,native,dw,dh);
    for(unsigned y=0;y<dh;y++)for(unsigned x=0;x<dw;x++){
        unsigned sy=y*2u,sx=x*2u;
        assert(native[(size_t)y*dw+x]==world[(size_t)sy*sw+sx]);
    }
    avp_jaguar_pause_op_event(&state,0x5052564fu,0,0,0,native,dw,dh);
    assert(!avp_jaguar_pause_op_active(&state));
    avp_jaguar_pause_op_destroy(&state);
    free(world);free(native);
    return 0;
}
