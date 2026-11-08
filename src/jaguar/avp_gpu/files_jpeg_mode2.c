#include "jaguar/avp_gpu/files_jpeg_mode2.h"
#include <stdint.h>

int avp_jagfiles_jpeg_mode2_load_cry(const AvpView3DBus *bus,
    const AvpJagFilesConfig *cfg, const AvpJagFilesGzipDispatch *gzip)
{
    AvpJagFilesPlan plan;
    AvpJagFilesGzipDispatch adapter;
    uint32_t dest,reported=0;
    int rc;
    if(!bus||!bus->rd||!bus->wr||!cfg||!gzip||
       gzip->bus!=bus||!gzip->invoke)return 0;
    if(bus->rd(bus->ctx,AVP_JPEG_MODE_VARIABLE,1)!=2u)return 0;
    if(bus->rd(bus->ctx,AVP_JPEG_CRY_LOADED,1)!=0u)return 1;
    dest=bus->rd(bus->ctx,AVP_JPEG_CRY_TABLE_POINTER,4);
    /* Do not permit an unknown-size cartridge output to overwrite the
     * 68000's game/data image. This bounds a known 128 KiB destination;
     * it does not purport to limit actual Jaguar cartridge memory. */
    if(dest==0u||(dest&1u)||dest>0x200000u-AVP_JPEG_CRY_TABLE_BYTES)
        return 0;
    rc=avp_jagfiles_plan(bus,cfg,AVP_JPEG_CRY_TABLE_FILE_ID,&plan);
    if(rc!=AVP_JAGFILES_OK||plan.tag!=0x475A4950u||
       plan.init_pc!=AVP_JAGFILES_GZIP_INIT_PC||
       plan.expand_pc!=AVP_JAGFILES_GZIP_EXP_PC)return 0;
    adapter=*gzip;
    adapter.reported_size=&reported;
    rc=avp_jagfiles_execute(bus,cfg,&plan,dest,
                           avp_jagfiles_gzip_dispatch_invoke,&adapter);
    if(rc!=AVP_JAGFILES_OK||reported!=AVP_JPEG_CRY_TABLE_BYTES)return 0;
    /* $BDAE: not.b $2EF64 (0 -> $FF). S48: was written as 1; found by the
     * natural-level-load walker check (panels_walk_rom_check). */
    bus->wr(bus->ctx,AVP_JPEG_CRY_LOADED,(~bus->rd(bus->ctx,AVP_JPEG_CRY_LOADED,1))&0xFFu,1);
    return 1;
}
