/* Sweep 32: ROM-free first-time $BD92/8940/8C28 transaction test. */
#include "jaguar/avp_gpu/files_jpeg_init.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct Mock {
    uint8_t ram[0x40000],gpu[0x2000];
    uint32_t reg[0x40],ctrl;
    unsigned writes,blits;
} Mock;
static Mock M;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"S32 synthetic test:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t *loc(Mock *m,uint32_t a)
{
    if(a<sizeof m->ram)return &m->ram[a];
    if(a>=0xF0A000u&&a<0xF0C000u)a-=0x8000u;
    if(a>=0xF02000u&&a<0xF04000u)return &m->gpu[a-0xF02000u];
    abort();
}
static uint32_t rd(void *ctx,uint32_t a,int n)
{
    Mock *m=ctx;uint32_t v=0;
    if(a==AVP_JPEG_GPU_CTRL)return m->ctrl;
    if(a>=0xF02200u&&a<0xF02300u)return m->reg[(a-0xF02200u)/4u];
    for(int i=0;i<n;i++)v=(v<<8)|*loc(m,a+(unsigned)i);
    return v;
}
static void wr(void *ctx,uint32_t a,uint32_t v,int n)
{
    Mock *m=ctx;
    m->writes++;
    if(a==AVP_JPEG_GPU_CTRL){CHECK(n==4);m->ctrl=v;return;}
    if(a>=0xF02200u&&a<0xF02300u){
        CHECK(n==4);m->reg[(a-0xF02200u)/4u]=v;
        if(a==0xF02238u){
            uint32_t src=m->reg[0x24u/4u]+m->reg[0x30u/4u];
            uint32_t dst=m->reg[0x00u/4u]+m->reg[0x0Cu/4u];
            uint32_t len=m->reg[0x3Cu/4u]&0xFFFFu;
            CHECK(v==(m->reg[0x0Cu/4u]<m->reg[0x30u/4u]?0x1800005u:0x1800001u));
            CHECK(len>0u&&len<=0xE00u);
            for(uint32_t i=0;i<len;i++)*loc(m,dst+i)=*loc(m,src+i);
            m->blits++;
        }
        return;
    }
    for(int i=n-1;i>=0;i--){*loc(m,a+(unsigned)i)=(uint8_t)v;v>>=8;}
}
static void setup(unsigned front)
{
    const uint32_t header=front?AVP_JPEG_FRONTEND_PROGRAM:AVP_JPEG_LEVEL_PROGRAM;
    memset(&M,0,sizeof M);
    AvpView3DBus bus={&M,rd,wr};
    wr(&M,header,AVP_JPEG_GPU_ENTRY,4);
    wr(&M,header+4u,front?19u:18u,4);
    for(unsigned i=0;i<24;i++)wr(&M,header+8u+i,0x30u+i,1);
    wr(&M,AVP_JPEG_QUALITY_LIST,0x3830u,2);
    /* Exercise $8C88 (unaligned source) for the quant copy. */
    wr(&M,AVP_JPEG_QUALITY_LIST+2u,0x2DDA2u,4);
    wr(&M,AVP_JPEG_QUALITY_LIST+6u,0u,2);
    for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++)wr(&M,0x2DDA2u+i,0x40u+i,1);
    for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++)wr(&M,AVP_JPEG_DEFAULT_TABLE+i,0x90u+i,1);
    wr(&M,AVP_JPEG_PREVIOUS_TAG,0x475A4950u,4);
    wr(&M,AVP_JPEG_MODE_VARIABLE,front?1u:0u,1);
    wr(&M,AVP_JPEG_CRY_LOADED,1u,1);
    (void)bus;
}
int main(void)
{
    AvpView3DBus bus={&M,rd,wr};AvpJagFilesJpegInstall p,saved;
    for(unsigned mode=0;mode<3;mode++){
        uint32_t expected;
        setup(mode==1u);
        wr(&M,AVP_JPEG_MODE_VARIABLE,mode,1);
        expected=mode==1u?AVP_JPEG_FRONTEND_PROGRAM:AVP_JPEG_LEVEL_PROGRAM;
        unsigned start=M.writes;
        CHECK(avp_jagfiles_jpeg_install_plan(&bus,mode,&p));
        CHECK(M.writes==start);
        CHECK(p.header==expected&&p.gpu_target==AVP_JPEG_GPU_ENTRY);
        CHECK(p.rounded_length==(mode==1u?20u:20u));
        CHECK(avp_jagfiles_jpeg_init_first(&bus,0x4A503830u));
        CHECK(M.blits==2u && M.ctrl==8u);
        CHECK(rd(&M,AVP_JPEG_GPU_MAILBOX,4)==0x8010u);
        CHECK(rd(&M,AVP_JPEG_QUALITY_VARIABLE,2)==0x3830u);
        for(unsigned i=0;i<p.rounded_length;i++)CHECK(*loc(&M,AVP_JPEG_GPU_ENTRY+i)==(uint8_t)(0x30u+i));
        for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++)CHECK(*loc(&M,AVP_JPEG_GPU_QUANT_TABLE+i)==(uint8_t)(0x40u+i));
        CHECK(M.reg[0x38u/4u]==0x1800005u);
    }
    setup(0);CHECK(avp_jagfiles_jpeg_install_plan(&bus,0,&p));saved=p;
    wr(&M,AVP_JPEG_LEVEL_PROGRAM+4u,0x10000u,4);
    unsigned count=M.writes;
    CHECK(!avp_jagfiles_jpeg_install_apply(&bus,&saved));
    CHECK(!avp_jagfiles_jpeg_init_first(&bus,0x4A503830u));
    CHECK(M.writes==count);
    wr(&M,AVP_JPEG_LEVEL_PROGRAM+4u,18u,4);
    wr(&M,AVP_JPEG_LEVEL_PROGRAM,0xF02000u,4);
    count=M.writes;
    CHECK(!avp_jagfiles_jpeg_init_first(&bus,0x4A503830u)&&M.writes==count);
    wr(&M,AVP_JPEG_LEVEL_PROGRAM,AVP_JPEG_GPU_ENTRY,4);
    wr(&M,AVP_JPEG_MODE_VARIABLE,2u,1);
    wr(&M,AVP_JPEG_CRY_LOADED,0u,1);
    count=M.writes;
    CHECK(!avp_jagfiles_jpeg_init_first(&bus,0x4A503830u)&&M.writes==count);
    wr(&M,AVP_JPEG_CRY_LOADED,1u,1);
    wr(&M,AVP_JPEG_PREVIOUS_TAG,0x4A503930u,4);
    count=M.writes;
    CHECK(!avp_jagfiles_jpeg_init_first(&bus,0x4A503830u)&&M.writes==count);
    CHECK(!avp_jagfiles_jpeg_init_first(&bus,0x475A4950u)&&M.writes==count);
    puts("S32 first-time JPEG GPU loader: modes 0/1/2, Blitter sequence, no-write failures PASS");
    return 0;
}
