/* RE25 Sweep 38: real retail-ROM FILES/JagPEG native bridge verification.
 * No ROM-derived bytes or decoded media are written to disk.
 * The original 68000 starts a level JPEG on an independently emulated Jaguar;
 * the original GPU decoder and converter run to completion. A clone of the
 * same pre-decode hardware state runs the native FILES/JagPEG expansion bridge.
 * Compare the original converter's final pixels, RGB scratch, and the
 * decoder's complete Blitter command stream against the native port.
 * No physical-console or timing parity claim. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_jpeg_native_expand.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define JPEG_ENTRY 0xF03000u
#define CONVERT_ENTRY 0xF03A00u
#define JPEG_HEAD 0x981E3F54u
#define JPEG_NEXT 0x00F08C3Fu
#define GPU_STEP_LIMIT 120000000ul
#define MAX_BLITS 16000u

typedef struct BlitRec {uint32_t fields[19];} BlitRec;
static BlitRec a_blits[MAX_BLITS],b_blits[MAX_BLITS];
static unsigned a_n,b_n;
static JagMachine A,B;
static unsigned character;
static unsigned changed_output_jobs,changed_rgb_jobs;
static void capture(BlitRec *r,const JagBlitter *b){
 const uint32_t s[19]={b->cmd,b->count,b->a1_base,b->a1_flags,b->a1_pixel,
 b->a1_fpixel,b->a1_inc,b->a1_finc,b->a1_step,b->a1_fstep,b->a1_clip,
 b->a2_base,b->a2_flags,b->a2_pixel,b->a2_step,b->iinc,
 (uint32_t)(b->patd>>32),(uint32_t)b->patd,b->zinc};
 memcpy(r->fields,s,sizeof s);
}
static void watch_a(void *p,const JagBlitter *b){(void)p;if(a_n<MAX_BLITS)capture(&a_blits[a_n],b);a_n++;}
static void watch_b(void *p,const JagBlitter *b){(void)p;if(b_n<MAX_BLITS)capture(&b_blits[b_n],b);b_n++;}
static void fix(JagMachine *m){m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static uint32_t rd(void *ctx,uint32_t a,int w){JagMachine *m=ctx;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *ctx,uint32_t a,uint32_t v,int w){JagMachine *m=ctx;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void on_frame(JagMachine *m,void *p){
 static const unsigned press[]={950u,1150u,1400u,1700u,2000u};
 uint32_t mask=0;unsigned i;(void)p;
 for(i=0;i<5;i++)if(m->frames>=press[i]&&m->frames<press[i]+6u)mask=0x20000u;
 if(m->frames>=1250u&&m->frames<1254u){if(character==1u)mask|=8u;if(character==2u)mask|=4u;}
 if(character==2u&&m->frames>=2300u&&m->frames<2306u)mask|=0x20000u;
 jagm_set_pad(m,0u,mask);
}
static int is_level_job(JagMachine *m){
 uint32_t payload=jagm_read32(m,AVP_JPG_MBOX_RECORD);
 return jagm_read32(m,JPEG_ENTRY)==JPEG_HEAD && jagm_read32(m,JPEG_ENTRY+4u)==JPEG_NEXT &&
 jagm_read8(m,AVP_JPG_VAR_MODE)==2u && payload>=0x800008u && payload<0xC00000u &&
 (jagm_read32(m,payload-8u)>>16)==0x4A50u &&
 jagm_read16(m,payload+4u)>0u&&jagm_read16(m,payload+6u)>0u;
}
static int gpu_complete(JagMachine *m,uint32_t addr,uint32_t expected){
 unsigned long steps=0;
 while(jagm_read32(m,addr)!=expected&&m->gpu.go&&steps<GPU_STEP_LIMIT){
  if(!jrisc_step(&m->gpu))break;
  steps++;
 }
 return jagm_read32(m,addr)==expected;
}
static int compare_one(unsigned number){
 uint32_t rec=jagm_read32(&A,AVP_JPG_MBOX_RECORD);
 uint32_t dest=m68k_get_reg(NULL,M68K_REG_A1),scratch=jagm_read32(&A,AVP_JPG_VAR_SCRATCH);
 uint32_t x=jagm_read16(&A,rec+4u),y=jagm_read16(&A,rec+6u);
 uint64_t pixels=(uint64_t)x*y*64u;
 uint32_t nbytes=(uint32_t)(pixels*2u),nrgba=(uint32_t)(pixels*4u);
 unsigned mismatches=0,scratch_bad=0,blit_bad=0,decoder_blits;
 unsigned changed_output=0,changed_rgb=0;
 AvpView3DBus bus;
 AvpJagFilesJpegNativeExpand adapter;
 uint32_t expected_job_dest=jagm_read32(&A,AVP_JPG_MBOX_DEST);
 if(pixels==0u||pixels>0x10000u||scratch==0u||
    (uint64_t)scratch+nrgba>0x200000u||(uint64_t)dest+nbytes>0x200000u ||
    expected_job_dest!=scratch || dest==scratch){
  fprintf(stderr,"job %u invalid original addresses/geometry: dest=%06x scratch=%06x %u x %u\n",number,dest,scratch,x,y);return 1;
 }
 memcpy(&B,&A,sizeof A);fix(&B);
 a_n=b_n=0;
 A.blit.on_blit=watch_a;A.blit.on_blit_ctx=NULL;
 B.blit.on_blit=watch_b;B.blit.on_blit_ctx=NULL;
 bus.ctx=&B;bus.rd=rd;bus.wr=wr;
 memset(&adapter,0,sizeof adapter);
 adapter.bus=&bus;adapter.archive_begin=0x800000u;adapter.archive_end=0xC00000u;
 adapter.output_begin=dest;adapter.output_end=dest+nbytes;
 adapter.scratch_begin=scratch;adapter.scratch_end=scratch+nrgba;
 if(!avp_jagfiles_jpeg_native_expand(&adapter,rec,dest)||adapter.last_size!=nbytes){
   fprintf(stderr,"job %u native bridge refused an authentic job\n",number);return 1;
 }
 /* A is still at the original GPU entry; verify actual image changes,
  * not only equality against a potentially untouched destination. */
 for(uint32_t i=0;i<nbytes;i++)if(jagm_read8(&A,dest+i)!=jagm_read8(&B,dest+i))changed_output++;
 for(uint32_t i=0;i<nrgba;i++)if(jagm_read8(&A,scratch+i)!=jagm_read8(&B,scratch+i))changed_rgb++;
 if(changed_output)changed_output_jobs++;
 if(changed_rgb)changed_rgb_jobs++;
 if(!gpu_complete(&A,AVP_JPG_MBOX_DONE,AVP_JPG_DONE_MAGIC)){
   fprintf(stderr,"job %u original JPEG GPU decode did not finish\n",number);return 1;
 }
 decoder_blits=a_n;
 /* Verify exactly the original decoder's RGB output before conversion. */
 for(uint32_t i=0;i<nrgba;i++)if(jagm_read8(&A,scratch+i)!=jagm_read8(&B,scratch+i))scratch_bad++;
 if(!jagm_run_until_gpu_pc(&A,CONVERT_ENTRY,A.frames,A.frames+24u)){
   fprintf(stderr,"job %u original converter was not reached\n",number);return 1;
 }
 if(!gpu_complete(&A,0x7F80u,0u)){
   fprintf(stderr,"job %u original converter did not complete\n",number);return 1;
 }
 for(uint32_t i=0;i<nbytes;i++)if(jagm_read8(&A,dest+i)!=jagm_read8(&B,dest+i))mismatches++;
 if(decoder_blits!=b_n||decoder_blits>MAX_BLITS)blit_bad++;
 for(unsigned i=0;i<decoder_blits&&i<b_n&&i<MAX_BLITS;i++)if(memcmp(a_blits[i].fields,b_blits[i].fields,sizeof a_blits[i].fields))blit_bad++;
 printf("job %u field %llu destination=%06x pixels=%llu output_diff=%u RGB_diff=%u blits=%u/%u differing=%u %s\n",
 number,(unsigned long long)A.frames,dest,(unsigned long long)pixels,mismatches,scratch_bad,decoder_blits,b_n,blit_bad,
 (!mismatches&&!scratch_bad&&!blit_bad)?"IDENTICAL":"DIFFERENT");fflush(stdout);
 return mismatches||scratch_bad||blit_bad;
}
int main(int argc,char **argv){
 static uint8_t rom[ROM_BYTES];
 unsigned matched=0,failed=0,requested;
 uint64_t start,stop;
 FILE *f;
 if(argc!=6){fputs("usage: check VERIFIED_ROM alien|predator|marine first_field jobs last_field\n",stderr);return 2;}
 if(strcmp(argv[2],"alien")==0)character=0u;
 else if(strcmp(argv[2],"predator")==0)character=1u;
 else if(strcmp(argv[2],"marine")==0)character=2u;
 else return 2;
 start=strtoul(argv[3],NULL,10);requested=(unsigned)strtoul(argv[4],NULL,10);stop=strtoul(argv[5],NULL,10);
 if(!requested||requested>64u||start>=stop)return 2;
 f=fopen(argv[1],"rb");if(!f)return 2;
 if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}fclose(f);
 if(jagm_init(&A,rom,sizeof rom))return 2;
 A.on_frame=on_frame;
 while(matched<requested&&A.frames<stop){
  if(!jagm_run_until_gpu_pc(&A,JPEG_ENTRY,start,stop))break;
  if(!is_level_job(&A)){
   if(!jrisc_step(&A.gpu))break;
   continue;
  }
  matched++;
  if(compare_one(matched)){
   failed++;break;
  }
  /* Resume the cartridge's GPU from its converter return. */
  if(!jrisc_step(&A.gpu))break;
 }
 printf("S38 %s native FILES/JagPEG retail checks: %u/%u, failures=%u, changed_output_jobs=%u, changed_rgb_jobs=%u\n",argv[2],matched,requested,failed,changed_output_jobs,changed_rgb_jobs);
 return matched==requested&&!failed&&changed_output_jobs&&changed_rgb_jobs?0:1;
}
