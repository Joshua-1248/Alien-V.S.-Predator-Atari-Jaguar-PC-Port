/* RE25 S48 (Claude): see include/jaguar/avp_gpu/panels_walk.h */
#include "jaguar/avp_gpu/panels_walk.h"

#define RD8(a)  (bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,1)&0xFFu)
#define RD16(a) (bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,2)&0xFFFFu)
#define RD32(a) bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,4)
#define WR8(a,v)  bus->wr(bus->ctx,(uint32_t)(a)&0xFFFFFFu,(uint32_t)(v)&0xFFu,1)
#define WR16(a,v) bus->wr(bus->ctx,(uint32_t)(a)&0xFFFFFFu,(uint32_t)(v)&0xFFFFu,2)
#define WR32(a,v) bus->wr(bus->ctx,(uint32_t)(a)&0xFFFFFFu,(uint32_t)(v),4)

static uint32_t slot_address(uint32_t index)
{   /* swap / clr.w / lsr.l #1: index * 32 KiB */
    return AVP_JAG_PANEL_SLOT_BASE+(((index&0xFFFFu)<<16)>>1);
}

/* $F230: one entry's ID bytes.  Returns 0 on an unsupported load. */
static int entry(const AvpJagFilesJpegDispatch *d,uint32_t *a5,uint32_t a0,uint32_t a2)
{
    const AvpView3DBus *bus=d->bus;
    uint32_t A5=*a5,A1,d0,d1;
    A1=slot_address(RD16(AVP_JAG_PANEL_SLOT_INDEX));
    WR32(a0,A1);                                   /* texture variable = first slot */
    for(;;){
        d0=RD8(A5);A5++;
        if(d0==0xFFu){A5+=A5&1u;break;}            /* $F2BE: word align */
        d1=RD16(AVP_JAG_PANEL_SLOT_INDEX);
        WR16(AVP_JAG_PANEL_SLOT_INDEX,d1+1u);
        A1=AVP_JAG_PANEL_CACHE+(uint32_t)(int32_t)(int16_t)d1;
        if(!(d0!=0x80u&&RD8(A1)==d0)){             /* not cached ($80 always loads) */
            WR8(A1,d0);
            A1=slot_address(d1);
            if(d0&0x80u){if(!avp_jagpanel_special_load(d,d0,A1,64u))return 0;}
            else if(!avp_jagpanel_files_load(d,d0,A1))return 0;
        }
        if(a2==0u)continue;                         /* $F28E */
        {   uint32_t t=AVP_JAG_PANEL_PROFILE_TABLE,id,file;
            int found=0;
            for(;;){
                id=RD16(t);t+=2u;
                if(id&0x8000u)break;
                file=RD16(t);t+=2u;
                if(id==d0){found=1;break;}
            }
            if(found){
                A1=RD32(AVP_JAG_PANEL_PROFILE_PTR);
                if(!avp_jagpanel_files_load(d,file,A1))return 0;
            }else{
                for(unsigned i=0;i<128u;i++){WR16(A1,0u);A1+=2u;}   /* $F2C8 */
            }
            WR32(AVP_JAG_PANEL_PROFILE_PTR,RD32(AVP_JAG_PANEL_PROFILE_PTR)+0x100u);
        }
    }
    *a5=A5;
    return 1;
}

uint32_t avp_jagpanel_walk(const AvpJagFilesJpegDispatch *d,uint32_t a5)
{
    const AvpView3DBus *bus;
    uint32_t A5,d0,d2,a0,a2;
    if(!d||!d->bus||!d->cfg)return 0;
    bus=d->bus;
    A5=RD32(a5);                                   /* movea.l (A5)+,A0 / movea.l A0,A5 */
    d0=RD16(A5);A5+=2u;
    if(!(d0&0x8000u))WR16(AVP_JAG_PANEL_SLOT_INDEX,d0);
    for(;;){
        d2=RD32(A5);A5+=4u;
        if(d2&0x80000000u)break;
        a0=d2;
        a2=RD32(A5);A5+=4u;
        if(a2)WR32(a2,RD32(AVP_JAG_PANEL_PROFILE_PTR));
        if(!entry(d,&A5,a0,a2))return 0;
    }
    return a5+4u;                                  /* caller's A5, post-incremented */
}
