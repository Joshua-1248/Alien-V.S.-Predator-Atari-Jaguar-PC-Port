#include "jaguar/avp_gpu/files_jpeg_native_expand.h"
#include <stdint.h>

static int fits(uint32_t addr,uint64_t len,uint32_t begin,uint32_t end)
{
    return begin < end && addr >= begin && (uint64_t)addr + len <= end;
}

int avp_jagfiles_jpeg_native_expand(void *opaque, uint32_t payload,
                                    uint32_t destination)
{
    AvpJagFilesJpegNativeExpand *n=(AvpJagFilesJpegNativeExpand*)opaque;
    const AvpView3DBus *bus;
    uint32_t mode,blocks_x,blocks_y,layout,scratch,table;
    uint64_t pixels,bytes,rgb_bytes;
    AvpJagpegJob job;
    AvpJagpegConvert cv;
    uint32_t actual;
    if(!n || !n->bus || !n->bus->rd || !n->bus->wr)return 0;
    bus=n->bus;
    /* $BE48's input is the record PAYLOAD (the FILES header is payload-8).
     * The record layout/geometry are read where original exp_jpeg expects. */
    if(n->archive_end>0x1000000u || n->output_end>0x200000u ||
       n->scratch_end>0x200000u ||
       (uint64_t)payload < (uint64_t)n->archive_begin+8u ||
       !fits(payload,8u,n->archive_begin,n->archive_end) ||
       (bus->rd(bus->ctx,payload-8u,4)>>16)!=0x4A50u)return 0;
    mode=bus->rd(bus->ctx,AVP_JPG_VAR_MODE,1);
    layout=bus->rd(bus->ctx,payload+2u,2);
    blocks_x=bus->rd(bus->ctx,payload+4u,2);
    blocks_y=bus->rd(bus->ctx,payload+6u,2);
    if(mode>2u || layout<1u || layout>3u ||
       blocks_x==0u || blocks_y==0u ||
       blocks_x>8191u || blocks_y>8191u)return 0;
    pixels=(uint64_t)blocks_x*blocks_y*64u;
    bytes=pixels*(mode==0u?4u:2u);
    rgb_bytes=pixels*4u;
    if(bytes==0u || bytes>UINT32_MAX ||
       !fits(destination,bytes,n->output_begin,n->output_end))return 0;
    if(mode==2u){
        scratch=bus->rd(bus->ctx,AVP_JPG_VAR_SCRATCH,4);
        table=bus->rd(bus->ctx,AVP_JPG_VAR_CONVERT_ARG,4);
        /* A mode-2 converter needs the actual CRY lookup data in DRAM.
         * A zero scratch pointer makes the original decode run in-place;
         * validate that separately without assuming the original allocation. */
        if(!table || !bus->rd(bus->ctx,AVP_JPEG_CRY_LOADED,1) ||
           !fits(table,AVP_JPEG_CRY_TABLE_BYTES,0u,0x200000u))return 0;
        if(scratch){
            if(!fits(scratch,rgb_bytes,n->scratch_begin,n->scratch_end))return 0;
        }else if(!fits(destination,rgb_bytes,n->output_begin,n->output_end))
            return 0;
    }
    actual=avp_jagpeg_exp(bus,payload,destination,&job,&cv);
    if(actual!=(uint32_t)bytes ||
       bus->rd(bus->ctx,AVP_JPG_MBOX_DONE,4)!=AVP_JPG_DONE_MAGIC ||
       (mode==2u && bus->rd(bus->ctx,0x7F80u,4)!=0u))return 0;
    n->last_size=actual;
    n->last_job=job;
    n->last_convert=cv;
    return 1;
}
