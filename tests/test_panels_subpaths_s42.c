/* S42: ROM-free safety checks for the isolated special-panel descriptor
 * lookup and triple-slot reuse. No fake JPEG/GZIP/Blitter implementations. */
#include "jaguar/avp_gpu/panels_special_lookup.h"
#include "jaguar/avp_gpu/panels_triple_cached.h"
#include "jaguar/avp_gpu/panels_cached.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t mem[0x200000u];
static uint32_t rd(void *ctx,uint32_t a,int w)
{const uint8_t *p=ctx;uint32_t v=0;if(a>sizeof mem-(unsigned)w)return 0;for(int i=0;i<w;i++)v=(v<<8)|p[a+(unsigned)i];return v;}
static void wr(void *ctx,uint32_t a,uint32_t v,int w)
{uint8_t *p=ctx;if(a>sizeof mem-(unsigned)w)return;for(int i=w-1;i>=0;i--){p[a+(unsigned)i]=(uint8_t)v;v>>=8;}}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"S42 safety test failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    AvpView3DBus b={mem,rd,wr};
    AvpJagPanelSpecialLookup out;
    uint8_t snapshot[16u];
    memset(mem,0,sizeof mem);
    for(uint32_t id=0x80u;id<=0x8cu;id++){
        uint32_t a=AVP_JAG_PANEL_SPECIAL_TABLE+4u*(id&0x7fu);
        wr(mem,a,16u,1);wr(mem,a+1u,id,1);
        wr(mem,a+2u,id+1u,1);wr(mem,a+3u,id+2u,1);
    }
    for(unsigned i=0;i<6u;i++)wr(mem,AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE+i,0x30u+i,1);
    wr(mem,AVP_JAG_PANEL_SPECIAL_INDEX,1u,2);
    memcpy(snapshot,mem+AVP_JAG_PANEL_SPECIAL_TABLE,sizeof snapshot);
    CHECK(avp_jagpanel_special_lookup(&b,0x80u,2u,&out));
    CHECK(out.file_index==16u&&out.primary_descriptor_address==AVP_JAG_PANEL_SPECIAL_TABLE+1u);
    CHECK(out.primary_descriptor[0]==0x80u&&out.primary_descriptor[1]==0x81u&&out.primary_descriptor[2]==0x82u);
    CHECK(out.secondary_descriptor_address==AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE);
    CHECK(out.secondary_descriptor[0]==0x30u&&out.secondary_descriptor[2]==0x32u);
    wr(mem,AVP_JAG_PANEL_SPECIAL_INDEX,2u,2);
    CHECK(avp_jagpanel_special_lookup(&b,0x80u,2u,&out));
    CHECK(out.secondary_descriptor_address==AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE+3u);
    CHECK(out.secondary_descriptor[0]==0x33u&&out.secondary_descriptor[2]==0x35u);
    wr(mem,AVP_JAG_PANEL_SPECIAL_INDEX,0u,2);
    CHECK(!avp_jagpanel_special_lookup(&b,0x80u,2u,&out));
    wr(mem,AVP_JAG_PANEL_SPECIAL_INDEX,3u,2);
    CHECK(!avp_jagpanel_special_lookup(&b,0x80u,2u,&out));
    for(unsigned id=0x81u;id<=0x8cu;id++){
        CHECK(avp_jagpanel_special_lookup(&b,id,0u,&out));
        CHECK(!out.secondary_descriptor_address);
        CHECK(out.primary_descriptor[0]==id);
    }
    CHECK(!avp_jagpanel_special_lookup(&b,0x7fu,2u,&out));
    CHECK(!avp_jagpanel_special_lookup(&b,0x8du,2u,&out));
    CHECK(!avp_jagpanel_special_lookup(&b,0x100u,2u,&out));
    CHECK(!avp_jagpanel_special_lookup(NULL,0x80u,2u,&out));
    CHECK(!avp_jagpanel_special_lookup(&b,0x80u,2u,NULL));
    CHECK(memcmp(snapshot,mem+AVP_JAG_PANEL_SPECIAL_TABLE,sizeof snapshot)==0);
    /* Successful triple-cache hit changes only the original index word. */
    wr(mem,AVP_JAG_PANEL_INDEX_ADDR,32u,2);
    for(unsigned i=0;i<4u;i++)wr(mem,AVP_JAG_PANEL_CACHE_BASE+31u+i,0xa5u,1);
    CHECK(avp_jagpanel_triple_cached_reuse(&b));
    CHECK(rd(mem,AVP_JAG_PANEL_INDEX_ADDR,2)==35u);
    for(unsigned i=0;i<4u;i++)CHECK(rd(mem,AVP_JAG_PANEL_CACHE_BASE+31u+i,1)==0xa5u);
    /* Mismatch is fail-closed, not a fake Blitter implementation. */
    wr(mem,AVP_JAG_PANEL_INDEX_ADDR,32u,2);
    wr(mem,AVP_JAG_PANEL_CACHE_BASE+34u,0xffu,1);
    CHECK(!avp_jagpanel_triple_cached_reuse(&b));
    CHECK(rd(mem,AVP_JAG_PANEL_INDEX_ADDR,2)==32u);
    wr(mem,AVP_JAG_PANEL_INDEX_ADDR,0u,2);
    CHECK(!avp_jagpanel_triple_cached_reuse(&b));
    CHECK(!avp_jagpanel_triple_cached_reuse(NULL));
    puts("S42 special panel lookup/secondary bounds and triple cache fail-closed safety PASS");
    return 0;
}
