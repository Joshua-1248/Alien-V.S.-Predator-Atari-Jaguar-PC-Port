#include "jaguar/avp_gpu/sprite_init.h"
#include <stddef.h>

#define RD16(a) (bus->rd(bus->ctx,(uint32_t)(a),2)&0xffffu)
#define RD32(a) bus->rd(bus->ctx,(uint32_t)(a),4)
#define WR32(a,v) bus->wr(bus->ctx,(uint32_t)(a),(uint32_t)(v),4)

/* $DB92, $BCCA and $BC18. Original uses the current level's $2F1A8
 * resource base, plus $3A00, as FILES temporary scratch. */
int avp_jagsprite_driver_prepare(const AvpJagFilesJpegDispatch *d)
{
    const AvpView3DBus *bus;
    if(!d||!d->bus||!d->cfg)return 0;
    bus=d->bus;
    WR32(0x02ef58u,RD32(0x02f1a8u)+0x3a00u);
    WR32(0x02ef50u,0);
    WR32(AVP_JAG_SPRITE_ALLOC_PTR,0x057c00u);
    return 1;
}

/* Character word 0=Marine, 4=Alien, 8=Predator. The dispatch at $DB86
 * holds 68000 ROM pointers, not one-byte species IDs. Follow its order.
 * $BD44 uses a hardware-mapped CLUT address, not a DRAM copy. */
int avp_jagsprite_character_init(const AvpJagFilesJpegDispatch *d)
{
    const AvpView3DBus *bus;
    static const uint32_t palette_roots[]={0x7dd0u,0x7de0u,0x7df0u,0x7dc0u,
                                           0x7db0u,0x7d70u,0x7d80u,0x7d90u};
    static const struct {uint16_t file;uint32_t address;} marine[]={
        {0xbfu,0x02f56cu},{0xc0u,0x02f558u},{0xc1u,0x02f55cu},
        {0xc4u,0x02f560u},{0xc3u,0x02f564u},{0xc2u,0x02f568u}
    };
    uint32_t character,first,clut,third;
    AvpJagFilesPlan palette_record;
    if(!d||!d->bus||!d->cfg)return 0;
    bus=d->bus;
    character=RD16(AVP_JAG_SPRITE_CHARACTER);
    if(character==4u){first=0xbau;clut=0xcau;third=0xbdu;}
    else if(character==8u){first=0xbbu;clut=0xcbu;third=0xbeu;}
    else if(character==0u){first=0xb9u;clut=0xc9u;third=0xbcu;}
    else return 0;
    if(!avp_jagsprite_alloc_load(d,first,0x02f550u))return 0;
    /* The original $BD88 NONE copy accepts TOM CLUT targets; the generic
     * DRAM-only panel helper intentionally rejects addresses >= $200000.
     * Use the same validated NONE record copier with Jaguar bus bounds. */
    if(avp_jagfiles_plan(bus,d->cfg,clut,&palette_record)!=AVP_JAGFILES_OK ||
       palette_record.tag!=0x4e4f4e45u ||
       avp_jagfiles_expand_none(bus,d->cfg,&palette_record,0xf005c0u,
                                 0xf00600u,NULL)!=AVP_JAGFILES_OK)return 0;
    if(!avp_jagsprite_alloc_load(d,third,0x0391e0u))return 0;
    if(character==4u){
        if(!avp_jagsprite_alloc_load(d,0xc5u,0x0310c8u))return 0;
    }else if(character==0u){
        for(unsigned i=0;i<sizeof marine/sizeof marine[0];i++)
            if(!avp_jagsprite_alloc_load(d,marine[i].file,marine[i].address))return 0;
    }
    for(unsigned i=0;i<8u;i++)WR32(0x02f570u+i*4u,palette_roots[i]);
    return 1;
}
int avp_jagsprite_common_init(const AvpJagFilesJpegDispatch *d)
{
    const AvpView3DBus *bus;
    if(!d||!d->bus||!d->cfg)return 0;
    bus=d->bus;
    if(!avp_jagsprite_alloc_load(d,0xccu,0x02f554u))return 0;
    return avp_jagsprite_alloc_load(d,RD16(0x03108au),0x03108cu);
}
