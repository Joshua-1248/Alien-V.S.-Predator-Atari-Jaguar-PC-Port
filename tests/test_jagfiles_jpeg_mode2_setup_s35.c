/* S35: synthetic composition of FILES #239 and JPEG mode-2 GPU installation. */
#include "jaguar/avp_gpu/files_jpeg_mode2_setup.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"S35:%d: %s\n",__LINE__,#x);exit(1);}} while(0)
#define FIRST 0x100000u
#define DEST  0x1D0000u
#define JP80  0x4A503830u
#define GZIP  0x475A4950u
#define QUANT_SRC 0x2DDA2u
typedef struct Mock {
    uint8_t dram[0x200000u],gpu[0x2000];
    uint32_t regs[0x40],ctrl;
    uint32_t vectors[3];
    unsigned calls,blits,writes,fail_on;
    uint32_t result_size;
} Mock;
static Mock M;
static uint8_t *byte_ptr(Mock *m,uint32_t a){
    if(a<sizeof m->dram)return &m->dram[a];
    if(a>=0xF0A000u&&a<0xF0C000u)a-=0x8000u;
    if(a>=0xF02000u&&a<0xF04000u)return &m->gpu[a-0xF02000u];
    fprintf(stderr,"unexpected address: %08x\n",a);abort();
}
static uint32_t rd(void *ctx,uint32_t a,int width){
    Mock *m=ctx;uint32_t v=0;
    CHECK(width==1||width==2||width==4);
    if(a==AVP_JPEG_GPU_CTRL)return m->ctrl;
    if(a>=0xF02200u&&a<0xF02300u)return m->regs[(a-0xF02200u)/4u];
    for(int i=0;i<width;i++)v=(v<<8)|*byte_ptr(m,a+(unsigned)i);
    return v;
}
static void wr(void *ctx,uint32_t a,uint32_t v,int width){
    Mock *m=ctx;CHECK(width==1||width==2||width==4);m->writes++;
    if(a==AVP_JPEG_GPU_CTRL){CHECK(width==4);m->ctrl=v;return;}
    if(a>=0xF02200u&&a<0xF02300u){
        CHECK(width==4);m->regs[(a-0xF02200u)/4u]=v;
        if(a==0xF02238u){
            uint32_t src=m->regs[0x24u/4u]+m->regs[0x30u/4u];
            uint32_t dst=m->regs[0x00u/4u]+m->regs[0x0Cu/4u];
            uint32_t count=m->regs[0x3Cu/4u]&0xFFFFu;
            CHECK(v==(m->regs[0x0Cu/4u]<m->regs[0x30u/4u]?0x1800005u:0x1800001u));
            CHECK(count>0u&&count<=0xE00u);
            for(uint32_t i=0;i<count;i++)*byte_ptr(m,dst+i)=*byte_ptr(m,src+i);
            m->blits++;
        }
        return;
    }
    for(int i=width-1;i>=0;i--){*byte_ptr(m,a+(unsigned)i)=(uint8_t)v;v>>=8;}
}
static int service(void *ctx,uint32_t vector){
    Mock *m=ctx;
    CHECK(m->calls<3u);
    m->vectors[m->calls++]=vector;
    if(m->fail_on==m->calls)return 0;
    if(vector==AVP_JAGFILES_GZIP_INIT_VECTOR1)
        CHECK(rd(m,AVP_JAGFILES_GZIP_WORK_MAILBOX,4)==0x1FEDCu);
    if(vector==AVP_JAGFILES_GZIP_EXP_VECTOR){
        CHECK(rd(m,AVP_JAGFILES_GZIP_INPUT,4)==FIRST+239u*8u+8u);
        CHECK(rd(m,AVP_JAGFILES_GZIP_OUTPUT,4)==DEST);
        for(unsigned i=0;i<AVP_JPEG_CRY_TABLE_BYTES;i++)*byte_ptr(m,DEST+i)=(uint8_t)(i*13u+7u);
        wr(m,AVP_JAGFILES_GZIP_RESULT,m->result_size,4);
    }
    return 1;
}
static AvpView3DBus bus={&M,rd,wr};
static AvpJagFilesConfig cfg={0x300u,FIRST,FIRST+0x1000u,0x400u,AVP_JAG_FILES_PREVIOUS_TAG,8u};
static AvpJagFilesGzipDispatch gzip={&bus,service,&M,NULL};
static void setup(void){
    memset(&M,0,sizeof M);
    for(unsigned i=0;i<240u;i++){
        uint32_t a=FIRST+i*8u;
        wr(&M,a,i==239u?GZIP:0x4E4F4E45u,4);
        wr(&M,a+4u,0u,4);
    }
    wr(&M,cfg.first_pointer,FIRST,4);
    wr(&M,cfg.dispatch_table,GZIP,4);
    wr(&M,cfg.dispatch_table+4u,AVP_JAGFILES_GZIP_INIT_PC,4);
    wr(&M,cfg.dispatch_table+8u,AVP_JAGFILES_GZIP_EXP_PC,4);
    wr(&M,cfg.dispatch_table+12u,0u,4);
    wr(&M,AVP_JPEG_LEVEL_PROGRAM,AVP_JPEG_GPU_ENTRY,4);
    wr(&M,AVP_JPEG_LEVEL_PROGRAM+4u,18u,4);
    for(unsigned i=0;i<20u;i++)wr(&M,AVP_JPEG_LEVEL_PROGRAM+8u+i,0x30u+i,1);
    wr(&M,AVP_JPEG_QUALITY_LIST,0x3830u,2);
    wr(&M,AVP_JPEG_QUALITY_LIST+2u,QUANT_SRC,4);
    wr(&M,AVP_JPEG_QUALITY_LIST+6u,0u,2);
    for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++)wr(&M,QUANT_SRC+i,0x40u+i,1);
    wr(&M,AVP_JPEG_MODE_VARIABLE,2u,1);
    wr(&M,AVP_JPEG_CRY_LOADED,0u,1);
    wr(&M,AVP_JPEG_CRY_TABLE_POINTER,DEST,4);
    wr(&M,AVP_JPEG_PREVIOUS_TAG,JP80,4); /* nested GZIP must invalidate JP cache */
    wr(&M,AVP_JAGFILES_GZIP_WORK_VARIABLE,0x1FEDCu,4);
    M.result_size=AVP_JPEG_CRY_TABLE_BYTES;
    M.calls=M.writes=M.blits=0;
}
int main(void){
    unsigned before,blits;
    AvpJagFilesConfig badcfg;
    setup();
    CHECK(avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,&gzip,JP80));
    CHECK(M.calls==3u&&M.blits==2u&&M.ctrl==8u);
    CHECK(M.vectors[0]==AVP_JAGFILES_GZIP_INIT_VECTOR1&&M.vectors[1]==AVP_JAGFILES_GZIP_INIT_VECTOR2&&M.vectors[2]==AVP_JAGFILES_GZIP_EXP_VECTOR);
    CHECK(rd(&M,AVP_JPEG_CRY_LOADED,1)==0xFFu /* S48: original not.b */);
    CHECK(rd(&M,AVP_JPEG_PREVIOUS_TAG,4)==GZIP); /* outer dispatcher not called */
    CHECK(rd(&M,AVP_JPEG_QUALITY_VARIABLE,2)==0x3830u);
    CHECK(rd(&M,AVP_JPEG_GPU_MAILBOX,4)==0x8010u);
    CHECK(rd(&M,DEST,1)==7u&&rd(&M,DEST+1u,1)==20u);
    for(unsigned i=0;i<20u;i++)CHECK(rd(&M,AVP_JPEG_GPU_ENTRY+i,1)==(uint8_t)(0x30u+i));
    for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++)CHECK(rd(&M,AVP_JPEG_GPU_QUANT_TABLE+i,1)==(uint8_t)(0x40u+i));
    /* Model the outer FILES dispatcher committing the newly initialized JP tag. */
    wr(&M,AVP_JPEG_PREVIOUS_TAG,JP80,4);
    blits=M.blits;
    CHECK(avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,NULL,JP80));
    /* A DIRECT $BD92 call reruns $BE00's hardware quant-table copy even
     * for the same JP tag; the outer FILES dispatcher skips that call. */
    CHECK(M.calls==3u&&M.blits==blits+1u);
    /* Already loaded with another codec: first JPEG GPU install, no GZIP. */
    setup();wr(&M,AVP_JPEG_CRY_LOADED,1u,1);
    wr(&M,AVP_JPEG_PREVIOUS_TAG,GZIP,4);
    CHECK(avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,NULL,JP80));
    CHECK(M.calls==0u&&M.blits==2u);
    /* Already-loaded cached JP must not read irrelevant GPU program header. */
    setup();wr(&M,AVP_JPEG_CRY_LOADED,1u,1);
    wr(&M,AVP_JPEG_LEVEL_PROGRAM,0u,4);
    CHECK(avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,NULL,JP80));
    CHECK(M.calls==0u&&M.blits==1u);
    setup();M.fail_on=2u;
    CHECK(!avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,&gzip,JP80));
    CHECK(M.calls==2u&&M.blits==0u&&rd(&M,AVP_JPEG_CRY_LOADED,1)==0u);
    setup();M.result_size=AVP_JPEG_CRY_TABLE_BYTES-1u;
    CHECK(!avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,&gzip,JP80));
    CHECK(M.calls==3u&&M.blits==0u&&rd(&M,AVP_JPEG_CRY_LOADED,1)==0u);
    setup();wr(&M,AVP_JPEG_LEVEL_PROGRAM,0u,4);
    before=M.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,&gzip,JP80));
    CHECK(M.writes==before&&M.calls==0u);
    setup();before=M.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,&gzip,GZIP));
    CHECK(M.writes==before&&M.calls==0u);
    setup();wr(&M,AVP_JPEG_MODE_VARIABLE,1u,1);before=M.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,&gzip,JP80));
    CHECK(M.writes==before&&M.calls==0u);
    setup();badcfg=cfg;badcfg.previous_tag++;
    before=M.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_prepare_native(&bus,&badcfg,&gzip,JP80));
    CHECK(M.writes==before&&M.calls==0u);
    setup();before=M.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_prepare_native(&bus,&cfg,NULL,JP80));
    CHECK(M.writes==before&&M.calls==0u);
    puts("S35 JPEG mode-2 orchestration: nested load, cache transition, GPU install, cached paths, failure gates PASS");
    return 0;
}
