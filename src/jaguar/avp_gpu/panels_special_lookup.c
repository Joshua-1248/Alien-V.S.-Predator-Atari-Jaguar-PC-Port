#include "jaguar/avp_gpu/panels_special_lookup.h"
#include <stddef.h>
#include <string.h>

int avp_jagpanel_special_lookup(const AvpView3DBus *bus, uint32_t special_id,
                                unsigned secondary_limit,
                                AvpJagPanelSpecialLookup *out)
{
    AvpJagPanelSpecialLookup tmp;
    uint32_t primary, secondary_index;
    if(!bus || !bus->rd || !out || special_id<0x80u || special_id>0x8cu)
        return 0;
    /* $F2DE clears bit 7; $F2E4 multiplies the remaining byte by four. */
    primary=AVP_JAG_PANEL_SPECIAL_TABLE+((special_id&0x7fu)<<2);
    if(primary+4u>0x200000u)return 0;
    memset(&tmp,0,sizeof tmp);
    tmp.file_index=bus->rd(bus->ctx,primary,1);
    tmp.primary_descriptor_address=primary+1u;
    for(unsigned i=0;i<3u;i++)
        tmp.primary_descriptor[i]=(uint8_t)bus->rd(bus->ctx,primary+1u+i,1);
    if(special_id==0x80u){
        /* $F302-$F31C: 3*(DRAM[$2F5AA]-1). The original does not
         * check this count; the isolated adapter refuses unsafe values. */
        uint32_t count=bus->rd(bus->ctx,AVP_JAG_PANEL_SPECIAL_INDEX,2);
        if(!count || !secondary_limit || count>secondary_limit)return 0;
        secondary_index=count-1u;
        if(secondary_index>(0x200000u-AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE-3u)/3u)
            return 0;
        tmp.secondary_descriptor_address=AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE+3u*secondary_index;
        for(unsigned i=0;i<3u;i++)
            tmp.secondary_descriptor[i]=(uint8_t)bus->rd(bus->ctx,tmp.secondary_descriptor_address+i,1);
    }
    *out=tmp;
    return 1;
}
