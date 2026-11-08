/* RE25 S46: actual retail $F2D6 -> $BD44 -> $BD92 -> $BE48 JP80
 * 68000/GPU launch parity, ending BEFORE any GPU decoder instruction.
 *
 * No original ROM, decoded pixels, or GPU program bytes are exported.
 * This is deliberately NOT a GPU decode, overlay completion, or timing test.
 */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_dispatch.h"
#include "jaguar/avp_gpu/files_jpeg_init.h"
#include "jaguar/avp_gpu/files_jpeg_dispatch.h"
#include "jaguar/avp_gpu/jagpeg_load.h"
#include "jaguar/avp_gpu/panels_cached.h"
#include "jaguar/avp_gpu/panels_special_lookup.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ROM_BYTES 4194304u
#define PANEL_LIST 0x190000u
#define PANEL_HEAD 0x191000u
#define PANEL_PTR  0x191100u
#define STACK      0x6c00u
#define FILES_JSR  0x00f2f0u
#define GPU_ENTRY  0xf03000u
#define GPU_PC_REG 0xf02110u
#define GPU_GO_REG 0xf02114u
#define MAX_STEPS  12000u

static uint8_t rom[ROM_BYTES];
static JagMachine original,native,boot;
static uint32_t bus_read(void *ctx,uint32_t a,int w)
{
    JagMachine *m=ctx;
    return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);
}
static void bus_write(void *ctx,uint32_t a,uint32_t v,int w)
{
    JagMachine *m=ctx;
    if(w==1)jagm_write8(m,a,(uint8_t)v);
    else if(w==2)jagm_write16(m,a,(uint16_t)v);
    else jagm_write32(m,a,v);
}
static void rebind(JagMachine *m)
{
    m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;
}
static int compare_hardware(const JagMachine *a,const JagMachine *b,
                            unsigned *gpu_bad,unsigned *blit_bad)
{
    JRisc ra=a->gpu,rb=b->gpu;
    JagBlitter ba=a->blit,bb=b->blit;
    ra.bus.ctx=NULL;rb.bus.ctx=NULL;
    ba.bus.ctx=NULL;bb.bus.ctx=NULL;
    *gpu_bad=(unsigned)(memcmp(&ra,&rb,sizeof ra)!=0);
    *blit_bad=(unsigned)(memcmp(&ba,&bb,sizeof ba)!=0);
    return !*gpu_bad && !*blit_bad;
}
static int run_one(unsigned special_id,unsigned variant)
{
    static const uint32_t previous_tags[]={0x475a4950u,0x4a503930u,0x4a503830u};
    static const char *names[]={"first-install","JP-cache-requant","exact-JP-cache"};
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,
        0xc00000u,AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    AvpJagFilesPlan plan;
    AvpView3DBus bus={&native,bus_read,bus_write};
    AvpJagpegJob job;
    uint32_t destination,record,original_tag,geometry,first_dram=0;
    unsigned steps,dram_bad=0,gpu_bad=0,blit_bad=0,program_bad=0;
    uint64_t initial_blits;
    int rc;
    memcpy(&original,&boot,sizeof original);rebind(&original);
    jagm_write16(&original,PANEL_LIST,3u);
    jagm_write32(&original,PANEL_LIST+2u,PANEL_PTR);
    jagm_write32(&original,PANEL_LIST+6u,0u);
    jagm_write8(&original,PANEL_LIST+10u,(uint8_t)special_id);
    jagm_write8(&original,PANEL_LIST+11u,0xffu);
    jagm_write32(&original,PANEL_LIST+12u,0xffffffffu);
    jagm_write32(&original,PANEL_HEAD,PANEL_LIST);
    jagm_write8(&original,AVP_JAG_PANEL_CACHE_BASE+3u,0xfeu);
    jagm_write16(&original,AVP_JAG_PANEL_SPECIAL_INDEX,1u);
    jagm_write32(&original,AVP_JPEG_PREVIOUS_TAG,previous_tags[variant]);
    jagm_write32(&original,STACK,0x1000u);
    m68k_set_reg(M68K_REG_SR,0x2700u);
    m68k_set_reg(M68K_REG_SP,STACK);
    m68k_set_reg(M68K_REG_A5,PANEL_HEAD);
    m68k_set_reg(M68K_REG_PC,0xf1f6u);
    for(steps=0;steps<MAX_STEPS;steps++){
        if(m68k_get_reg(NULL,M68K_REG_PC)==FILES_JSR)break;
        (void)m68k_execute(1);
    }
    if(steps>=MAX_STEPS||m68k_get_reg(NULL,M68K_REG_PC)!=FILES_JSR){
        fprintf(stderr,"id %02x %s: original special-panel dispatch never reached FILES\n",
            special_id,names[variant]);return 1;
    }
    /* These are the actual input registers at JSR $BD44. */
    if(m68k_get_reg(NULL,M68K_REG_D0)!=16u ||
       m68k_get_reg(NULL,M68K_REG_A1)>0x1fffffu){
        fprintf(stderr,"id %02x: unexpected retail FILES index or destination\n",special_id);
        return 1;
    }
    destination=(uint32_t)m68k_get_reg(NULL,M68K_REG_A1);
    initial_blits=original.blit.blits;
    memcpy(&native,&original,sizeof native);rebind(&native);
    /* Original $BD44 initialization + $BE48 setup executes on Musashi. */
    for(steps=0;steps<MAX_STEPS && !original.gpu.go;steps++)
        (void)m68k_execute(1);
    if(steps>=MAX_STEPS || !original.gpu.go || original.gpu.pc!=GPU_ENTRY ||
       jagm_read8(&original,AVP_JPEG_MODE_VARIABLE)!=0u){
        fprintf(stderr,"id %02x %s: original never reached mode-0 GPU launch\n",
            special_id,names[variant]);return 1;
    }
    record=jagm_read32(&original,AVP_JPG_MBOX_RECORD);
    original_tag=jagm_read32(&original,AVP_JPEG_PREVIOUS_TAG);
    geometry=((uint32_t)jagm_read16(&original,AVP_JPG_VAR_WIDTH_PX)<<16)|
              jagm_read16(&original,AVP_JPG_VAR_HEIGHT_PX);
    rc=avp_jagfiles_plan(&bus,&cfg,16u,&plan);
    if(rc!=AVP_JAGFILES_OK||plan.tag!=0x4a503830u||
       plan.payload!=record||plan.init_pc!=AVP_JAGFILES_JPEG_INIT_PC||
       plan.expand_pc!=AVP_JAGFILES_JPEG_EXP_PC||
       (unsigned)plan.call_init!=(previous_tags[variant]!=plan.tag)){
        fprintf(stderr,"id %02x %s: authentic JP80 FILES plan mismatch\n",
            special_id,names[variant]);return 1;
    }
    if(plan.call_init){
        if((previous_tags[variant]>>16)==0x4a50u){
            if(!avp_jagfiles_jpeg_init_cached(&bus,plan.tag))return 1;
        }else if(!avp_jagfiles_jpeg_init_first(&bus,plan.tag))return 1;
        bus_write(&native,AVP_JPEG_PREVIOUS_TAG,plan.tag,4);
    }
    /* $BE48 writes geometry, then the four GPU job words and GO. */
    avp_jagpeg_prepare(&bus,plan.payload,destination,&job);
    bus_write(&native,AVP_JPG_MBOX_RECORD,job.record,4);
    bus_write(&native,AVP_JPG_MBOX_DEST,job.dest,4);
    bus_write(&native,AVP_JPG_MBOX_WBLOCKS,job.width_blocks,4);
    bus_write(&native,AVP_JPG_MBOX_WCODE,job.width_code,4);
    bus_write(&native,AVP_JPG_MBOX_DONE,0u,4);
    bus_write(&native,GPU_PC_REG,GPU_ENTRY,4);
    bus_write(&native,GPU_GO_REG,1u,4);
    for(uint32_t i=0;i<JAGM_RAM_SIZE;i++){
        if(i>=STACK-0x100u && i<STACK+8u)continue;
        if(original.ram[i]!=native.ram[i]){if(!dram_bad)first_dram=i;dram_bad++;}
    }
    (void)compare_hardware(&original,&native,&gpu_bad,&blit_bad);
    /* Compare the entire GPU local RAM via architectural bus reads as well. */
    for(uint32_t i=0;i<0x1000u;i++)
        if(jagm_read8(&original,GPU_ENTRY+i)!=jagm_read8(&native,GPU_ENTRY+i))program_bad++;
    if(original_tag!=plan.tag ||
       original.blit.blits-initial_blits!=(variant==0u?2u:variant==1u?1u:0u) ||
       native.blit.blits-initial_blits!=(variant==0u?2u:variant==1u?1u:0u) ||
       geometry!=0x00800080u ||
       jagm_read32(&original,AVP_JPG_MBOX_DEST)!=destination ||
       jagm_read32(&native,AVP_JPG_MBOX_DONE)!=0u ||
       dram_bad||gpu_bad||blit_bad||program_bad){
        fprintf(stderr,"id %02x %s: DRAM=%u first=%06x GPURAM=%u GPUstate=%u BLITstate=%u blits=%llu/%llu geometry=%08x\n",
            special_id,names[variant],dram_bad,first_dram,program_bad,
            gpu_bad,blit_bad,
            (unsigned long long)(original.blit.blits-initial_blits),
            (unsigned long long)(native.blit.blits-initial_blits),geometry);
        return 1;
    }
    printf("S46 JP80 %02x %s: original 68000/GPU launch exact, 2MiB DRAM (non-stack), 4KiB GPU, Blitter PASS\n",
        special_id,names[variant]);
    return 0;
}
int main(int argc,char **argv)
{
    FILE *f;
    unsigned passed=0;
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&original,rom,sizeof rom))return 2;
    jagm_run_frames(&original,40u);
    memcpy(&boot,&original,sizeof boot);
    for(unsigned v=0;v<3u;v++)for(unsigned i=0;i<13u;i++){
        if(run_one(0x80u+i,v))return 1;
        passed++;
    }
    printf("S46 authentic retail JP80 68000/GPU pre-decode launch: %u/39 identical\n",passed);
    return passed==39u?0:1;
}
