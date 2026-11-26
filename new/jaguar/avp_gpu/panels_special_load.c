/* RE25 S48 (Claude): see include/jaguar/avp_gpu/panels_special_load.h */
#include "jaguar/avp_gpu/panels_special_load.h"
#include "jaguar/avp_gpu/panels_special_lookup.h"
#include "jaguar/avp_gpu/panels_overlay.h"
#include "jaguar/avp_gpu/files_jpeg_native_expand.h"
#include <stddef.h>

/* diagnostic: last stage reached (1 lookup,2 main,3 overlay load,4 overlay blit,5 second) */
int avp_jagpanel_special_stage;
int avp_jagpanel_special_detail;

/* $BD44 for one FILES index (JPEG, GZIP and NONE records).  *size gets the
 * value the original returns in D0 (decoded / inflated / stored bytes). */
int avp_jagpanel_files_load_sized(const AvpJagFilesJpegDispatch *d,uint32_t index,
                                  uint32_t dest,uint32_t *size)
{
    AvpJagFilesPlan plan;
    int r=avp_jagfiles_plan(d->bus,d->cfg,index,&plan);
    uint32_t got=0;
    if(r!=AVP_JAGFILES_OK){avp_jagpanel_special_detail=100+r;return 0;}
    avp_jagpanel_special_detail=(int)(plan.tag&0xFFFFu);
    if(plan.tag==0x475A4950u){                     /* 'GZIP': cartridge service via $BF52/$BF6E */
        if(!d->gzip)return 0;
        r=avp_jagfiles_execute(d->bus,d->cfg,&plan,dest,avp_jagfiles_gzip_dispatch_invoke,(void*)d->gzip);
        if(r==AVP_JAGFILES_OK&&d->gzip->reported_size)got=*d->gzip->reported_size;
    }else if(plan.tag==0x4E4F4E45u){               /* 'NONE': $BD88 verbatim copy */
        r=avp_jagfiles_expand_none(d->bus,d->cfg,&plan,dest,0x200000u,&got);
    }else{
        r=avp_jagfiles_jpeg_execute(d,&plan,dest);
        if(r==AVP_JAGFILES_OK&&d->expand==avp_jagfiles_jpeg_native_expand&&d->expand_ctx)
            got=((const AvpJagFilesJpegNativeExpand*)d->expand_ctx)->last_size;
    }
    if(r!=AVP_JAGFILES_OK){avp_jagpanel_special_detail=200-r;return 0;}
    if(size)*size=got;
    return 1;
}
int avp_jagpanel_files_load(const AvpJagFilesJpegDispatch *d,uint32_t index,uint32_t dest)
{
    return avp_jagpanel_files_load_sized(d,index,dest,NULL);
}
/* $F322: one overlay descriptor {FILES id, x, y} */
static int overlay(const AvpJagFilesJpegDispatch *d,const uint8_t desc[3],uint32_t dest)
{
    if(desc[0]==0u)return 1;                       /* tst.b d0 / beq rts */
    avp_jagpanel_special_stage=3;
    if(!avp_jagpanel_files_load(d,desc[0],AVP_JAG_PANEL_OVERLAY_SCRATCH))return 0;
    avp_jagpanel_special_stage=4;
    return avp_jagpanel_overlay_postload(d->bus,desc[1],desc[2],dest,1,1);
}
int avp_jagpanel_special_load(const AvpJagFilesJpegDispatch *d,
                              uint32_t special_id,uint32_t dest,unsigned secondary_limit)
{
    AvpJagPanelSpecialLookup lk;
    if(!d||!d->bus||!d->cfg)return 0;
    avp_jagpanel_special_stage=1;
    if(!avp_jagpanel_special_lookup(d->bus,special_id,secondary_limit,&lk))return 0;
    avp_jagpanel_special_stage=2;
    if(!avp_jagpanel_files_load(d,lk.file_index,dest))return 0;
    if(!overlay(d,lk.primary_descriptor,dest))return 0;
    avp_jagpanel_special_stage=5;
    if((special_id&0xFFu)==0x80u){
        if(!lk.secondary_descriptor_address)return 0;
        if(!overlay(d,lk.secondary_descriptor,dest))return 0;
    }
    return 1;
}
