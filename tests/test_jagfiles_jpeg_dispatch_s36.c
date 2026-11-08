/* S36 integration: reuse the S35 bounded Jaguar mock as a fixture.
 * Its previous tests are also executed; no new ROM data is embedded. */
#define main s35_fixture_regressions
#include "test_jagfiles_jpeg_mode2_setup_s35.c"
#undef main
#include "jaguar/avp_gpu/files_jpeg_dispatch.h"
#undef CHECK
#define CHECK(x) do {if(!(x)){fprintf(stderr,"S36:%d: %s\n",__LINE__,#x);exit(1);}} while(0)

static unsigned expansions;
static int expand_ok=1;
static int expand(void *ctx,uint32_t record,uint32_t out)
{
    Mock *m=(Mock*)ctx;
    CHECK(m==&M);
    CHECK(record==FIRST+8u);
    CHECK(out==0x1C0000u);
    expansions++;
    return expand_ok;
}
static AvpJagFilesJpegDispatch jd={&bus,&cfg,&gzip,expand,&M};
static AvpJagFilesPlan plan;
static void jpeg_setup(unsigned mode,uint32_t prev,int table_loaded)
{
    setup();
    wr(&M,FIRST,JP80,4);  /* original JPxx record, replacing mock NONE */
    wr(&M,cfg.dispatch_table+12u,JP80,4);
    wr(&M,cfg.dispatch_table+16u,AVP_JAGFILES_JPEG_INIT_PC,4);
    wr(&M,cfg.dispatch_table+20u,AVP_JAGFILES_JPEG_EXP_PC,4);
    wr(&M,cfg.dispatch_table+24u,0u,4);
    wr(&M,AVP_JPEG_FRONTEND_PROGRAM,AVP_JPEG_GPU_ENTRY,4);
    wr(&M,AVP_JPEG_FRONTEND_PROGRAM+4u,18u,4);
    for(unsigned i=0;i<20u;i++)wr(&M,AVP_JPEG_FRONTEND_PROGRAM+8u+i,0x50u+i,1);
    wr(&M,AVP_JPEG_MODE_VARIABLE,mode,1);
    wr(&M,AVP_JPEG_PREVIOUS_TAG,prev,4);
    wr(&M,AVP_JPEG_CRY_LOADED,(uint32_t)table_loaded,1);
    M.calls=M.blits=M.writes=0;
    expansions=0;
    expand_ok=1;
    CHECK(avp_jagfiles_plan(&bus,&cfg,0u,&plan)==AVP_JAGFILES_OK);
    CHECK(plan.tag==JP80 && plan.init_pc==AVP_JAGFILES_JPEG_INIT_PC);
}
static void execute_jpeg(void)
{
    CHECK(avp_jagfiles_jpeg_execute(&jd,&plan,0x1C0000u)==AVP_JAGFILES_OK);
    CHECK(expansions==1u);
    CHECK(rd(&M,AVP_JPEG_PREVIOUS_TAG,4)==JP80);
}
#ifndef AVP_TEST_S36_AS_FIXTURE
int main(void)
{
    AvpJagFilesPlan fake;
    unsigned before;
    s35_fixture_regressions();

    jpeg_setup(0,GZIP,1);
    execute_jpeg();
    CHECK(M.blits==2u && M.calls==0u);
    CHECK(rd(&M,AVP_JPEG_QUALITY_VARIABLE,2)==0x3830u);
    /* This second FILES call is already cached and must not reinstall. */
    CHECK(avp_jagfiles_jpeg_execute(&jd,&plan,0x1C0000u)==AVP_JAGFILES_OK);
    CHECK(expansions==2u && M.blits==2u && M.calls==0u);

    jpeg_setup(1,GZIP,1);
    execute_jpeg();
    CHECK(M.blits==2u && rd(&M,AVP_JPEG_GPU_ENTRY,1)==0x50u);

    jpeg_setup(2,0x4A503930u,0);
    execute_jpeg();
    CHECK(M.calls==3u && M.blits==2u);
    CHECK(rd(&M,AVP_JPEG_CRY_LOADED,1)==0xFFu); /* S48: original not.b */
    CHECK(rd(&M,DEST,1)==7u);

    /* GZIP was already the active codec: nested FILES skips its init,
     * but still expands the table and reinstalls the JPEG GPU program. */
    jpeg_setup(2,GZIP,0);
    execute_jpeg();
    CHECK(M.calls==1u && M.blits==2u);

    /* Repeated JPEG tag with loaded table may bypass initialization. */
    jpeg_setup(2,JP80,1);
    execute_jpeg();
    CHECK(M.calls==0u && M.blits==0u);

    /* A different JPxx quality does not reinstall GPU code, but $BE00
     * still issues one original Blitter quant-table copy. */
    jpeg_setup(1,0x4A503930u,1);
    execute_jpeg();
    CHECK(M.blits==1u && M.calls==0u);
    CHECK(rd(&M,AVP_JPEG_QUALITY_VARIABLE,2)==0x3830u);

    /* External expansion failure must propagate without a false success. */
    jpeg_setup(0,GZIP,1);expand_ok=0;
    CHECK(avp_jagfiles_jpeg_execute(&jd,&plan,0x1C0000u)==AVP_JAGFILES_INVALID);
    CHECK(expansions==1u);

    /* The adapter refuses absent expansion engines BEFORE GPU bus writes. */
    jpeg_setup(2,GZIP,0);jd.expand=NULL;before=M.writes;
    CHECK(avp_jagfiles_jpeg_execute(&jd,&plan,0x1C0000u)==AVP_JAGFILES_INVALID);
    CHECK(M.writes==before && M.calls==0u && M.blits==0u);
    jd.expand=expand;

    /* Unverified handler PCs and stale records must never be routed. */
    fake=plan;fake.init_pc=0xDEADu;before=M.writes;
    CHECK(avp_jagfiles_jpeg_execute(&jd,&fake,0x1C0000u)==AVP_JAGFILES_INVALID);
    fake=plan;fake.expand_pc=0xBADu;
    CHECK(avp_jagfiles_jpeg_execute(&jd,&fake,0x1C0000u)==AVP_JAGFILES_INVALID);
    CHECK(M.writes==before && M.calls==0u);
    CHECK(!avp_jagfiles_jpeg_dispatch_invoke(&jd,0xFADEu,FIRST,0x1C0000u));
    CHECK(!avp_jagfiles_jpeg_dispatch_invoke(&jd,AVP_JAGFILES_JPEG_EXP_PC,FIRST+16u,0x1C0000u));
    CHECK(M.writes==before && M.calls==0u);

    /* No fraudulent success if the nested original GZIP service is absent. */
    jpeg_setup(2,GZIP,0);jd.gzip=NULL;
    CHECK(avp_jagfiles_jpeg_execute(&jd,&plan,0x1C0000u)==AVP_JAGFILES_INVALID);
    CHECK(M.calls==0u && expansions==0u && rd(&M,AVP_JPEG_CRY_LOADED,1)==0u);
    jd.gzip=&gzip;

    puts("S36 native FILES JPEG dispatch routing: modes 0/1/2, cache, nested GZIP, failure gates PASS");
    return 0;
}

#endif /* AVP_TEST_S36_AS_FIXTURE */
