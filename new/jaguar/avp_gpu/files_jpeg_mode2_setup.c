#include "jaguar/avp_gpu/files_jpeg_mode2_setup.h"
#include <stdint.h>

int avp_jagfiles_jpeg_mode2_prepare_native(const AvpView3DBus *bus,
    const AvpJagFilesConfig *cfg, const AvpJagFilesGzipDispatch *gzip,
    uint32_t jp_tag)
{
    AvpJagFilesJpegQuant quant;
    AvpJagFilesJpegInstall install;
    uint32_t previous;
    int needs_table,needs_install;
    if(!bus||!bus->rd||!bus->wr||!cfg||
       cfg->previous_tag!=AVP_JPEG_PREVIOUS_TAG||
       (jp_tag>>16)!=0x4A50u||
       bus->rd(bus->ctx,AVP_JPEG_MODE_VARIABLE,1)!=2u)return 0;

    needs_table=bus->rd(bus->ctx,AVP_JPEG_CRY_LOADED,1)==0u;
    previous=bus->rd(bus->ctx,AVP_JPEG_PREVIOUS_TAG,4);
    /* The nested GZIP FILES request replaces the codec cache with GZIP,
     * so it will necessarily require JP's first-installation branch. */
    needs_install=needs_table||(previous>>16)!=0x4A50u;

    /* Refuse malformed JPEG descriptors BEFORE invoking an external
     * cartridge service. Revalidate after it, as the service may change RAM. */
    if(!avp_jagfiles_jpeg_quant_plan(bus,(uint16_t)jp_tag,&quant)||
       (needs_install&&!avp_jagfiles_jpeg_install_plan(bus,2u,&install)))return 0;
    if(needs_table){
        if(!gzip||!avp_jagfiles_jpeg_mode2_load_cry(bus,cfg,gzip))return 0;
    }

    /* Do not use the pre-nested previous-tag value here. */
    previous=bus->rd(bus->ctx,AVP_JPEG_PREVIOUS_TAG,4);
    if((previous>>16)==0x4A50u)
        return avp_jagfiles_jpeg_init_cached(bus,jp_tag);
    return avp_jagfiles_jpeg_init_first(bus,jp_tag);
}
