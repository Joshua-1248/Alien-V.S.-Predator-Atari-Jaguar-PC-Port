#include "jaguar/avp_gpu/files_dispatch.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t m[0x4000];
static unsigned init_calls,expand_calls;
static uint32_t last_record,last_dest;
static uint32_t get32(void *ctx,uint32_t a,int n)
{
    uint8_t *p=ctx;uint32_t v=0;
    if(a>sizeof m || (unsigned)n>sizeof m-a)abort();
    for(int i=0;i<n;i++)v=(v<<8)|p[a+(unsigned)i];
    return v;
}
static void put32(void *ctx,uint32_t a,uint32_t v,int n)
{
    uint8_t *p=ctx;
    if(a>sizeof m || (unsigned)n>sizeof m-a)abort();
    for(int i=n-1;i>=0;i--){p[a+(unsigned)i]=(uint8_t)v;v>>=8;}
}
static int call(void *ctx,uint32_t routine,uint32_t record,uint32_t dst)
{
    (void)ctx;last_record=record;last_dest=dst;
    if(routine==0xBD92u){init_calls++;return 1;}
    if(routine==0xBE48u || routine==0xBD88u){expand_calls++;return 1;}
    return 0;
}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"failure: line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
int main(void)
{
    AvpView3DBus bus={m,get32,put32};
    AvpJagFilesConfig cfg={0x100u,0x1000u,0x2000u,0x300u,0x220u,8u};
    AvpJagFilesPlan p,before;
    /* Three authentic-style FILES headers: 4-byte tag + size + payload;
     * each following record begins at a 4-byte boundary. */
    put32(m,0x100u,0x1100u,4);
    put32(m,0x1100u,0x4A503830u,4);put32(m,0x1104u,5u,4);
    put32(m,0x1110u,0x4A503830u,4);put32(m,0x1114u,4u,4);
    put32(m,0x111Cu,0x4E4F4E45u,4);put32(m,0x1120u,3u,4);
    put32(m,0x1128u,0u,4);
    put32(m,0x300u,0x4E4F4E45u,4);put32(m,0x304u,0u,4);put32(m,0x308u,0xBD88u,4);
    put32(m,0x30Cu,0x4A503830u,4);put32(m,0x310u,0xBD92u,4);put32(m,0x314u,0xBE48u,4);
    CHECK(avp_jagfiles_plan(&bus,&cfg,0u,&p)==AVP_JAGFILES_OK);
    CHECK(p.header==0x1100u && p.payload==0x1108u && p.call_init==1);
    CHECK(p.table_entry==0x30Cu && p.init_pc==0xBD92u && p.expand_pc==0xBE48u);
    CHECK(avp_jagfiles_execute(&bus,&cfg,&p,0x1820u,call,NULL)==AVP_JAGFILES_OK);
    CHECK(init_calls==1u && expand_calls==1u && last_record==0x1108u && last_dest==0x1820u);
    CHECK(get32(m,0x220u,4)==0x4A503830u);
    CHECK(avp_jagfiles_plan(&bus,&cfg,1u,&p)==AVP_JAGFILES_OK);
    CHECK(p.header==0x1110u && !p.call_init);
    CHECK(avp_jagfiles_execute(&bus,&cfg,&p,0x1900u,call,NULL)==AVP_JAGFILES_OK);
    CHECK(init_calls==1u && expand_calls==2u);
    CHECK(avp_jagfiles_plan(&bus,&cfg,2u,&p)==AVP_JAGFILES_OK);
    CHECK(p.header==0x111Cu && p.init_pc==0u && !p.call_init);
    CHECK(avp_jagfiles_execute(&bus,&cfg,&p,0x1900u,call,NULL)==AVP_JAGFILES_OK);
    CHECK(init_calls==1u && expand_calls==3u);
    /* NONE has no initializer, so the cached tag remains JP80. */
    CHECK(get32(m,0x220u,4)==0x4A503830u);
    CHECK(avp_jagfiles_plan(&bus,&cfg,3u,&p)==AVP_JAGFILES_NOT_FOUND);
    CHECK(avp_jagfiles_plan(&bus,&cfg,65536u,&p)==AVP_JAGFILES_INVALID);
    /* A stale plan cannot call the wrong expander. */
    CHECK(avp_jagfiles_plan(&bus,&cfg,0u,&p)==AVP_JAGFILES_OK);
    put32(m,0x314u,0xBD88u,4);
    CHECK(avp_jagfiles_execute(&bus,&cfg,&p,0x1900u,call,NULL)==AVP_JAGFILES_INVALID);
    CHECK(init_calls==1u && expand_calls==3u);
    put32(m,0x314u,0xBE48u,4);
    /* Missing dispatch entry or early zero sentinel must never fabricate
     * an expander, mutate cached state, or issue a bus write. */
    put32(m,0x300u,0u,4);
    CHECK(avp_jagfiles_plan(&bus,&cfg,0u,&p)==AVP_JAGFILES_NO_HANDLER);
    put32(m,0x300u,0x4E4F4E45u,4);
    before=p;put32(m,0x1104u,0xFFFFFFFFu,4);
    CHECK(avp_jagfiles_plan(&bus,&cfg,1u,&p)==AVP_JAGFILES_OUT_OF_RANGE);
    CHECK(memcmp(&p,&before,sizeof p)==0);
    put32(m,0x1104u,5u,4);
    cfg.archive_end=0x1108u;
    CHECK(avp_jagfiles_plan(&bus,&cfg,0u,&p)==AVP_JAGFILES_OUT_OF_RANGE);
    CHECK(avp_jagfiles_plan(&bus,&cfg,1u,&p)==AVP_JAGFILES_OUT_OF_RANGE);
    cfg.archive_end=0x2000u;
    /* Original initializer fires after a codec change, even if the caller
     * obtained its plan while the cached codec still matched. */
    CHECK(avp_jagfiles_plan(&bus,&cfg,0u,&p)==AVP_JAGFILES_OK);
    put32(m,0x220u,0x4A503730u,4);
    CHECK(avp_jagfiles_execute(&bus,&cfg,&p,0x1900u,call,NULL)==AVP_JAGFILES_OK);
    CHECK(init_calls==2u && expand_calls==4u);
    /* Native NONE expansion works from the original FILES record bytes;
     * capacity errors must not write any part of the destination. */
    CHECK(avp_jagfiles_plan(&bus,&cfg,2u,&p)==AVP_JAGFILES_OK);
    put32(m,p.payload,0x12345678u,4);
    put32(m,0x1900u,0xDEADBEEFu,4);
    uint32_t nwritten=99u;
    CHECK(avp_jagfiles_expand_none(&bus,&cfg,&p,0x1900u,0x1902u,&nwritten)==AVP_JAGFILES_OUT_OF_RANGE);
    CHECK(get32(m,0x1900u,4)==0xDEADBEEFu && nwritten==99u);
    CHECK(avp_jagfiles_expand_none(&bus,&cfg,&p,0x1900u,0x1904u,&nwritten)==AVP_JAGFILES_OK);
    CHECK(nwritten==3u && get32(m,0x1900u,4)==0x123456EFu);
    puts("S30 original FILES dispatcher synthetic: PASS");
    return 0;
}
