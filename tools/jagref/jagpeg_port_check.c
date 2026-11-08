/* RE24 S26: retail-cartridge JagPEG GPU decode parity.
 *
 * Runs the user's ROM in jagref until the game loads its level-texture GPU
 * decoder, then clones the exact *pre-decoding* hardware state.  The JRISC
 * program and the native JagPEG C routine must produce byte-identical DRAM,
 * GPU local RAM, mailbox completion, and Blitter command streams.
 *
 * No decoded output or cartridge data are written to disk.  Unlike the
 * view parity harness this deliberately does not compare GPU registers: the
 * independent C decoder uses host-local temporary registers.
 */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/jagpeg_load.h"
#include "view3d_snapshot.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JPEG_ENTRY 0xF03000u
#define CONVERTER_ENTRY 0xF03A00u
#define JPEG_HEAD 0x981E3F54u
#define JPEG_NEXT 0x00F08C3Fu
#define JPEG_LIMIT 120000000ul
#define MAX_BLITS 16000u
#define ROM_BYTES (4u*1024u*1024u)

typedef struct BlitRec { uint32_t val[19]; } BlitRec;
static BlitRec original_blits[MAX_BLITS],native_blits[MAX_BLITS];
static unsigned original_count,native_count;
static JagMachine machine,original,native;
static enum { ALIEN, PREDATOR, MARINE } character=ALIEN;

