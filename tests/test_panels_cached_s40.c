/* S40: no proprietary resources. Refuse unsupported Jaguar panel paths. */
#include "jaguar/avp_gpu/panels_cached.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t mem[0x200000u];
static uint32_t rd(void *ctx,uint32_t a,int w)
{
    const uint8_t *p=ctx;
    uint32_t v=0;
    if(a>sizeof mem-(unsigned)w)return 0;
    for(int i=0;i<w;i++)v=v<<8|p[a+(unsigned)i];
    return v;
}
static void wr(void *ctx,uint32_t a,uint32_t v,int w)
{
    uint8_t *p=ctx;
    if(a>sizeof mem-(unsigned)w)return;
    for(int i=w-1;i>=0;i--){p[a+(unsigned)i]=(uint8_t)v;v>>=8;}
}
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"failure at %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    AvpView3DBus bus={mem,rd,wr};
    uint32_t next=0u;
    const uint32_t list=0x10000u,ptr=0x11000u;
    REQUIRE(!avp_jagpanel_walk_cached(NULL,list,list+16u,&next));
    REQUIRE(!avp_jagpanel_walk_cached(&bus,list,list+5u,&next));
    /* Original $80 forces the special notice loader even if already cached.
     * The modeled early side effects still update the index/cache/pointer. */
    wr(mem,list,0,2);wr(mem,list+2u,ptr,4);wr(mem,list+6u,0,4);
    wr(mem,list+10u,0x80u,1);wr(mem,list+11u,0xFFu,1);
    wr(mem,list+12u,0xFFFFFFFFu,4);
    wr(mem,AVP_JAG_PANEL_CACHE_BASE,0x80u,1);
    REQUIRE(!avp_jagpanel_walk_cached(&bus,list,list+16u,&next));
    REQUIRE(rd(mem,ptr,4)==AVP_JAG_PANEL_SLOT_BASE);
    REQUIRE(rd(mem,AVP_JAG_PANEL_INDEX_ADDR,2)==1u);
    REQUIRE(rd(mem,AVP_JAG_PANEL_CACHE_BASE,1)==0x80u);
    REQUIRE(rd(mem,AVP_JAG_PANEL_SLOT_BASE,4)==0u);
    /* An ordinary uncached path is also rejected by the cache-only entry;
     * never guess a compressed resource's codec or payload. */
    memset(mem,0,sizeof mem);
    wr(mem,list,0,2);wr(mem,list+2u,ptr,4);wr(mem,list+6u,0,4);
    wr(mem,list+10u,74u,1);wr(mem,list+11u,0xFFu,1);
    wr(mem,list+12u,0xFFFFFFFFu,4);
    REQUIRE(!avp_jagpanel_walk_cached(&bus,list,list+16u,&next));
    REQUIRE(rd(mem,AVP_JAG_PANEL_CACHE_BASE,1)==74u);
    REQUIRE(rd(mem,AVP_JAG_PANEL_SLOT_BASE,4)==0u);
    /* Test-only rejection of a notice overlay list. */
    memset(mem,0,sizeof mem);
    wr(mem,list,0,2);wr(mem,list+2u,ptr,4);wr(mem,list+6u,0x12000u,4);
    wr(mem,list+10u,0xFFu,1);wr(mem,list+12u,0xFFFFFFFFu,4);
    REQUIRE(!avp_jagpanel_walk_cached(&bus,list,list+16u,&next));
    puts("S40 panel cached-only: original special/uncached/notice paths safely gated");
    return 0;
}
