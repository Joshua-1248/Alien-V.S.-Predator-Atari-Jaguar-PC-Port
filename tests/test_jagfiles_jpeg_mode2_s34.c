/* RE25-S34: ROM-free mode-2 nested FILES #239 GZIP boundary test. */
#include "jaguar/avp_gpu/files_jpeg_mode2.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"S34 mode-2:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define FIRST 0x100000u
#define DEST 0x1D0000u
#define TAG_GZIP 0x475A4950u
#define TAG_JP80 0x4A503830u
typedef struct Mock {
    uint8_t mem[0x200000u];
    unsigned calls,writes,fail_on;
    uint32_t seq[3],result_len;
} Mock;
static Mock m;
static uint32_t rd(void *p,uint32_t a,int width)
{
    Mock *x=p;uint32_t v=0;
    CHECK((width==1||width==2||width==4)&&a<sizeof x->mem&&(unsigned)width<=sizeof x->mem-a);
    for(int i=0;i<width;i++)v=(v<<8)|x->mem[a+(unsigned)i];
    return v;
}
static void wr(void *p,uint32_t a,uint32_t v,int width)
{
    Mock *x=p;
    CHECK((width==1||width==2||width==4)&&a<sizeof x->mem&&(unsigned)width<=sizeof x->mem-a);
    x->writes++;
    for(int i=width-1;i>=0;i--){x->mem[a+(unsigned)i]=(uint8_t)v;v>>=8;}
}
static int vector(void *p,uint32_t address)
{
    Mock *x=p;
    CHECK(x->calls<3u);
    x->seq[x->calls++]=address;
    if(x->fail_on==x->calls)return 0;
    if(address==AVP_JAGFILES_GZIP_INIT_VECTOR1){
        CHECK(rd(x,AVP_JAGFILES_GZIP_WORK_MAILBOX,4)==0x1FEDCu);
    }else if(address==AVP_JAGFILES_GZIP_EXP_VECTOR){
        CHECK(rd(x,AVP_JAGFILES_GZIP_INPUT,4)==FIRST+239u*8u+8u);
        CHECK(rd(x,AVP_JAGFILES_GZIP_OUTPUT,4)==DEST);
        for(unsigned i=0;i<AVP_JPEG_CRY_TABLE_BYTES;i++)x->mem[DEST+i]=(uint8_t)(i*13u+7u);
        wr(x,AVP_JAGFILES_GZIP_RESULT,x->result_len,4);
    }
    return 1;
}
static AvpView3DBus bus={&m,rd,wr};
static AvpJagFilesConfig config={0x300u,FIRST,FIRST+0x1000u,0x400u,AVP_JAG_FILES_PREVIOUS_TAG,8u};
static AvpJagFilesGzipDispatch adapter={&bus,vector,&m,NULL};
static void setup(void)
{
    memset(&m,0,sizeof m);
    for(unsigned i=0;i<240u;i++){
        uint32_t a=FIRST+i*8u;
        wr(&m,a,i==239u?TAG_GZIP:0x4E4F4E45u,4);
        wr(&m,a+4u,0u,4);
    }
    wr(&m,FIRST+240u*8u,0u,4);
    wr(&m,config.first_pointer,FIRST,4);
    wr(&m,config.dispatch_table,TAG_GZIP,4);
    wr(&m,config.dispatch_table+4u,AVP_JAGFILES_GZIP_INIT_PC,4);
    wr(&m,config.dispatch_table+8u,AVP_JAGFILES_GZIP_EXP_PC,4);
    wr(&m,config.dispatch_table+12u,0u,4);
    wr(&m,AVP_JPEG_CRY_TABLE_POINTER,DEST,4);
    wr(&m,AVP_JPEG_CRY_LOADED,0u,1);
    wr(&m,AVP_JPEG_MODE_VARIABLE,2u,1);
    wr(&m,AVP_JAGFILES_GZIP_WORK_VARIABLE,0x1FEDCu,4);
    wr(&m,config.previous_tag,TAG_JP80,4);
    m.result_len=AVP_JPEG_CRY_TABLE_BYTES;
    m.calls=0;m.writes=0;
}
int main(void)
{
    unsigned before;
    setup();
    CHECK(avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==3u);
    CHECK(m.seq[0]==AVP_JAGFILES_GZIP_INIT_VECTOR1);
    CHECK(m.seq[1]==AVP_JAGFILES_GZIP_INIT_VECTOR2);
    CHECK(m.seq[2]==AVP_JAGFILES_GZIP_EXP_VECTOR);
    CHECK(rd(&m,AVP_JPEG_CRY_LOADED,1)==0xFFu); /* S48: original not.b */
    CHECK(rd(&m,config.previous_tag,4)==TAG_GZIP);
    CHECK(m.mem[DEST]==7u && m.mem[DEST+1u]==20u);
    CHECK(m.mem[DEST+AVP_JPEG_CRY_TABLE_BYTES-1u]==(uint8_t)((AVP_JPEG_CRY_TABLE_BYTES-1u)*13u+7u));
    before=m.writes;
    CHECK(avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==3u&&m.writes==before);
    setup();m.fail_on=1u;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==1u && rd(&m,AVP_JPEG_CRY_LOADED,1)==0u);
    CHECK(rd(&m,config.previous_tag,4)==TAG_JP80);
    setup();m.fail_on=3u;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==3u && rd(&m,AVP_JPEG_CRY_LOADED,1)==0u);
    CHECK(rd(&m,config.previous_tag,4)==TAG_GZIP);
    setup();m.result_len=AVP_JPEG_CRY_TABLE_BYTES-2u;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==3u&&rd(&m,AVP_JPEG_CRY_LOADED,1)==0u);
    setup();wr(&m,FIRST+239u*8u,0x4E4F4E45u,4);
    before=m.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==0u&&m.writes==before);
    setup();wr(&m,config.dispatch_table+8u,0x123456u,4);
    before=m.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==0u&&m.writes==before);
    setup();wr(&m,AVP_JPEG_CRY_TABLE_POINTER,0x1F0002u,4);
    before=m.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==0u&&m.writes==before);
    setup();wr(&m,AVP_JPEG_CRY_TABLE_POINTER,DEST+1u,4);
    before=m.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==0u&&m.writes==before);
    setup();wr(&m,AVP_JPEG_MODE_VARIABLE,1u,1);
    before=m.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,&adapter));
    CHECK(m.calls==0u&&m.writes==before);
    setup();before=m.writes;
    CHECK(!avp_jagfiles_jpeg_mode2_load_cry(&bus,&config,NULL));
    CHECK(m.calls==0u&&m.writes==before);
    puts("RE25 S34 FILES #239 CRY table: load, cache, idempotency, failure gates PASS");
    return 0;
}
