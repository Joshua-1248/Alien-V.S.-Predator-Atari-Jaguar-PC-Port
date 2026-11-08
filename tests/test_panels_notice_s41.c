/* S41: synthetic bounds/safety tests for isolated retail notice subset. */
#include "jaguar/avp_gpu/panels_cached.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t mem[0x200000u];
static uint32_t rd(void *ctx,uint32_t adr,int w)
{const uint8_t *p=ctx;uint32_t v=0;if(adr>sizeof mem-(unsigned)w)return 0;for(int i=0;i<w;i++)v=v<<8|p[adr+(unsigned)i];return v;}
static void wr(void *ctx,uint32_t adr,uint32_t val,int w)
{uint8_t *p=ctx;if(adr>sizeof mem-(unsigned)w)return;for(int i=w-1;i>=0;i--){p[adr+(unsigned)i]=(uint8_t)val;val>>=8;}}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"S41 fail at line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    AvpView3DBus bus={mem,rd,wr};uint32_t next=0u;
    const uint32_t list=0x10000u,texture=0x11000u,notice=0x12000u,buffer=0x13000u;
    memset(mem,0,sizeof mem);
    /* No mapped table key: retail fallback clears 256 bytes starting at
     * the cache entry (A1), NOT the published notice buffer. */
    wr(mem,AVP_JAG_PANEL_NOTICE_MAP_BASE,0xFFFFu,2);
    wr(mem,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,buffer,4);
    memset(mem+buffer,0xA5u,256u);
    memset(mem+AVP_JAG_PANEL_CACHE_BASE+3u,0xCDu,260u);
    wr(mem,AVP_JAG_PANEL_CACHE_BASE+3u,145u,1);
    wr(mem,list,3u,2);
    wr(mem,list+2u,texture,4);wr(mem,list+6u,notice,4);
    wr(mem,list+10u,145u,1);wr(mem,list+11u,0xFFu,1);
    wr(mem,list+12u,0xFFFFFFFFu,4);
    CHECK(avp_jagpanel_walk_cached_or_none_notice(&bus,NULL,list,list+16u,&next));
    CHECK(next==list+16u);
    CHECK(rd(mem,notice,4)==buffer);
    CHECK(rd(mem,texture,4)==AVP_JAG_PANEL_SLOT_BASE+3u*AVP_JAG_PANEL_SLOT_BYTES);
    CHECK(rd(mem,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,4)==buffer+256u);
    for(unsigned j=0;j<256u;j++)CHECK(mem[AVP_JAG_PANEL_CACHE_BASE+3u+j]==0u);
    for(unsigned j=0;j<256u;j++)CHECK(mem[buffer+j]==0xA5u);
    /* The old cached-only API remains deliberately fail-closed. */
    CHECK(!avp_jagpanel_walk_cached(&bus,list,list+16u,&next));
    /* A mapped ID with no authenticated original FILES metadata must fail. */
    memset(mem,0,sizeof mem);
    wr(mem,AVP_JAG_PANEL_NOTICE_MAP_BASE,145u,2);
    wr(mem,AVP_JAG_PANEL_NOTICE_MAP_BASE+2u,74u,2);
    wr(mem,list,3u,2);
    wr(mem,list+2u,texture,4);wr(mem,list+6u,notice,4);
    wr(mem,list+10u,145u,1);wr(mem,list+11u,0xFFu,1);
    wr(mem,list+12u,0xFFFFFFFFu,4);
    wr(mem,AVP_JAG_PANEL_CACHE_BASE+3u,145u,1);
    wr(mem,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,buffer,4);
    CHECK(!avp_jagpanel_walk_cached_or_none_notice(&bus,NULL,list,list+16u,&next));
    /* Forced $80 special panel must not fall into the NONE/notice branch. */
    wr(mem,list+10u,0x80u,1);
    CHECK(!avp_jagpanel_walk_cached_or_none_notice(&bus,NULL,list,list+16u,&next));
    /* Empty notice list still publishes current buffer without clearing. */
    memset(mem,0,sizeof mem);
    wr(mem,list,3u,2);wr(mem,list+2u,texture,4);
    wr(mem,list+6u,notice,4);wr(mem,list+10u,0xFFu,1);
    wr(mem,list+12u,0xFFFFFFFFu,4);
    wr(mem,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,buffer,4);
    CHECK(avp_jagpanel_walk_cached_or_none_notice(&bus,NULL,list,list+16u,&next));
    CHECK(rd(mem,notice,4)==buffer);
    CHECK(rd(mem,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,4)==buffer);
    puts("S41 isolated notice subset: pointer publishing, original A1 clear and unsupported paths PASS");
    return 0;
}
