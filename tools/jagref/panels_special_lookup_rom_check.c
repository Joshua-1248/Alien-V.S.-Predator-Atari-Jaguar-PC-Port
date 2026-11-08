/* S42: 13 authentic original 68000 $F2D6 high-bit panel lookups.
 * Stop BEFORE calling $BD44 (JP80); this is intentionally NOT a GPU,
 * JPEG, secondary overlay, or full Blitter-compositing parity claim.
 * The private retail ROM is only read into RAM and never written out. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/panels_special_lookup.h"
#include "jaguar/avp_gpu/panels_cached.h"
#include "jaguar/avp_gpu/files_dispatch.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define LIST 0x190000u
#define HEAD 0x191000u
#define PTR 0x191100u
#define STACK 0x6c00u
#define AT_DECODER_CALL 0xf2f0u
static uint8_t rom[ROM_BYTES];
static JagMachine mach, boot;
static uint32_t rd(void *ctx,uint32_t addr,int width)
{JagMachine *m=(JagMachine*)ctx;return width==1?jagm_read8(m,addr):width==2?jagm_read16(m,addr):jagm_read32(m,addr);}
static void wr(void *ctx,uint32_t addr,uint32_t value,int width)
{JagMachine *m=(JagMachine*)ctx;if(width==1)jagm_write8(m,addr,(uint8_t)value);else if(width==2)jagm_write16(m,addr,(uint16_t)value);else jagm_write32(m,addr,value);}
static void fix(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int check_one(unsigned n)
{
    uint32_t id=0x80u+n;
    AvpJagPanelSpecialLookup plan;
    AvpJagFilesPlan file;
    AvpView3DBus bus;
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xC00000u,
                           AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    unsigned steps;
    memcpy(&mach,&boot,sizeof mach);fix(&mach);
    bus.ctx=&mach;bus.rd=rd;bus.wr=wr;
    /* Controlled authentic runtime state, same sort of injected panel-list
     * harness used in S40/S41. The original 68000 decodes the actual table. */
    jagm_write16(&mach,LIST,3u);
    jagm_write32(&mach,LIST+2u,PTR);
    jagm_write32(&mach,LIST+6u,0u);
    jagm_write8(&mach,LIST+10u,(uint8_t)id);
    jagm_write8(&mach,LIST+11u,0xffu);
    jagm_write32(&mach,LIST+12u,0xffffffffu);
    jagm_write32(&mach,HEAD,LIST);
    jagm_write8(&mach,AVP_JAG_PANEL_CACHE_BASE+3u, n==0u?0x80u:0xfeu);
    jagm_write16(&mach,AVP_JAG_PANEL_SPECIAL_INDEX,1u);
    jagm_write32(&mach,STACK,0x1000u);
    if(!avp_jagpanel_special_lookup(&bus,id,1u,&plan)){
        fprintf(stderr,"id %02x: bounded native resolver refused retail descriptor\n",id);return 1;
    }
    if(avp_jagfiles_plan(&bus,&cfg,plan.file_index,&file)!=AVP_JAGFILES_OK ||
       (file.tag>>16)!=0x4a50u){
        fprintf(stderr,"id %02x: original linked FILES is not a JP80 record\n",id);return 1;
    }
    m68k_set_reg(M68K_REG_SR,0x2700u);
    m68k_set_reg(M68K_REG_SP,STACK);
    m68k_set_reg(M68K_REG_A5,HEAD);
    m68k_set_reg(M68K_REG_PC,0xf1f6u);
    for(steps=0;steps<2000u;steps++){
        if(m68k_get_reg(NULL,M68K_REG_PC)==AT_DECODER_CALL)break;
        (void)m68k_execute(1);
    }
    if(m68k_get_reg(NULL,M68K_REG_PC)!=AT_DECODER_CALL){
        fprintf(stderr,"id %02x: original did not reach FILES $BD44 call, PC=%06x\n",
                id,(unsigned)m68k_get_reg(NULL,M68K_REG_PC));return 1;
    }
    if((uint32_t)m68k_get_reg(NULL,M68K_REG_D0)!=plan.file_index ||
       (uint32_t)m68k_get_reg(NULL,M68K_REG_A0)!=plan.primary_descriptor_address ||
       jagm_read8(&mach,AVP_JAG_PANEL_CACHE_BASE+3u)!=id ||
       jagm_read16(&mach,AVP_JAG_PANEL_INDEX_ADDR)!=4u ||
       jagm_read32(&mach,PTR)!=(AVP_JAG_PANEL_SLOT_BASE+3u*AVP_JAG_PANEL_SLOT_BYTES)){
        fprintf(stderr,"id %02x: original 68000 lookup/register/cache/slot mismatch D0=%08x A0=%08x\n",
                id,(unsigned)m68k_get_reg(NULL,M68K_REG_D0),
                (unsigned)m68k_get_reg(NULL,M68K_REG_A0));return 1;
    }
    for(unsigned i=0;i<3u;i++)
        if(plan.primary_descriptor[i]!=jagm_read8(&mach,(uint32_t)m68k_get_reg(NULL,M68K_REG_A0)+i))return 1;
    if(id==0x80u){
        if(plan.secondary_descriptor_address!=AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE)return 1;
        for(unsigned i=0;i<3u;i++)
            if(plan.secondary_descriptor[i]!=jagm_read8(&mach,AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE+i))return 1;
    }else if(plan.secondary_descriptor_address)return 1;
    printf("original $F2D6 special %02x FILES=%u (JP80), descriptor=%06x, cache/slot/index matched, instructions=%u PASS\n",
           id,plan.file_index,plan.primary_descriptor_address,steps);
    return 0;
}
int main(int argc,char **argv)
{
    FILE *fp;
    if(argc!=2||(fp=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,sizeof rom,fp)!=sizeof rom||fgetc(fp)!=EOF){fclose(fp);return 2;}
    fclose(fp);
    if(jagm_init(&mach,rom,sizeof rom))return 2;
    jagm_run_frames(&mach,40);
    memcpy(&boot,&mach,sizeof boot);
    for(unsigned i=0;i<13u;i++)if(check_one(i))return 1;
    puts("S42 13 retail special-panel descriptor lookups match original 68000 BEFORE JP80 dispatch");
    return 0;
}
