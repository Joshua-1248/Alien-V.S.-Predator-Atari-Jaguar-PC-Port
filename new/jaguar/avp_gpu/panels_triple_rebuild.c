#include "jaguar/avp_gpu/panels_triple_rebuild.h"
#include "jaguar/avp_gpu/panels_cached.h"
#include <stdint.h>

/* Original $8DD8: bit zero of B_CMD reads back as one when idle.
 * The reference hardware executes each command synchronously. Other backends
 * must implement the same idle acknowledgment; a finite bound avoids hangs. */
static int idle(const AvpView3DBus *bus)
{
    for(unsigned n=0u;n<4096u;n++)
        if(bus->rd(bus->ctx,0xF02238u,4)&1u)return 1;
    return 0;
}
static void put(const AvpView3DBus *bus,uint32_t offset,uint32_t value)
{
    bus->wr(bus->ctx,0xF02200u+offset,value,4);
}
int avp_jagpanel_triple_rebuild(const AvpView3DBus *bus, int blitter_ready)
{
    uint32_t index, cache, source, a1, id;
    int mismatch=0;
    if(!bus||!bus->rd||!bus->wr||!blitter_ready)return 0;
    index=bus->rd(bus->ctx,AVP_JAG_PANEL_INDEX_ADDR,2);
    /* Native API deliberately excludes wrapped/negative original indexes,
     * and destination buffers that could overrun Jaguar DRAM. */
    if(index<1u||index>32u)return 0;
    cache=AVP_JAG_PANEL_CACHE_BASE+index-1u;
    id=bus->rd(bus->ctx,cache,1);
    for(unsigned i=1u;i<4u;i++)
        if(bus->rd(bus->ctx,cache+i,1)!=id){mismatch=1;break;}
    if(!mismatch||!idle(bus))return 0;
    source=AVP_JAG_PANEL_SLOT_BASE+(index-1u)*AVP_JAG_PANEL_SLOT_BYTES;
    a1=source+AVP_JAG_PANEL_SLOT_BYTES;
    /* $F3E6 / $F41C: the original increments first, then fills the next
     * three cache bytes with the oldest byte, even on a mismatch at the end. */
    bus->wr(bus->ctx,AVP_JAG_PANEL_INDEX_ADDR,(index+3u)&0xffffu,2);
    for(unsigned i=1u;i<4u;i++)bus->wr(bus->ctx,cache+i,id,1);
    /* $F43A-$F4A6: first hardware blit. Values and ordering are taken
     * directly from the retail 68000, not inferred from the image result. */
    put(bus,0x24u,source);
    put(bus,0x00u,a1);
    put(bus,0x28u,0x00013820u);
    put(bus,0x04u,0x00033820u);
    put(bus,0x30u,0u);
    put(bus,0x0cu,0x0000007fu);
    put(bus,0x34u,0x0001ff80u);
    put(bus,0x1cu,0x00010000u);
    put(bus,0x20u,0u);
    put(bus,0x10u,0xff80ffffu);
    put(bus,0x3cu,0x00800080u);
    put(bus,0x38u,0x01800601u);
    if(!idle(bus))return 0;
    /* $F4B4-$F4F2: second hardware blit. */
    a1+=AVP_JAG_PANEL_SLOT_BYTES;
    put(bus,0x00u,a1);
    put(bus,0x30u,0u);
    put(bus,0x0cu,0x007f007fu);
    put(bus,0x1cu,0x0000ffffu);
    put(bus,0x10u,0xffff0080u);
    put(bus,0x3cu,0x00800080u);
    put(bus,0x38u,0x01800601u);
    if(!idle(bus))return 0;
    /* $F500-$F53E: third hardware blit; no final wait in original. */
    a1+=AVP_JAG_PANEL_SLOT_BYTES;
    put(bus,0x00u,a1);
    put(bus,0x30u,0u);
    put(bus,0x0cu,0x007f0000u);
    put(bus,0x1cu,0xffff0000u);
    put(bus,0x10u,0x00800001u);
    put(bus,0x3cu,0x00800080u);
    put(bus,0x38u,0x01800601u);
    return 1;
}
