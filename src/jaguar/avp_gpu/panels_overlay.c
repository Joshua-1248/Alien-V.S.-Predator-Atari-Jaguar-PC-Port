#include "jaguar/avp_gpu/panels_overlay.h"
/* Exact register-writing order of retail 68000 $F34E-$F3D8.
 * All arithmetic that is .w in the source explicitly wraps at 16 bits. */
static void put(const AvpView3DBus *bus,uint32_t off,uint32_t val)
{bus->wr(bus->ctx,0xF02200u+off,val,4);}
static int idle(const AvpView3DBus *bus)
{
    for(unsigned n=0;n<4096u;n++)if(bus->rd(bus->ctx,0xF02238u,4)&1u)return 1;
    return 0;
}
int avp_jagpanel_overlay_postload(const AvpView3DBus *bus,
                                  uint8_t x,uint8_t y,uint32_t destination,
                                  int resource_ready,int blitter_ready)
{
    uint16_t width,height,px,py,remainder,step;
    uint32_t a2flags;
    if(!bus||!bus->rd||!bus->wr||!resource_ready||!blitter_ready)return 0;
    if(destination>0x1ffffcu || (destination&1u))return 0;
    width=(uint16_t)bus->rd(bus->ctx,AVP_JAG_PANEL_OVERLAY_WIDTH,2);
    height=(uint16_t)bus->rd(bus->ctx,AVP_JAG_PANEL_OVERLAY_HEIGHT,2);
    /* Bounded verifier API: cannot prove hardware safety for zero/extreme
     * dimensions or a destination outside physical DRAM. */
    if(!width||!height||width>512u||height>512u||!idle(bus))return 0;
    a2flags=0x20u|bus->rd(bus->ctx,AVP_JAG_PANEL_OVERLAY_A2_FLAGS,2);
    px=(uint16_t)((int16_t)(int8_t)x+0x40);
    py=(uint16_t)((int16_t)(int8_t)y+0x40);
    py=(uint16_t)(py-(height>>1));
    px=(uint16_t)(px-(width>>1));
    remainder=px&3u;
    step=(uint16_t)(width+(remainder?4u:0u));
    step=(uint16_t)(0u-step);
    /* Original order: A2_FLAGS, A2_BASE, A2_PIXEL, A1_FLAGS, A1_BASE,
     * A1_PIXEL, B_COUNT? Here B_COUNT is written LAST, after all steps. */
    put(bus,0x28u,a2flags);
    put(bus,0x24u,AVP_JAG_PANEL_OVERLAY_SCRATCH);
    put(bus,0x30u,0u);
    put(bus,0x04u,0x00003820u);
    put(bus,0x00u,destination);
    put(bus,0x0cu,((uint32_t)py<<16)|px);
    put(bus,0x3cu,((uint32_t)height<<16)|width);
    put(bus,0x34u,0x00010000u|step);
    put(bus,0x10u,0x00010000u|(uint16_t)(step+remainder));
    put(bus,0x38u,0x01800601u);
    return 1;
}
