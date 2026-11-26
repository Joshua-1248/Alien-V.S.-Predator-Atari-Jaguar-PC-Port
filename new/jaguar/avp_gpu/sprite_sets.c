/* RE25 S48 (Claude): see include/jaguar/avp_gpu/sprite_sets.h */
#include "jaguar/avp_gpu/sprite_sets.h"

#define RD16(a) (bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,2)&0xFFFFu)
#define RD32(a) bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,4)
#define WR32(a,v) bus->wr(bus->ctx,(uint32_t)(a)&0xFFFFFFu,(uint32_t)(v),4)
static uint32_t sext16(uint32_t v){return (uint32_t)(int32_t)(int16_t)(v&0xFFFFu);}

int avp_jagsprite_alloc_load(const AvpJagFilesJpegDispatch *d,uint32_t file,uint32_t a2)
{
    const AvpView3DBus *bus=d->bus;
    uint32_t a1=RD32(AVP_JAG_SPRITE_ALLOC_PTR),size=0;
    if(!avp_jagpanel_files_load_sized(d,file&0xFFFFu,a1,&size))return 0;
    if(a2&0xFFFFu||a2>>16)WR32(a2,a1);              /* cmpa.w #0,A2: whole register */
    size+=7u;size=(size&0xFFFFFF00u)|(size&0xF8u);  /* addq.l #7 / and.b #$F8 */
    a1+=size;
    WR32(AVP_JAG_SPRITE_ALLOC_PTR,a1);
    return 1;
}
int avp_jagsprite_set_entry(const AvpJagFilesJpegDispatch *d,uint32_t a6)
{
    const AvpView3DBus *bus=d->bus;
    uint32_t a2=AVP_JAG_SPRITE_PTR_TABLE+sext16(RD16(a6+4u)),d0;
    if(!avp_jagsprite_alloc_load(d,RD16(a6),a2))return 0;
    a2+=4u;
    d0=RD16(a6+2u);
    if(d0&0x8000u){WR32(a2,0u);return 1;}
    return avp_jagsprite_alloc_load(d,d0,a2);
}
int avp_jagsprite_load_sets(const AvpJagFilesJpegDispatch *d)
{
    const AvpView3DBus *bus;
    uint32_t a6;
    if(!d||!d->bus||!d->cfg)return 0;
    bus=d->bus;
    a6=RD32(AVP_JAG_SPRITE_LIST_TABLE+sext16(RD16(AVP_JAG_SPRITE_CHARACTER)));
    while(!(RD16(a6)&0x8000u)){
        if(!avp_jagsprite_set_entry(d,a6))return 0;
        a6+=0x14u;
    }
    return 1;
}
