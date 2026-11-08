/* RE25 S47: JP80 original GPU decoder, naturally primed mode-2 context.
 * The retail program and QCONV are naturally loaded through original 68000.
 * At the known GPU instruction boundary, this TEST injects FILES #16 into
 * the original decoder's mailbox. Compare original GPU output against the
 * independently native-ported decoder and its full Blitter command stream.
 * This is a controlled decode experiment, NOT a naturally occurring panel
 * transaction, full overlay pipeline, or physical timing measurement.
 * No ROM bytes, pixel data, or snapshots are written to disk. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_dispatch.h"
#include "jaguar/avp_gpu/files_jpeg_init.h"
#include "jaguar/avp_gpu/jagpeg_load.h"
#include "jaguar/avp_gpu/panels_special_lookup.h"
#include "m68k.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define JPEG_ENTRY 0xf03000u
#define JPEG_LIMIT 20000000u
#define MAX_BLITS 1024u
static uint8_t rom[ROM_BYTES];
static JagMachine A,B,BOOT;
static uint32_t ab[MAX_BLITS][19],bb[MAX_BLITS][19];
static unsigned an,bn;
static void copy_blit(uint32_t dst[19],const JagBlitter *b)
{
    const uint32_t v[19]={b->cmd,b->count,b->a1_base,b->a1_flags,b->a1_pixel,
        b->a1_fpixel,b->a1_inc,b->a1_finc,b->a1_step,b->a1_fstep,b->a1_clip,
        b->a2_base,b->a2_flags,b->a2_pixel,b->a2_step,b->iinc,
        (uint32_t)(b->patd>>32),(uint32_t)b->patd,b->zinc};
    memcpy(dst,v,sizeof v);
}
static void wa(void *p,const JagBlitter *b){(void)p;if(an<MAX_BLITS)copy_blit(ab[an],b);an++;}
static void wb(void *p,const JagBlitter *b){(void)p;if(bn<MAX_BLITS)copy_blit(bb[bn],b);bn++;}
static void fix(JagMachine *m){m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static uint32_t rd(void *c,uint32_t a,int w)
{JagMachine *m=c;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *c,uint32_t a,uint32_t v,int w)
{JagMachine *m=c;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void on_frame(JagMachine *m,void *ctx)
{
    static const unsigned press[]={950u,1150u,1400u,1700u,2000u};
    uint32_t mask=0; (void)ctx;
    for(unsigned i=0;i<5u;i++)if(m->frames>=press[i]&&m->frames<press[i]+6u)mask=0x20000u;
    jagm_set_pad(m,0u,mask);
}
static int run_one(uint32_t file_id)
{
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,
        0xc00000u,AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    AvpJagFilesPlan plan;AvpJagpegJob job;
    AvpView3DBus bus={&A,rd,wr},other={&B,rd,wr};
    uint32_t scratch,rgbsz,blit_bad=0,mem_bad=0,first=0,oldtag,bx,by,changed=0;
    unsigned steps=0;
    memcpy(&A,&BOOT,sizeof A);fix(&A);
    jagm_run_frames(&A,0u); /* bind Musashi to THIS reference machine */
    if(jagm_read8(&A,AVP_JPG_VAR_MODE)!=2u ||
       !jagm_read8(&A,AVP_JPEG_CRY_LOADED) ||
       A.gpu.pc!=JPEG_ENTRY || !A.gpu.go){
        fputs("natural mode-2 JPEG prerequisites absent\n",stderr);return 1;
    }
    oldtag=jagm_read32(&A,AVP_JPEG_PREVIOUS_TAG);
    if(avp_jagfiles_plan(&bus,&cfg,file_id,&plan)!=AVP_JAGFILES_OK ||
       (plan.tag>>16)!=0x4a50u){
        fprintf(stderr,"no authentic JPEG FILES #%u\n",file_id);return 1;
    }
    scratch=jagm_read32(&A,AVP_JPG_VAR_SCRATCH);
    bx=jagm_read16(&A,plan.payload+4u);by=jagm_read16(&A,plan.payload+6u);
    if(!bx||!by||bx>16u||by>16u){
        fprintf(stderr,"unsupported original JPEG geometry %u,%u\n",bx,by);return 1;
    }
    rgbsz=bx*by*64u*4u;
    if(!scratch || (uint64_t)scratch+rgbsz>JAGM_RAM_SIZE){
        fprintf(stderr,"unsupported original JPEG scratch=%06x\n",scratch);return 1;
    }
    /* Original mode-2 ROM GPU program + authentic QCONV already installed.
     * Quantization is changed by the separately ROM-verified native Blitter
     * initializer. This is an intentionally INJECTED mailbox, not the
     * original 68000's uninterrupted panel-loader call path. */
    if((oldtag>>16)!=0x4a50u || !avp_jagfiles_jpeg_init_cached(&bus,plan.tag)){
        fprintf(stderr,"cannot set authentic JPEG quantization tag prev=%08x\n",oldtag);return 1;
    }
    wr(&A,AVP_JPEG_PREVIOUS_TAG,plan.tag,4);
    avp_jagpeg_prepare(&bus,plan.payload,0x0dd000u,&job);
    if(job.dest!=scratch || job.width_blocks!=bx){
        fprintf(stderr,"JPEG destination not scratch %06x != %06x\n",job.dest,scratch);return 1;
    }
    wr(&A,AVP_JPG_MBOX_RECORD,job.record,4);
    wr(&A,AVP_JPG_MBOX_DEST,job.dest,4);
    wr(&A,AVP_JPG_MBOX_WBLOCKS,job.width_blocks,4);
    wr(&A,AVP_JPG_MBOX_WCODE,job.width_code,4);
    wr(&A,AVP_JPG_MBOX_DONE,0u,4);
    memcpy(&B,&A,sizeof B);fix(&B);
    an=bn=0;
    A.blit.on_blit=wa;B.blit.on_blit=wb;
    A.blit.on_blit_ctx=NULL;B.blit.on_blit_ctx=NULL;
    avp_jagpeg_gpu_decode(&other,&job);
    while(steps<JPEG_LIMIT && A.gpu.go &&
          jagm_read32(&A,AVP_JPG_MBOX_DONE)!=AVP_JPG_DONE_MAGIC){
        if(!jrisc_step(&A.gpu))break;
        steps++;
    }
    if(jagm_read32(&A,AVP_JPG_MBOX_DONE)!=AVP_JPG_DONE_MAGIC){
        fprintf(stderr,"retail JP%04x FILES %u GPU DID NOT COMPLETE steps=%u pc=%06x done=%08x native=%08x\n",
            (unsigned)(plan.tag&0xffffu),file_id,steps,A.gpu.pc,
            jagm_read32(&A,AVP_JPG_MBOX_DONE),jagm_read32(&B,AVP_JPG_MBOX_DONE));return 1;
    }
    for(uint32_t i=0;i<rgbsz;i++){
        if(A.ram[scratch+i]!=BOOT.ram[scratch+i])changed++;
        if(A.ram[scratch+i]!=B.ram[scratch+i]){if(!mem_bad)first=scratch+i;mem_bad++;}
    }
    if(an!=bn||an!=bx*by||an>MAX_BLITS || !changed)blit_bad++;
    for(unsigned i=0;i<an&&i<bn&&i<MAX_BLITS;i++)
        if(memcmp(ab[i],bb[i],sizeof ab[i]))blit_bad++;
    printf("S47 FILES %u JP%c%c original mode2 GPU: %ux%u field=%llu instructions=%u blits=%u/%u RGB_bytes=%u changed=%u differing=%u first=%06x Blitter_cmd_differing=%u %s\n",
        file_id,(int)(plan.tag>>8&0xffu),(int)(plan.tag&0xffu),bx*8u,by*8u,
        (unsigned long long)A.frames,steps,an,bn,rgbsz,changed,mem_bad,first,blit_bad,
        (!mem_bad&&!blit_bad)?"PASS":"FAIL");
    fflush(stdout);
    return mem_bad||blit_bad;
}
int main(int argc,char **argv)
{
    FILE *f;unsigned ok=0;
    uint8_t refs[0x46u]={0};
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,ROM_BYTES,f)!=ROM_BYTES||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&A,rom,ROM_BYTES))return 2;
    A.on_frame=on_frame;
    /* Original game's panel descriptor table is live at field 40. It is
     * temporary RAM and is later reused by level loading; don't assert
     * the early-boot layout still exists at the natural JPEG boundary. */
    jagm_run_frames(&A,40u);
    for(unsigned special=0x80u;special<=0x8cu;special++){
        uint32_t desc=AVP_JAG_PANEL_SPECIAL_TABLE+((special&0x7fu)<<2);
        uint8_t main_id=jagm_read8(&A,desc),overlay=jagm_read8(&A,desc+1u);
        if(main_id!=16u || overlay<0x38u || overlay>0x45u){
            fprintf(stderr,"field40 retail descriptor %02x unexpected main=%u overlay=%u\n",special,main_id,overlay);
            return 1;
        }
        refs[overlay]++;
    }
    {
        uint8_t secondary=jagm_read8(&A,AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE);
        if(secondary<0x38u||secondary>0x45u)return 1;
        refs[secondary]++;
    }
    if(!jagm_run_until_gpu_pc(&A,JPEG_ENTRY,1640u,1760u)){
        fputs("natural original JPEG GPU launch not found\n",stderr);return 1;
    }
    memcpy(&BOOT,&A,sizeof BOOT);fix(&BOOT);
    for(uint32_t id=0x38u;id<=0x45u;id++)
        if(refs[id]!=1u){
            fprintf(stderr,"retail panel-overlay FILES #%u referenced %u times\n",id,refs[id]);
            return 1;
        }
    for(uint32_t file_id=0x38u;file_id<=0x45u;file_id++){
        if(run_one(file_id))return 1;
        ok++;
    }
    if(run_one(16u))return 1;
    ok++;
    printf("S47 original GPU mode2 JP80 main and 14 JP90 overlay retail records: %u/15 RGB images and complete Blitter streams identical to native\n",ok);
    return ok==15u?0:1;
}
