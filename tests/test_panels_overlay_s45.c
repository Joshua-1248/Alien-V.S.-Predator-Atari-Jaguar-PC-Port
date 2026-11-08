/* RE25-S45 ROM-free hard gates for isolated panel overlay Blitter tail. */
#include "jaguar/avp_gpu/panels_overlay.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t memory[0x200000u];
static unsigned writes,commands;
static uint32_t values[16],addresses[16];
static uint32_t rd(void *ctx,uint32_t addr,int width)
{
    uint8_t*m=ctx;uint32_t value=0u;
    if(addr==0xf02238u)return 1u;
    if(addr>=sizeof memory||(unsigned)width>sizeof memory-addr)return 0;
    for(int i=0;i<width;i++)value=(value<<8)|m[addr+(unsigned)i];
    return value;
}
static void wr(void *ctx,uint32_t addr,uint32_t value,int width)
{
    uint8_t*m=ctx;
    if(addr>=0xf02200u&&addr<0xf02300u){
        if(writes<16u){addresses[writes]=addr;values[writes]=value;}
        writes++;
        if(addr==0xf02238u&&value==0x01800601u)commands++;
        return;
    }
    if(addr>=sizeof memory||(unsigned)width>sizeof memory-addr)return;
    for(int i=width-1;i>=0;i--){m[addr+(unsigned)i]=(uint8_t)value;value>>=8;}
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"S45 overlay safety check line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    AvpView3DBus bus={memory,rd,wr};
    memset(memory,0,sizeof memory);
    wr(memory,AVP_JAG_PANEL_OVERLAY_WIDTH,128u,2);
    wr(memory,AVP_JAG_PANEL_OVERLAY_HEIGHT,128u,2);
    wr(memory,AVP_JAG_PANEL_OVERLAY_A2_FLAGS,0x3820u,2);
    CHECK(!avp_jagpanel_overlay_postload(NULL,0u,0u,0xdd000u,1,1));
    CHECK(!avp_jagpanel_overlay_postload(&bus,0u,0u,0xdd000u,0,1));
    CHECK(!avp_jagpanel_overlay_postload(&bus,0u,0u,0xdd000u,1,0));
    CHECK(!avp_jagpanel_overlay_postload(&bus,0u,0u,0x200000u,1,1));
    CHECK(!avp_jagpanel_overlay_postload(&bus,0u,0u,0xdd001u,1,1));
    CHECK(writes==0u&&commands==0u);
    wr(memory,AVP_JAG_PANEL_OVERLAY_WIDTH,0u,2);
    CHECK(!avp_jagpanel_overlay_postload(&bus,0u,0u,0xdd000u,1,1));
    wr(memory,AVP_JAG_PANEL_OVERLAY_WIDTH,513u,2);
    CHECK(!avp_jagpanel_overlay_postload(&bus,0u,0u,0xdd000u,1,1));
    wr(memory,AVP_JAG_PANEL_OVERLAY_WIDTH,128u,2);
    wr(memory,AVP_JAG_PANEL_OVERLAY_HEIGHT,0u,2);
    CHECK(!avp_jagpanel_overlay_postload(&bus,0u,0u,0xdd000u,1,1));
    wr(memory,AVP_JAG_PANEL_OVERLAY_HEIGHT,128u,2);
    CHECK(writes==0u&&commands==0u);
    CHECK(avp_jagpanel_overlay_postload(&bus,0u,0xf0u,0xdd000u,1,1));
    CHECK(writes==10u&&commands==1u);
    CHECK(addresses[0]==0xf02228u&&values[0]==0x3820u);
    CHECK(addresses[1]==0xf02224u&&values[1]==0x1fa000u);
    CHECK(addresses[4]==0xf02200u&&values[4]==0xdd000u);
    CHECK(addresses[9]==0xf02238u&&values[9]==0x01800601u);
    CHECK(values[5]==0xfff00000u); /* y=-16+64-64, x=0+64-64 */
    puts("S45 overlay postload ROM-free bounded-input/Blitter-command gates PASS");
    return 0;
}
