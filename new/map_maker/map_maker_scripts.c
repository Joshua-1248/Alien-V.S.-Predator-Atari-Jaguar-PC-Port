#include "map_maker_scripts.h"
#include <string.h>

#include "map_maker_scripts_catalog.inc"

unsigned avp_mm_script_catalog_size(void)
{
    return (unsigned)(sizeof avp_mm_retail_script_rows / sizeof avp_mm_retail_script_rows[0]);
}
unsigned avp_mm_script_copy_level(unsigned level,AvpMmScriptRow *out,unsigned cap)
{
    unsigned n=0;
    if(!out||level<1u||level>15u)return 0;
    for(unsigned i=0;i<avp_mm_script_catalog_size();i++){
        if(avp_mm_retail_script_rows[i].level!=level)continue;
        if(n>=cap)return 0;
        out[n++]=avp_mm_retail_script_rows[i];
    }
    return n;
}
int avp_mm_script_lookup(const AvpMmScriptRow *rows,unsigned n,
                         unsigned level,unsigned offset,AvpMmScriptInfo *out)
{
    if(!out)return 0;
    memset(out,0,sizeof *out);
    if(!rows||n>AVP_MM_MAX_SCRIPTS)return 0;
    if(level<1u||level>15u||offset>=64u*64u*8u)return 0;
    for(unsigned i=0;i<n;i++){
        const AvpMmScriptRow *r=&rows[i];
        if(r->level!=level||r->offset!=offset)continue;
        switch(r->kind){
        case 1u:out->flags|=AVP_MM_SCRIPT_ACCESS;out->access_code=r->arg;break;
        case 2u:out->flags|=AVP_MM_SCRIPT_EXIT;
            out->destination_x=r->destination_x;
            out->destination_y=r->destination_y;
            out->destination_level=r->destination_level;break;
        case 3u:out->flags|=AVP_MM_SCRIPT_INTERLOCK;break;
        case 4u:out->flags|=AVP_MM_SCRIPT_REQUIRED;
            out->interlock_required_by=(unsigned)r->arg;break;
        default:break;
        }
    }
    return out->flags!=0u;
}

int avp_mm_script_at(unsigned level,unsigned offset,AvpMmScriptInfo *out)
{
    return avp_mm_script_lookup(avp_mm_retail_script_rows,
                                avp_mm_script_catalog_size(),level,offset,out);
}
/* A serialized overlay is an offset-only move of the original indexed rows.
 * This guards against fabricated access, dropped exits, orphaned interlocks,
 * and merging or splitting two original script anchors. */
int avp_mm_script_validate(const AvpMmScriptRow *rows,unsigned count,unsigned level)
{
    AvpMmScriptRow original[AVP_MM_MAX_SCRIPTS];
    unsigned n=avp_mm_script_copy_level(level,original,AVP_MM_MAX_SCRIPTS);
    if(!rows||!n||count!=n)return 0;
    for(unsigned i=0;i<n;i++){
        const AvpMmScriptRow *a=&rows[i],*b=&original[i];
        unsigned field=a->offset&7u;
        if(a->level!=level||a->kind!=b->kind||a->offset>=32768u||
           (a->kind==2u?field>=4u:field<6u)||
           a->destination_x!=b->destination_x||
           a->destination_y!=b->destination_y||
           a->destination_level!=b->destination_level)return 0;
        if(a->kind!=4u&&a->arg!=b->arg)return 0;
        if(a->kind==4u){
            int found=0;
            for(unsigned j=0;j<n;j++)if(original[j].kind==3u &&
                original[j].offset==(unsigned)b->arg &&
                rows[j].kind==3u && rows[j].offset==(unsigned)a->arg)
                found=1;
            if(!found)return 0;
        }
        for(unsigned j=0;j<i;j++){
            /* Original equality of offsets must be preserved exactly. */
            if((original[i].offset==original[j].offset)!=
               (rows[i].offset==rows[j].offset))return 0;
        }
    }
    return 1;
}
