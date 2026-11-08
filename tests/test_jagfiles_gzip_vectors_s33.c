#include "jaguar/avp_gpu/files_gzip_vectors.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) { \
    fprintf(stderr,"S33 GZIP vector test:%d: %s\n",__LINE__,#x); \
    exit(1); } } while(0)

typedef struct Mock {
    uint8_t bytes[0x30000];
    unsigned writes, reads, calls, failures;
    uint32_t sequence[3];
    unsigned stop_after;
    uint32_t expect_input,expect_output;
} Mock;
static uint32_t rd(void *context,uint32_t address,int width)
{
    Mock *m=context;
    uint32_t value=0u;
    CHECK(width==1||width==2||width==4);
    CHECK(address<sizeof m->bytes && (unsigned)width<=sizeof m->bytes-address);
    m->reads++;
    for(int i=0;i<width;i++)value=(value<<8)|m->bytes[address+(unsigned)i];
    return value;
}
static void wr(void *context,uint32_t address,uint32_t value,int width)
{
    Mock *m=context;
    CHECK(width==4 && address<=sizeof m->bytes-4u);
    m->writes++;
    for(int i=width-1;i>=0;i--){m->bytes[address+(unsigned)i]=(uint8_t)value;value>>=8;}
}
static int invoke(void *context,uint32_t vector)
{
    Mock *m=context;
    CHECK(m->calls<3u);
    m->sequence[m->calls++]=vector;
    if(m->stop_after && m->calls==m->stop_after)return 0;
    if(vector==AVP_JAGFILES_GZIP_EXP_VECTOR){
        CHECK(rd(m,AVP_JAGFILES_GZIP_INPUT,4)==m->expect_input);
        CHECK(rd(m,AVP_JAGFILES_GZIP_OUTPUT,4)==m->expect_output);
        wr(m,AVP_JAGFILES_GZIP_RESULT,0x148u,4);
    }
    return 1;
}
int main(void)
{
    Mock m;AvpView3DBus bus={&m,rd,wr};uint32_t length;
    memset(&m,0,sizeof m);
    m.expect_input=0x815C10u;m.expect_output=0x1F800u;
    wr(&m,AVP_JAGFILES_GZIP_WORK_VARIABLE,0x01FEDCu,4);
    m.writes=0;m.reads=0;
    CHECK(avp_jagfiles_gzip_init_vectors(&bus,invoke,&m));
    CHECK(m.calls==2u && m.sequence[0]==AVP_JAGFILES_GZIP_INIT_VECTOR1);
    CHECK(m.sequence[1]==AVP_JAGFILES_GZIP_INIT_VECTOR2);
    CHECK(m.writes==1u && rd(&m,AVP_JAGFILES_GZIP_WORK_MAILBOX,4)==0x1FEDCu);
    CHECK(m.reads==2u);
    m.calls=0;
    length=0xFFFFFFFFu;
    CHECK(avp_jagfiles_gzip_expand_vector(&bus,0x815C10u,0x1F800u,invoke,&m,&length));
    CHECK(m.calls==1u && m.sequence[0]==AVP_JAGFILES_GZIP_EXP_VECTOR);
    CHECK(length==0x148u);
    CHECK(rd(&m,AVP_JAGFILES_GZIP_INPUT,4)==0x815C10u);
    CHECK(rd(&m,AVP_JAGFILES_GZIP_OUTPUT,4)==0x1F800u);
    /* Null/malformed requests must make no changes or cartridge calls. */
    m.calls=0;
    unsigned writes=m.writes;
    CHECK(!avp_jagfiles_gzip_expand_vector(&bus,0x1000000u,0x1F800u,invoke,&m,&length));
    CHECK(!avp_jagfiles_gzip_expand_vector(&bus,0x815C10u,0x1000000u,invoke,&m,&length));
    CHECK(!avp_jagfiles_gzip_expand_vector(&bus,0x815C10u,0x1F800u,NULL,&m,&length));
    CHECK(!avp_jagfiles_gzip_expand_vector(&bus,0x815C10u,0x1F800u,invoke,&m,NULL));
    CHECK(!avp_jagfiles_gzip_init_vectors(&bus,NULL,&m));
    CHECK(!avp_jagfiles_gzip_init_vectors(NULL,invoke,&m));
    CHECK(m.writes==writes && m.calls==0u);
    /* Callback errors retain earlier side effects but cannot forge success. */
    m.stop_after=1u;
    CHECK(!avp_jagfiles_gzip_init_vectors(&bus,invoke,&m));
    CHECK(m.calls==1u && m.sequence[0]==AVP_JAGFILES_GZIP_INIT_VECTOR1);
    m.calls=0;m.stop_after=2u;
    CHECK(!avp_jagfiles_gzip_init_vectors(&bus,invoke,&m));
    CHECK(m.calls==2u && m.sequence[1]==AVP_JAGFILES_GZIP_INIT_VECTOR2);
    m.calls=0;m.stop_after=1u;
    length=0xDEADBEEFu;
    CHECK(!avp_jagfiles_gzip_expand_vector(&bus,0x815C10u,0x1F800u,invoke,&m,&length));
    CHECK(m.calls==1u && length==0xDEADBEEFu);
    /* The existing S30 FILES dispatcher must invoke GZIP init once after a
     * tag change and expand on each record, using the authentic PCs. */
    memset(&m,0,sizeof m);
    m.expect_input=0x1108u;m.expect_output=0x1F800u;
    AvpJagFilesConfig cfg={0x100u,0x1000u,0x2000u,0x300u,0x220u,8u};
    AvpJagFilesPlan plan;
    AvpJagFilesGzipDispatch adapter={&bus,invoke,&m,&length};
    wr(&m,0x100u,0x1100u,4);
    wr(&m,0x1100u,0x475A4950u,4); /* GZIP */
    wr(&m,0x1104u,4u,4);
    wr(&m,0x110Cu,0u,4);
    wr(&m,0x300u,0x475A4950u,4);
    wr(&m,0x304u,AVP_JAGFILES_GZIP_INIT_PC,4);
    wr(&m,0x308u,AVP_JAGFILES_GZIP_EXP_PC,4);
    wr(&m,0x30Cu,0u,4);
    wr(&m,AVP_JAGFILES_GZIP_WORK_VARIABLE,0x1FEDCu,4);
    CHECK(avp_jagfiles_plan(&bus,&cfg,0u,&plan)==AVP_JAGFILES_OK);
    CHECK(plan.call_init==1 && plan.payload==0x1108u);
    /* Unsupported dispatch PCs fail, while the real S30 bridge must run
     * init -> expand and update its previous-codec cache after init. */
    CHECK(!avp_jagfiles_gzip_dispatch_invoke(&adapter,0x123456u,0x1108u,0x1F800u));
    CHECK(m.calls==0u);
    CHECK(avp_jagfiles_execute(&bus,&cfg,&plan,0x1F800u,
                               avp_jagfiles_gzip_dispatch_invoke,&adapter)==AVP_JAGFILES_OK);
    CHECK(m.calls==3u && m.sequence[0]==AVP_JAGFILES_GZIP_INIT_VECTOR1);
    CHECK(m.sequence[1]==AVP_JAGFILES_GZIP_INIT_VECTOR2);
    CHECK(m.sequence[2]==AVP_JAGFILES_GZIP_EXP_VECTOR);
    CHECK(rd(&m,cfg.previous_tag,4)==0x475A4950u && length==0x148u);
    m.calls=0;
    CHECK(avp_jagfiles_execute(&bus,&cfg,&plan,0x1F800u,
                               avp_jagfiles_gzip_dispatch_invoke,&adapter)==AVP_JAGFILES_OK);
    CHECK(m.calls==1u && m.sequence[0]==AVP_JAGFILES_GZIP_EXP_VECTOR);
    /* On an original initialiser failure, the FILES dispatcher must not
     * advance its successful-initialisation cache. */
    wr(&m,cfg.previous_tag,0x4A503830u,4); /* JP80 -> GZIP */
    m.calls=0;m.stop_after=1u;
    CHECK(avp_jagfiles_execute(&bus,&cfg,&plan,0x1F800u,
                               avp_jagfiles_gzip_dispatch_invoke,&adapter)==AVP_JAGFILES_INVALID);
    CHECK(m.calls==1u && rd(&m,cfg.previous_tag,4)==0x4A503830u);
    puts("RE25 S33 original GZIP vector boundary: order, mailboxes, failures PASS");
    return 0;
}
