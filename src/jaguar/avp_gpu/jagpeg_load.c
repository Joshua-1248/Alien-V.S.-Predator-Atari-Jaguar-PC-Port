/* RE24 Session 2 (Claude): native C port of exp_jpeg ($BE48-$BF50).
 * See include/jaguar/avp_gpu/jagpeg_load.h. */
#include "jaguar/avp_gpu/jagpeg_load.h"

#define RD8(a)  (bus->rd(bus->ctx,(uint32_t)(a),1)&0xFFu)
#define RD16(a) (bus->rd(bus->ctx,(uint32_t)(a),2)&0xFFFFu)
#define RD32(a) bus->rd(bus->ctx,(uint32_t)(a),4)
#define WR16(a,v) bus->wr(bus->ctx,(uint32_t)(a),(uint32_t)(v)&0xFFFFu,2)
#define WR32(a,v) bus->wr(bus->ctx,(uint32_t)(a),(uint32_t)(v),4)

static uint16_t rol16(uint16_t v,unsigned n){return (uint16_t)((v<<n)|(v>>(16u-n)));}

void avp_jagpeg_prepare(const AvpView3DBus *bus,uint32_t record,uint32_t dest,AvpJagpegJob *job)
{
    uint32_t d0,d1;
    uint16_t w,d2;
    d0=RD16(record+4u);                       /* width in 8x8 blocks */
    WR16(AVP_JPG_VAR_WIDTH_PX,d0<<3);
    WR16(AVP_JPG_VAR_HEIGHT_PX,RD16(record+6u)<<3);
    /* Blitter width code: the exponent of the pixel width ... */
    d1=d0;w=(uint16_t)(d1<<3);
    d2=0x10u;
    for(;;){ int out; d2--; out=(w&0x8000u)!=0; w=(uint16_t)(w<<1); if(out)break; }
    w=(uint16_t)((w&0xFF00u)|(d2&0xFFu));
    w=rol16(w,3);w=rol16(w,8);
    d1=(d1&0xFFFF0000u)|w;
    WR16(AVP_JPG_VAR_BLIT_WIDTH,w);
    job->record=record;
    if(RD8(AVP_JPG_VAR_MODE)==2u&&RD32(AVP_JPG_VAR_SCRATCH)!=0u)job->dest=RD32(AVP_JPG_VAR_SCRATCH);
    else job->dest=dest;
    job->width_blocks=d0;
    job->width_code=d1;
}

uint32_t avp_jagpeg_exp(const AvpView3DBus *bus,uint32_t record,uint32_t dest,
                        AvpJagpegJob *job_out,AvpJagpegConvert *cv_out)
{
    AvpJagpegJob job;
    AvpJagpegConvert cv={0,0,0,0};
    uint32_t d0;
    avp_jagpeg_prepare(bus,record,dest,&job);
    WR32(AVP_JPG_MBOX_RECORD,job.record);WR32(AVP_JPG_MBOX_DEST,job.dest);
    WR32(AVP_JPG_MBOX_WBLOCKS,job.width_blocks);WR32(AVP_JPG_MBOX_WCODE,job.width_code);
    WR32(AVP_JPG_MBOX_DONE,0u);
    avp_jagpeg_gpu_decode(bus,&job);
    d0=job.width_blocks*RD16(record+6u);      /* mulu.w: both are words */
    d0&=0xFFFFFFFFu;
    d0<<=6;                                   /* pixels */
    if(RD8(AVP_JPG_VAR_MODE)==2u){
        uint32_t scratch=RD32(AVP_JPG_VAR_SCRATCH);
        cv.pixels=d0;
        cv.source=scratch?scratch:dest;
        cv.dest=dest;
        cv.arg=RD32(AVP_JPG_VAR_CONVERT_ARG);
        WR32(0x7F80u,cv.pixels);WR32(0x7F84u,cv.source);WR32(0x7F88u,cv.dest);WR32(0x7F8Cu,cv.arg);
        avp_jagpeg_gpu_convert(bus,&cv);
        WR32(0x7F80u,0u);                     /* the converter's completion handshake */
    }
    d0<<=1;
    if(RD8(AVP_JPG_VAR_MODE)==0u)d0<<=1;
    if(job_out)*job_out=job;
    if(cv_out)*cv_out=cv;
    return d0;
}
