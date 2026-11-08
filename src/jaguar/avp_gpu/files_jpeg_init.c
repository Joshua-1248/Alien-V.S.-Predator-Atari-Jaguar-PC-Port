#include "jaguar/avp_gpu/files_jpeg_init.h"
#include <stdint.h>

/* Original $BE06-$BE1E: scan until a zero quality or a match; then copy
 * exactly 320 bytes from its source, defaulting to $02E518.  Bounds are
 * host-validation conditions, NOT claims about a Jaguar hardware limit. */
int avp_jagfiles_jpeg_quant_plan(const AvpView3DBus *bus, uint16_t quality,
                                AvpJagFilesJpegQuant *result)
{
    AvpJagFilesJpegQuant p={AVP_JPEG_DEFAULT_TABLE,
                            AVP_JPEG_GPU_QUANT_TABLE,quality,0u};
    if(!bus||!bus->rd||!result)return 0;
    for(unsigned i=0;i<7u;i++){
        uint32_t address=AVP_JPEG_QUALITY_LIST+i*6u;
        uint16_t key=(uint16_t)bus->rd(bus->ctx,address,2);
        if(!key)break;
        if(key==quality){
            p.source=bus->rd(bus->ctx,address+2u,4);
            p.matching_entry=i+1u;
            break;
        }
    }
    if(p.source>0x200000u-AVP_JPEG_QUANT_BYTES||
       p.target!=AVP_JPEG_GPU_QUANT_TABLE)return 0;
    *result=p;
    return 1;
}

int avp_jagfiles_jpeg_quant_apply(const AvpView3DBus *bus,
                                 const AvpJagFilesJpegQuant *plan)
{
    AvpJagFilesJpegQuant fresh;
    if(!bus||!bus->rd||!bus->wr||!plan||
       !avp_jagfiles_jpeg_quant_plan(bus,plan->quality,&fresh)||
       fresh.source!=plan->source||fresh.target!=plan->target||
       fresh.matching_entry!=plan->matching_entry)return 0;
    for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++){
        uint32_t value=bus->rd(bus->ctx,plan->source+i,1);
        bus->wr(bus->ctx,plan->target+i,value,1);
    }
    return 1;
}


/* $BE00 reaches the same $8C28 Blitter copy for cached JPEG quality
 * changes as for first-installation. Do not substitute a CPU byte copy: the
 * original leaves observable Blitter register and command state. */
static void original_blitter_copy(const AvpView3DBus *bus,
                                  uint32_t src,uint32_t dst,uint32_t length);

int avp_jagfiles_jpeg_init_cached(const AvpView3DBus *bus,uint32_t tag)
{
    AvpJagFilesJpegQuant plan;
    uint32_t mode;
    if(!bus||!bus->rd||!bus->wr||(tag>>16)!=0x4A50u||
       bus->rd(bus->ctx,AVP_JPEG_PREVIOUS_TAG,2)!=0x4A50u)return 0;
    mode=bus->rd(bus->ctx,AVP_JPEG_MODE_VARIABLE,1);
    if(mode>2u ||
       (mode==2u && !bus->rd(bus->ctx,AVP_JPEG_CRY_LOADED,1))||
       !avp_jagfiles_jpeg_quant_plan(bus,(uint16_t)tag,&plan))return 0;
    /* $BD92 writes D0.w to $2EF5C before the mode dispatch.  Both
     * $BDCA (mode 1) and $BDE0 (modes 0/2) skip GPU reinstallation when
     * the previous initialized tag has high word 'JP', then call $BE00. */
    bus->wr(bus->ctx,AVP_JPEG_QUALITY_VARIABLE,(uint16_t)tag,2);
    original_blitter_copy(bus,plan.source,plan.target,AVP_JPEG_QUANT_BYTES);
    return 1;
}

/* Original $8940: decode a program's two-word header and use the Blitter's
 * GPU address alias. The strict bounds are host-side safety constraints,
 * not an assertion of a new Jaguar hardware limit. */
