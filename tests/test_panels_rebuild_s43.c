/* ROM-free validation: failure gates, cache mutation, exact Blitter commands. */
#include "jaguar/avp_gpu/panels_cached.h"
#include "jaguar/avp_gpu/panels_triple_rebuild.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint8_t mem[0x200000u];
static unsigned issued,register_writes;
static uint32_t rd(void *ctx,uint32_t a,int w)
{
    uint8_t *m=ctx;
    uint32_t v=0u;
    if(a==0xf02238u)return 1u;
    if(a>=0x200000u||(uint32_t)w>0x200000u-a)return 0u;
    for(int i=0;i<w;i++)v=(v<<8)|m[a+(unsigned)i];
    return v;
}
static void wr(void *ctx,uint32_t a,uint32_t v,int w)
{
    uint8_t *m=ctx;
    if(a>=0xf02200u&&a<0xf02300u){
        register_writes++;
        if(a==0xf02238u&&v==0x01800601u)issued++;
        return;
    }
    if(a>=0x200000u||(uint32_t)w>0x200000u-a)return;
    for(int i=w-1;i>=0;i--){m[a+(unsigned)i]=(uint8_t)v;v>>=8;}
}
#define CHECK(cond) do{if(!(cond)){fprintf(stderr,"S43 safety failure @%u: %s\n",__LINE__,#cond);return 1;}}while(0)
int main(void)
{
    AvpView3DBus b={mem,rd,wr};
    memset(mem,0,sizeof mem);
    wr(mem,AVP_JAG_PANEL_INDEX_ADDR,1u,2);
    for(unsigned i=0;i<4u;i++)wr(mem,AVP_JAG_PANEL_CACHE_BASE+i,0x21u,1);
    CHECK(!avp_jagpanel_triple_rebuild(&b,1));
    CHECK(!avp_jagpanel_triple_rebuild(&b,0));
    CHECK(!avp_jagpanel_triple_rebuild(NULL,1));
    CHECK(register_writes==0u&&issued==0u);
    wr(mem,AVP_JAG_PANEL_CACHE_BASE+2u,0x55u,1);
    CHECK(!avp_jagpanel_triple_rebuild(&b,0));
    CHECK(register_writes==0u&&issued==0u);
    CHECK(avp_jagpanel_triple_rebuild(&b,1));
    CHECK(register_writes==26u&&issued==3u);
    CHECK(rd(mem,AVP_JAG_PANEL_INDEX_ADDR,2)==4u);
    for(unsigned i=0;i<4u;i++)CHECK(rd(mem,AVP_JAG_PANEL_CACHE_BASE+i,1)==0x21u);
    wr(mem,AVP_JAG_PANEL_INDEX_ADDR,0u,2);
    CHECK(!avp_jagpanel_triple_rebuild(&b,1));
    wr(mem,AVP_JAG_PANEL_INDEX_ADDR,33u,2);
    CHECK(!avp_jagpanel_triple_rebuild(&b,1));
    CHECK(register_writes==26u);
    puts("S43 cache-miss Blitter bounded-input and register-command safety PASS");
    return 0;
}
