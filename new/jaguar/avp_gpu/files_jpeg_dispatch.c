#include "jaguar/avp_gpu/files_jpeg_dispatch.h"
#include <stdint.h>

static int valid(const AvpJagFilesJpegDispatch *d)
{
    return d && d->bus && d->bus->rd && d->bus->wr && d->cfg && d->expand &&
           d->cfg->archive_begin < d->cfg->archive_end &&
           d->cfg->archive_end <= 0x1000000u;
}

int avp_jagfiles_jpeg_dispatch_invoke(void *ctx, uint32_t routine_pc,
    uint32_t record_address, uint32_t destination_address)
{
    AvpJagFilesJpegDispatch *d=(AvpJagFilesJpegDispatch*)ctx;
    uint32_t tag,mode,prev;
    if(!valid(d))return 0;
    if(routine_pc==AVP_JAGFILES_JPEG_INIT_PC){
        if(record_address<d->cfg->archive_begin ||
           (uint64_t)record_address+8u>d->cfg->archive_end)return 0;
        tag=d->bus->rd(d->bus->ctx,record_address,4);
        if((tag>>16)!=0x4A50u)return 0;
        mode=d->bus->rd(d->bus->ctx,AVP_JPEG_MODE_VARIABLE,1);
        if(mode>2u)return 0;
        if(mode==2u)
            return avp_jagfiles_jpeg_mode2_prepare_native(
                d->bus,d->cfg,d->gzip,tag);
        prev=d->bus->rd(d->bus->ctx,AVP_JPEG_PREVIOUS_TAG,4);
        if((prev>>16)==0x4A50u)
            return avp_jagfiles_jpeg_init_cached(d->bus,tag);
        return avp_jagfiles_jpeg_init_first(d->bus,tag);
    }
    if(routine_pc==AVP_JAGFILES_JPEG_EXP_PC){
        if(record_address < d->cfg->archive_begin+8u ||
           record_address>=d->cfg->archive_end ||
           (d->bus->rd(d->bus->ctx,record_address-8u,4)>>16)!=0x4A50u)
            return 0;
        return d->expand(d->expand_ctx,record_address,destination_address)!=0;
    }
    return 0;
}

int avp_jagfiles_jpeg_execute(const AvpJagFilesJpegDispatch *d,
    const AvpJagFilesPlan *plan, uint32_t destination_address)
{
    if(!valid(d)||!plan||(plan->tag>>16)!=0x4A50u||
       plan->init_pc!=AVP_JAGFILES_JPEG_INIT_PC||
       plan->expand_pc!=AVP_JAGFILES_JPEG_EXP_PC||
       d->cfg->previous_tag!=AVP_JPEG_PREVIOUS_TAG||
       !d->cfg->max_entries||
       (uint64_t)plan->header+8u>d->cfg->archive_end||
       plan->header<d->cfg->archive_begin||
       plan->payload!=plan->header+8u||
       d->bus->rd(d->bus->ctx,plan->header,4)!=plan->tag)
        return AVP_JAGFILES_INVALID;
    return avp_jagfiles_execute(d->bus,d->cfg,plan,destination_address,
        avp_jagfiles_jpeg_dispatch_invoke,(void*)d);
}
