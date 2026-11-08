/* S37: exercise the actual S36 dispatcher with an S37 native expansion
 * callback. The GPU decoder itself is represented by a stub in this test;
 * separate S25/S26 parity fixtures exercise the actual implementation. */
#define AVP_TEST_S36_AS_FIXTURE 1
#include "test_jagfiles_jpeg_dispatch_s36.c"
#include "jaguar/avp_gpu/files_jpeg_native_expand.h"
#undef CHECK
#define CHECK(x) do {if(!(x)){fprintf(stderr,"S37:%d: %s\n",__LINE__,#x);exit(1);}} while(0)
static unsigned actual_decodes;
static unsigned actual_mode;
static uint32_t returned_size;
static int completed=1;

/* Intentionally linked in place of the real JagPEG implementation; verifies
 * dispatch, boundaries, and completion independently of real image bits. */
uint32_t avp_jagpeg_exp(const AvpView3DBus *b,uint32_t payload,uint32_t out,
                       AvpJagpegJob *job,AvpJagpegConvert *cv)
{
    uint32_t x=b->rd(b->ctx,payload+4u,2),y=b->rd(b->ctx,payload+6u,2);
    unsigned mode=b->rd(b->ctx,AVP_JPG_VAR_MODE,1);
    actual_decodes++;
    CHECK(mode==actual_mode);
    CHECK(payload==FIRST+8u);
    CHECK(out==0x1C0000u);
    job->record=payload;job->dest=out;
    job->width_blocks=x;job->width_code=0xDEADBEEFu;
    cv->pixels=x*y*64u;cv->source=0x150000u;cv->dest=out;cv->arg=DEST;
    b->wr(b->ctx,AVP_JPG_MBOX_DONE,completed?AVP_JPG_DONE_MAGIC:1u,4);
    if(mode==2u)b->wr(b->ctx,0x7F80u,completed?0u:1u,4);
    return returned_size?returned_size:x*y*64u*(mode==0u?4u:2u);
}

static AvpJagFilesJpegNativeExpand native={
    &bus,FIRST,FIRST+0x1000u,0x1C0000u,0x1D0000u,
    0x150000u,0x160000u,0u,{0},{0}
};
static AvpJagFilesJpegDispatch native_dispatch={&bus,&cfg,&gzip,
    avp_jagfiles_jpeg_native_expand,&native};
static void prepare_record(unsigned mode)
{
    jpeg_setup(mode,JP80,1);
    wr(&M,FIRST+8u+2u,1u,2); /* 4:4:4 layout */
    wr(&M,FIRST+8u+4u,2u,2);
    wr(&M,FIRST+8u+6u,3u,2);
    wr(&M,AVP_JPG_VAR_SCRATCH,0x150000u,4);
    wr(&M,AVP_JPG_VAR_CONVERT_ARG,DEST,4);
    actual_decodes=0;actual_mode=mode;returned_size=0u;completed=1;
    native.last_size=0u;
}
int main(void)
{
    unsigned before;
    jpeg_setup(0u,JP80,1);execute_jpeg(); /* existing S36 callback smoke */
    prepare_record(0u);
    CHECK(avp_jagfiles_jpeg_execute(&native_dispatch,&plan,0x1C0000u)==AVP_JAGFILES_OK);
    CHECK(actual_decodes==1u&&native.last_size==1536u);
    CHECK(native.last_job.record==FIRST+8u);
    prepare_record(1u);
    CHECK(avp_jagfiles_jpeg_execute(&native_dispatch,&plan,0x1C0000u)==AVP_JAGFILES_OK);
    CHECK(actual_decodes==1u&&native.last_size==768u);
    prepare_record(2u);
    CHECK(avp_jagfiles_jpeg_execute(&native_dispatch,&plan,0x1C0000u)==AVP_JAGFILES_OK);
    CHECK(actual_decodes==1u&&native.last_size==768u);
    CHECK(native.last_convert.arg==DEST);

    prepare_record(0u);returned_size=1535u;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+8u,0x1C0000u));
    CHECK(actual_decodes==1u&&native.last_size==0u);
    prepare_record(0u);completed=0;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+8u,0x1C0000u));
    CHECK(actual_decodes==1u&&native.last_size==0u);

    prepare_record(0u);before=M.writes;
    wr(&M,FIRST+8u+4u,0u,2);before=M.writes;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+8u,0x1C0000u));
    CHECK(actual_decodes==0u && M.writes==before);
    prepare_record(0u);wr(&M,FIRST+8u+2u,0u,2);before=M.writes;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+8u,0x1C0000u));
    CHECK(actual_decodes==0u && M.writes==before);
    prepare_record(0u);before=M.writes;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+8u,0x1CFF00u));
    CHECK(actual_decodes==0u && M.writes==before);
    prepare_record(0u);before=M.writes;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+9u,0x1C0000u));
    CHECK(actual_decodes==0u && M.writes==before);
    prepare_record(2u);wr(&M,AVP_JPG_VAR_SCRATCH,0x15FF00u,4);before=M.writes;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+8u,0x1C0000u));
    CHECK(actual_decodes==0u && M.writes==before);
    prepare_record(2u);wr(&M,AVP_JPG_VAR_CONVERT_ARG,0u,4);before=M.writes;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+8u,0x1C0000u));
    CHECK(actual_decodes==0u && M.writes==before);
    prepare_record(2u);wr(&M,AVP_JPEG_CRY_LOADED,0u,1);before=M.writes;
    CHECK(!avp_jagfiles_jpeg_native_expand(&native,FIRST+8u,0x1C0000u));
    CHECK(actual_decodes==0u && M.writes==before);
    puts("S37 native JagPEG FILES bridge: all mock dispatcher and fail-closed tests PASS");
    return 0;
}