static void blit_record(BlitRec *r,const JagBlitter *b)
{
    const uint32_t v[19]={b->cmd,b->count,b->a1_base,b->a1_flags,b->a1_pixel,b->a1_fpixel,
      b->a1_inc,b->a1_finc,b->a1_step,b->a1_fstep,b->a1_clip,b->a2_base,b->a2_flags,
      b->a2_pixel,b->a2_step,b->iinc,(uint32_t)(b->patd>>32),(uint32_t)b->patd,b->zinc};
    memcpy(r->val,v,sizeof v);
}
static void orig_blit(void *ctx,const JagBlitter *b)
{(void)ctx;if(original_count<MAX_BLITS)blit_record(&original_blits[original_count],b);original_count++;}
static void native_blit(void *ctx,const JagBlitter *b)
{(void)ctx;if(native_count<MAX_BLITS)blit_record(&native_blits[native_count],b);native_count++;}
static void fix_buses(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static uint32_t bus_read(void *ctx,uint32_t a,int w)
{JagMachine *m=(JagMachine*)ctx;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void bus_write(void *ctx,uint32_t a,uint32_t v,int w)
{JagMachine *m=(JagMachine*)ctx;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void on_frame(JagMachine *m,void *ctx)
{
    static const unsigned press[]={950,1150,1400,1700,2000};
    uint32_t mask=0;unsigned i;(void)ctx;
    for(i=0;i<5;i++)if(m->frames>=press[i]&&m->frames<press[i]+6u)mask=0x20000u;
    if(m->frames>=1250u&&m->frames<1254u){
        if(character==PREDATOR)mask|=8u;
        if(character==MARINE)mask|=4u;
    }
    if(character==MARINE&&m->frames>=2300u&&m->frames<2306u)mask|=0x20000u;
    jagm_set_pad(m,0,mask);
}
static int is_jpeg_entry(JagMachine *m)
{
    uint32_t record;
    if(jagm_read32(m,JPEG_ENTRY)!=JPEG_HEAD||jagm_read32(m,JPEG_ENTRY+4u)!=JPEG_NEXT)return 0;
    /* The retail front-end uses a distinct program and triple blits per
       block; it is NOT this level-texture decoder.  See S26 audit. */
    if(jagm_read8(m,AVP_JPG_VAR_MODE)!=2u)return 0;
    record=jagm_read32(m,AVP_JPG_MBOX_RECORD);
    if(record<0x800000u||record>0xBFFFFFu)return 0;
    if(jagm_read16(m,record+4u)==0u||jagm_read16(m,record+6u)==0u)return 0;
    return 1;
}
static int compare_decode(unsigned job_number,uint64_t field,int convert)
{
    unsigned bdiff=0,drdiff=0,grdiff=0;
    uint32_t firstdram=0,firstgpu=0;
    unsigned i;unsigned long steps=0;
    int prep_bad=0;
    AvpJagpegJob job;AvpView3DBus bus;
    memcpy(&original,&machine,sizeof original);memcpy(&native,&machine,sizeof native);
    fix_buses(&original);fix_buses(&native);
    original_count=native_count=0;
    original.blit.on_blit=orig_blit;original.blit.on_blit_ctx=NULL;
    native.blit.on_blit=native_blit;native.blit.on_blit_ctx=NULL;
    while(steps<JPEG_LIMIT && (convert? jagm_read32(&original,0x7F80u)!=0u : jagm_read32(&original,AVP_JPG_MBOX_DONE)!=AVP_JPG_DONE_MAGIC) && original.gpu.go){
        if(!jrisc_step(&original.gpu))break;
        steps++;
    }
    if(convert? jagm_read32(&original,0x7F80u)!=0u : jagm_read32(&original,AVP_JPG_MBOX_DONE)!=AVP_JPG_DONE_MAGIC){
        fprintf(stderr,"job %u: original GPU %s failed to complete after %lu instructions\n",job_number,convert?"converter":"decoder",steps);
        return 1;
    }
    bus.ctx=&native;bus.rd=bus_read;bus.wr=bus_write;
    job.record=jagm_read32(&native,AVP_JPG_MBOX_RECORD);
    job.dest=jagm_read32(&native,AVP_JPG_MBOX_DEST);
    job.width_blocks=jagm_read32(&native,AVP_JPG_MBOX_WBLOCKS);
    job.width_code=jagm_read32(&native,AVP_JPG_MBOX_WCODE);
    if(!convert){
        AvpJagpegJob prepared;
        avp_jagpeg_prepare(&bus,job.record,job.dest,&prepared);
        prep_bad=(prepared.record!=job.record||prepared.dest!=job.dest||
          prepared.width_blocks!=job.width_blocks||prepared.width_code!=job.width_code||
          jagm_read16(&native,AVP_JPG_VAR_WIDTH_PX)!=jagm_read16(&original,AVP_JPG_VAR_WIDTH_PX)||
          jagm_read16(&native,AVP_JPG_VAR_HEIGHT_PX)!=jagm_read16(&original,AVP_JPG_VAR_HEIGHT_PX)||
          jagm_read16(&native,AVP_JPG_VAR_BLIT_WIDTH)!=jagm_read16(&original,AVP_JPG_VAR_BLIT_WIDTH));
        if(prep_bad)fprintf(stderr,"job %u: native 68000 JagPEG prepare/mailer differs\n",job_number);
    }
    if(convert){
        AvpJagpegConvert cv;
        cv.pixels=jagm_read32(&native,0x7F80u);
        cv.source=jagm_read32(&native,0x7F84u);
        cv.dest=jagm_read32(&native,0x7F88u);
        cv.arg=jagm_read32(&native,0x7F8Cu);
        avp_jagpeg_gpu_convert(&bus,&cv);
    }else avp_jagpeg_gpu_decode(&bus,&job);
    for(i=0;i<original_count&&i<native_count&&i<MAX_BLITS;i++)
        if(memcmp(original_blits[i].val,native_blits[i].val,sizeof original_blits[i].val))bdiff++;
    for(uint32_t p=0;p<JAGM_RAM_SIZE;p++)if(original.ram[p]!=native.ram[p]){
        if(!drdiff)firstdram=p;
        drdiff++;
    }
    for(uint32_t p=0;p<original.gpu.ram_size;p++)if(original.gpu.ram[p]!=native.gpu.ram[p]){
        if(!grdiff)firstgpu=p;
        grdiff++;
    }
    printf("job %u field %llu: original GPU %lu instructions; blits %u/%u differing %u; DRAM differing %u; GPU local RAM differing %u; prepare %s; %s\n",
      job_number,(unsigned long long)field,steps,original_count,native_count,bdiff,drdiff,grdiff,prep_bad?"DIFFERENT":"IDENTICAL",
      (prep_bad==0&&bdiff==0&&drdiff==0&&grdiff==0&&original_count==native_count&&original_count<=MAX_BLITS)?"IDENTICAL":"DIFFERENT");
    if(drdiff)printf("  first differing DRAM location: %06X\n",firstdram);
    if(grdiff)printf("  first differing GPU RAM offset: %04X\n",firstgpu);
    fflush(stdout);
    return (prep_bad||bdiff||drdiff||grdiff||original_count!=native_count||original_count>MAX_BLITS)?1:0;
}
/* Capture/replay option only touches the verification tool. Each capture
 * contains cartridge-derived writable RAM and MUST remain private. */
static int run_fixture_mode(const char *rompath,const char *mode,const char *dir,
                            unsigned maxjobs,uint64_t minfield,uint64_t maxfield,int convert)
{
    char path[1024];unsigned matched=0,failed=0;
    const char *who=character==ALIEN?"alien":character==PREDATOR?"predator":"marine";
    const char *kind=convert?"convert":"decode";
    FILE *f=fopen(rompath,"rb");uint8_t *rom;
    uint64_t fp;
    const uint32_t entry=convert?CONVERTER_ENTRY:JPEG_ENTRY;
    int recording=!strcmp(mode,"--capture");
    if(!f){perror("open ROM");return 2;}
    if(fseek(f,0,SEEK_END)||ftell(f)!=(long)ROM_BYTES||fseek(f,0,SEEK_SET)){
        fclose(f);fputs("unexpected ROM length\n",stderr);return 2;
    }
    rom=malloc(ROM_BYTES);
    if(!rom||fread(rom,1,ROM_BYTES,f)!=ROM_BYTES){fclose(f);free(rom);return 2;}
    fclose(f);
    fp=avp_view3d_rom_fingerprint(rom,ROM_BYTES);
    if(jagm_init(&machine,rom,ROM_BYTES)){free(rom);return 2;}
    if(recording)machine.on_frame=on_frame;
    while(matched<maxjobs){
        int n=snprintf(path,sizeof path,"%s/%s_%s_%03u.avpjpeg",dir,who,kind,matched+1u);
        if(n<0||(size_t)n>=sizeof path){fputs("fixture path too long\n",stderr);failed++;break;}
        if(recording){
            if(!jagm_run_until_gpu_pc(&machine,entry,minfield,maxfield))break;
            if(convert?jagm_read32(&machine,0x7F80u)==0u:!is_jpeg_entry(&machine)){
                if(!jrisc_step(&machine.gpu)){failed++;break;}
                continue;
            }
            /* No overwriting existing private fixtures. */
            if(!avp_jagpeg_snapshot_write(path,&machine,fp,entry)){
                fprintf(stderr,"refusing/unable to capture %s\n",path);failed++;break;
            }
        }else if(!avp_jagpeg_snapshot_read(path,&machine,fp,entry)){
            fprintf(stderr,"missing, corrupt or mismatched private fixture %s\n",path);failed++;break;
        }
        matched++;
        if(!convert && !is_jpeg_entry(&machine)){
            fprintf(stderr,"invalid level decoder mailbox in job %u\n",matched);failed++;
        }else if(convert && jagm_read32(&machine,0x7F80u)==0u){
            fprintf(stderr,"invalid converter job %u\n",matched);failed++;
        }else if(compare_decode(matched,machine.frames,convert))failed++;
        if(recording && !jrisc_step(&machine.gpu)){failed++;break;}
    }
    printf("JagPEG %s %s: %u of %u jobs, %u differences\n",who,kind,matched,maxjobs,failed);
    free(rom);
    return failed||matched!=maxjobs;
}
int main(int argc,char **argv)
{
    FILE *f;uint8_t *rom;
    unsigned matched=0,failed=0,maxjobs;
    uint64_t minfield,maxfield;
    int convert;
    if(argc<2){fputs("usage: jagpeg_port_check AVP.jag [options]\n",stderr);return 2;}
    if(argc>=4&&(!strcmp(argv[2],"--capture")||!strcmp(argv[2],"--replay"))){
        /* ROM --capture|--replay PRIVATE_DIR [min-field jobs max-field mode character]
         * The script verifies the retail ROM SHA-256 before this entrypoint. */
        if(argc>9){fputs("too many fixture arguments\n",stderr);return 2;}
        minfield=argc>4?strtoull(argv[4],NULL,10):1600u;
        maxjobs=argc>5?(unsigned)strtoul(argv[5],NULL,10):13u;
        maxfield=argc>6?strtoull(argv[6],NULL,10):2100u;
        if(argc>7&&strcmp(argv[7],"convert")&&strcmp(argv[7],"decode")){
            fputs("unsupported JagPEG mode\n",stderr);return 2;
        }
        convert=argc>7&&!strcmp(argv[7],"convert");
        if(argc>8){
            if(!strcmp(argv[8],"predator"))character=PREDATOR;
            else if(!strcmp(argv[8],"marine"))character=MARINE;
            else if(strcmp(argv[8],"alien")){fputs("unknown character\n",stderr);return 2;}
        }
        if(maxjobs==0||(!strcmp(argv[2],"--capture")&&maxfield<=minfield))return 2;
        return run_fixture_mode(argv[1],argv[2],argv[3],maxjobs,minfield,maxfield,convert);
    }
    maxjobs=argc>3?(unsigned)strtoul(argv[3],NULL,10):13u;
    minfield=argc>2?strtoull(argv[2],NULL,10):1600u;
    maxfield=argc>4?strtoull(argv[4],NULL,10):2100u;
    convert=argc>5&&!strcmp(argv[5],"convert");
    if(argc>6){
        if(!strcmp(argv[6],"predator"))character=PREDATOR;
        else if(!strcmp(argv[6],"marine"))character=MARINE;
        else if(strcmp(argv[6],"alien")){fputs("unknown character selection\n",stderr);return 2;}
    }
    if(argc<2||maxjobs==0||maxfield<=minfield){
        fputs("usage: jagpeg_port_check AVP.jag [minfield jobs maxfield decode|convert char]\n",stderr);
        return 2;
    }
    f=fopen(argv[1],"rb");if(!f){perror("open ROM");return 2;}
    if(fseek(f,0,SEEK_END)!=0||ftell(f)!=(long)ROM_BYTES||fseek(f,0,SEEK_SET)!=0){fputs("unexpected ROM length\n",stderr);fclose(f);return 2;}
    rom=(uint8_t*)malloc(ROM_BYTES);
    if(!rom||fread(rom,1,ROM_BYTES,f)!=ROM_BYTES){fputs("ROM read failure\n",stderr);fclose(f);free(rom);return 2;}
    fclose(f);
    if(jagm_init(&machine,rom,ROM_BYTES)){fputs("reference machine initialization failed\n",stderr);free(rom);return 2;}
    machine.on_frame=on_frame;
    while(machine.frames<maxfield&&matched<maxjobs){
        if(!jagm_run_until_gpu_pc(&machine,convert?CONVERTER_ENTRY:JPEG_ENTRY,minfield,maxfield))break;
        if(convert ? jagm_read32(&machine,0x7F80u)!=0u : is_jpeg_entry(&machine)){
            matched++;
            if(compare_decode(matched,machine.frames,convert))failed++;
        }
        if(!jrisc_step(&machine.gpu)){fputs("GPU failed to advance\n",stderr);failed++;break;}
    }
    printf("JagPEG %s original-ROM comparisons: %u jobs, %u failures\n",convert?"converter":"decoder",matched,failed);
    free(rom);
    return (failed||matched!=maxjobs)?1:0;
}