int avp_jagfiles_jpeg_install_plan(const AvpView3DBus *bus,unsigned mode,
                                   AvpJagFilesJpegInstall *result)
{
    AvpJagFilesJpegInstall p;
    uint64_t end;
    if(!bus||!bus->rd||!result||mode>2u)return 0;
    p.mode=mode;
    p.header=mode==1u?AVP_JPEG_FRONTEND_PROGRAM:AVP_JPEG_LEVEL_PROGRAM;
    p.gpu_target=bus->rd(bus->ctx,p.header,4);
    p.length=bus->rd(bus->ctx,p.header+4u,4);
    if(p.length>UINT32_MAX-3u)return 0;
    p.rounded_length=(p.length+3u)&~3u;
    p.source=p.header+8u;
    end=(uint64_t)p.source+p.rounded_length;
    /* A loading plan must not overlap the quantization table or mailbox. */
    if(p.gpu_target!=AVP_JPEG_GPU_ENTRY||p.length==0u||
       p.rounded_length>AVP_JPEG_GPU_QUANT_TABLE-AVP_JPEG_GPU_ENTRY||
       end>0x200000u)return 0;
    p.alias_target=p.gpu_target+AVP_JPEG_GPU_ALIAS_DELTA;
    *result=p;
    return 1;
}

static void original_blitter_copy(const AvpView3DBus *bus,uint32_t src,uint32_t dst,uint32_t length)
{
    uint32_t src_align=src&~7u,dst_align=dst&~7u;
    uint32_t src_offset=src&7u,dst_offset=dst&7u;
#define W(a,v) bus->wr(bus->ctx,(a),(v),4)
    W(0xF02228u,0x18u);
    W(0xF02224u,src_align);
    W(0xF02230u,src_offset);
    W(0xF02204u,0x18u);
    W(0xF02200u,dst_align);
    W(0xF0220Cu,dst_offset);
    W(0xF0223Cu,length|0x10000u);
    W(0xF02238u,dst_offset<src_offset?0x1800005u:0x1800001u);
#undef W
}

int avp_jagfiles_jpeg_install_apply(const AvpView3DBus *bus,
                                    const AvpJagFilesJpegInstall *plan)
{
    AvpJagFilesJpegInstall p;
    if(!bus||!bus->rd||!bus->wr||!plan||
       !avp_jagfiles_jpeg_install_plan(bus,plan->mode,&p)||
       p.header!=plan->header||p.source!=plan->source||
       p.gpu_target!=plan->gpu_target||p.alias_target!=plan->alias_target||
       p.length!=plan->length||p.rounded_length!=plan->rounded_length)return 0;
    /* $8940: stop GPU, then $8C28: source->GPU alias. */
    bus->wr(bus->ctx,AVP_JPEG_GPU_CTRL,8u,4);
    original_blitter_copy(bus,p.source,p.alias_target,p.rounded_length);
    return 1;
}

int avp_jagfiles_jpeg_init_first(const AvpView3DBus *bus,uint32_t tag)
{
    AvpJagFilesJpegInstall install;
    AvpJagFilesJpegQuant quant;
    unsigned mode;
    if(!bus||!bus->rd||!bus->wr||(tag>>16)!=0x4A50u||
       bus->rd(bus->ctx,AVP_JPEG_PREVIOUS_TAG,2)==0x4A50u)return 0;
    mode=bus->rd(bus->ctx,AVP_JPEG_MODE_VARIABLE,1);
    if(mode>2u||(mode==2u&&!bus->rd(bus->ctx,AVP_JPEG_CRY_LOADED,1))||
       !avp_jagfiles_jpeg_install_plan(bus,mode,&install)||
       !avp_jagfiles_jpeg_quant_plan(bus,(uint16_t)tag,&quant))return 0;
    /* $BD92 quality write, $BDCA/$BDE0 GPU program install, $BDF6
     * Huffman mailbox, $BE00 quant table (which reads that quality). */
    bus->wr(bus->ctx,AVP_JPEG_QUALITY_VARIABLE,(uint16_t)tag,2);
    if(!avp_jagfiles_jpeg_install_apply(bus,&install))return 0;
    bus->wr(bus->ctx,AVP_JPEG_GPU_MAILBOX,0x8010u,4);
    /* $BE00 calls the same $8C28 hardware copy routine.  Keep Blitter
     * register state identical, not merely its final 320 destination bytes. */
    original_blitter_copy(bus,quant.source,quant.target,AVP_JPEG_QUANT_BYTES);
    return 1;
}
