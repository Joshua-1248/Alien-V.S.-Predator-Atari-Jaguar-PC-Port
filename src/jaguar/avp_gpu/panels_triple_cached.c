#include "jaguar/avp_gpu/panels_triple_cached.h"
#include "jaguar/avp_gpu/panels_cached.h"
#include <stdint.h>

int avp_jagpanel_triple_cached_reuse(const AvpView3DBus *bus)
{
    uint32_t index, base, first;
    if(!bus || !bus->rd || !bus->wr)return 0;
    index=bus->rd(bus->ctx,AVP_JAG_PANEL_INDEX_ADDR,2);
    /* The original $F3FE uses a sign-extended 16-bit index minus one.
     * Do not guess its wrapped/negative-cache behavior in the isolated API. */
    if(index==0u || index>0x7ffcu)return 0;
    base=AVP_JAG_PANEL_CACHE_BASE+index-1u;
    if(base+4u>0x200000u)return 0;
    first=bus->rd(bus->ctx,base,1);
    for(uint32_t j=1u;j<4u;j++)
        if(bus->rd(bus->ctx,base+j,1)!=first)return 0;
    /* Original advances the panel index by three before checking the
     * cache. Only the positive-match path is supported here. */
    bus->wr(bus->ctx,AVP_JAG_PANEL_INDEX_ADDR,(index+3u)&0xffffu,2);
    return 1;
}
