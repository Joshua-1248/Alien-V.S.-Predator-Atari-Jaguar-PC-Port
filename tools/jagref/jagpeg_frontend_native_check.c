/* RE24 S27: ROM-gated, locally regenerated native-C parity of the ORIGINAL
 * front-end JagPEG GPU program. NOT linked to the playable PC port.
 * The generated instruction translation is produced from the user's cartridge
 * in a temporary build directory and must never enter release archives.
 */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/original_view_native.h"
#include "jaguar/avp_gpu/jagpeg_load.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BANK (g->bank)
#ifdef FRONTEND_SOURCE
#ifndef FRONTEND_IMAGE_HASH
#error "ROM-gated generation must supply the front-end code hash"
#endif
#include FRONTEND_SOURCE
static uint64_t code_hash(const uint8_t *p)
{
    uint64_t h=14695981039346656037ull;
    for(unsigned i=0;i<0xA00u;i++){h^=p[i];h*=1099511628211ull;}
    return h;
}
#endif
#define GPU_ENTRY 0xF03000u
#define ROM_BYTES (4u*1024u*1024u)
#define MAX_BLITS 18000u
#define STEP_LIMIT 120000000ul
typedef struct BlitRec {uint32_t v[19];} BlitRec;
static BlitRec orig_blits[MAX_BLITS],native_blits[MAX_BLITS];
static unsigned no,nn;
static JagMachine machine,A,B;
#ifdef FRONTEND_SOURCE
static AvpGpuRecompCtx *active_ctx;
#endif
static void record(BlitRec *r,const JagBlitter *b){
 const uint32_t v[19]={b->cmd,b->count,b->a1_base,b->a1_flags,b->a1_pixel,b->a1_fpixel,
 b->a1_inc,b->a1_finc,b->a1_step,b->a1_fstep,b->a1_clip,b->a2_base,b->a2_flags,
 b->a2_pixel,b->a2_step,b->iinc,(uint32_t)(b->patd>>32),(uint32_t)b->patd,b->zinc};
 memcpy(r->v,v,sizeof v);
}
static void obs_orig(void *unused,const JagBlitter *b){(void)unused;if(no<MAX_BLITS)record(&orig_blits[no],b);no++;}
static void obs_native(void *unused,const JagBlitter *b){(void)unused;if(nn<MAX_BLITS)record(&native_blits[nn],b);nn++;}
#ifdef FRONTEND_SOURCE
static uint32_t rd(void *ctx,uint32_t a,int w){
 JagMachine *m=ctx;
 if(w==4 && (a&0xFFFFFCu)==0xF02100u && active_ctx){
    /* The native C engine owns live arithmetic flags; the reference bus
       does not execute JRISC arithmetic on the native side. A GPU FLAGS
       read must combine those live flags with the real machine's control
       bits rather than returning the stale reference CPU-side flags. */
    uint32_t f=jagm_read32(m,a)&~7u;
    return f|(active_ctx->z?1u:0u)|(active_ctx->c?2u:0u)|(active_ctx->n?4u:0u);
 }
 return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);
}
static void wr(void *ctx,uint32_t a,uint32_t v,int w){
 JagMachine *m=ctx;
 if(w==1)jagm_write8(m,a,(uint8_t)v);
 else if(w==2)jagm_write16(m,a,(uint16_t)v);
 else jagm_write32(m,a,v);
 if(active_ctx && a==AVP_JPG_MBOX_DONE && w==4 && v==AVP_JPG_DONE_MAGIC)active_ctx->stop=1;
 if(active_ctx && a==0xF02100u && w==4){
    uint32_t flags=m->gpu.flags;
    active_ctx->bank=(flags&8u)?0u:((flags>>14)&1u);
    active_ctx->z=flags&1u;active_ctx->c=(flags>>1)&1u;active_ctx->n=(flags>>2)&1u;
 }
}
#endif
static void fix(JagMachine *m){m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int is_front(JagMachine *m){
 uint32_t rec=jagm_read32(m,AVP_JPG_MBOX_RECORD);
 return jagm_read8(m,AVP_JPG_VAR_MODE)==1u &&
   jagm_read32(m,GPU_ENTRY)==0x981E3F54u && jagm_read32(m,GPU_ENTRY+4)==0x00F08C3Fu &&
   rec>=0x800000u && rec<=0xBFFFFFu && jagm_read16(m,rec+4u)!=0 && jagm_read16(m,rec+6u)!=0;
}
static unsigned pad_start[5]={950,1150,1400,1700,2000};
static void framepad(JagMachine *m,void *ctx){
 uint32_t mask=0;(void)ctx;
 for(unsigned i=0;i<5;i++)if(m->frames>=pad_start[i]&&m->frames<pad_start[i]+6u)mask=0x20000u;
 jagm_set_pad(m,0,mask);
}
static int compare_job(uint64_t field){
 unsigned long steps=0;
#ifdef FRONTEND_SOURCE
 AvpGpuRecompCtx g;unsigned blit_diff=0,dr_diff=0,gpu_diff=0,reg_diff=0;int arch_bad=0;
 uint32_t end;
#endif
 memcpy(&A,&machine,sizeof A);memcpy(&B,&machine,sizeof B);fix(&A);fix(&B);
 no=nn=0;A.blit.on_blit=obs_orig;B.blit.on_blit=obs_native;
 while(steps<STEP_LIMIT && A.gpu.go && jagm_read32(&A,AVP_JPG_MBOX_DONE)!=AVP_JPG_DONE_MAGIC){
   if(!jrisc_step(&A.gpu))break;
   steps++;
 }
 if(jagm_read32(&A,AVP_JPG_MBOX_DONE)!=AVP_JPG_DONE_MAGIC){
   fprintf(stderr,"original decoder failed at field %llu (PC=%06X steps=%lu)\n",
           (unsigned long long)field,A.gpu.pc,steps);return 1;
 }
#ifndef FRONTEND_SOURCE
 printf("capture field %llu; original instructions %lu blits %u\n",(unsigned long long)field,steps,no);
 return 0;
#else
 memset(&g,0,sizeof g);memcpy(g.r,B.gpu.reg,sizeof g.r);
 g.bank=(B.gpu.flags&8u)?0u:((B.gpu.flags>>14)&1u);
 g.z=B.gpu.flags&1u;g.c=(B.gpu.flags>>1)&1u;g.n=(B.gpu.flags>>2)&1u;
 g.divoff=(int)(B.gpu.divctrl&1u);g.remain=B.gpu.remain;g.acc=B.gpu.acc;
 if(code_hash(B.gpu.ram)!=(uint64_t)FRONTEND_IMAGE_HASH){
    fprintf(stderr,"front-end GPU code hash differs; regenerate for this exact original program\n");
    return 1;
 }
 g.ctx=&B;g.rd=rd;g.wr=wr;
 active_ctx=&g;
 {end=avp_jagpeg_frontend_generated(&g,GPU_ENTRY);
  active_ctx=NULL;
  if(!g.stop||g.fault||end==0xFFFFFFFFu){
    fprintf(stderr,"native program incomplete at %06X stop=%d fault=%06X bank=%u flags=%08X bank0r28=%08X bank1r28=%08X\n",end,g.stop,g.fault,g.bank,B.gpu.flags,g.r[0][28],g.r[1][28]);
    return 1;
  }
 }
 for(unsigned i=0;i<no&&i<nn&&i<MAX_BLITS;i++)if(memcmp(orig_blits[i].v,native_blits[i].v,sizeof orig_blits[i].v))blit_diff++;
 for(unsigned i=0;i<JAGM_RAM_SIZE;i++)if(A.ram[i]!=B.ram[i])dr_diff++;
 for(unsigned i=0;i<A.gpu.ram_size;i++)if(A.gpu.ram[i]!=B.gpu.ram[i])gpu_diff++;
 for(unsigned bank=0;bank<2u;bank++)for(unsigned r=0;r<32u;r++)
    if(A.gpu.reg[bank][r]!=g.r[bank][r]){
       fprintf(stderr,"register difference bank=%u r%u original=%08X generated=%08X\n",
          bank,r,A.gpu.reg[bank][r],g.r[bank][r]);
       reg_diff++;
    }
 {
  uint32_t actual_flags=(B.gpu.flags&~7u)|(g.z?1u:0u)|(g.c?2u:0u)|(g.n?4u:0u);
  unsigned bank=(actual_flags&8u)?0u:((actual_flags>>14)&1u);
  arch_bad=(reg_diff!=0||end!=A.gpu.pc||actual_flags!=A.gpu.flags||bank!=g.bank||
            g.acc!=A.gpu.acc||g.remain!=A.gpu.remain||
            g.divoff!=(int)(A.gpu.divctrl&1u));
  printf("  GPU architecture: register differences=%u, flags=%08X/%08X, exit PC=%06X/%06X, %s\n",
         reg_diff,A.gpu.flags,actual_flags,A.gpu.pc,end,arch_bad?"DIFFERENT":"IDENTICAL");
 }
 printf("field %llu original %lu instructions blits %u/%u mismatches %u DRAM %u GPU RAM %u %s\n",
 (unsigned long long)field,steps,no,nn,blit_diff,dr_diff,gpu_diff,
 (no==nn&&no<=MAX_BLITS&&!blit_diff&&!dr_diff&&!gpu_diff&&!arch_bad)?"IDENTICAL":"DIFFERENT");fflush(stdout);
 return no!=nn||no>MAX_BLITS||blit_diff||dr_diff||gpu_diff||arch_bad;
#endif
}
int main(int argc,char **argv){
 FILE *f;uint8_t *rom;int capture=(argc==5 && !strcmp(argv[3],"--capture"));
 int first=argc>2?2:0,last=capture?3:argc;int failures=0;
 if(argc<3){fputs("usage: check verified_ROM field [additional-fields...] OR check verified_ROM 186 --capture private-output\n",stderr);return 2;}
 f=fopen(argv[1],"rb");if(!f){perror("ROM");return 2;}
 if(fseek(f,0,SEEK_END)||ftell(f)!=(long)ROM_BYTES||fseek(f,0,SEEK_SET)){fclose(f);return 2;}
 rom=malloc(ROM_BYTES);
 if(!rom||fread(rom,1,ROM_BYTES,f)!=ROM_BYTES){fclose(f);free(rom);return 2;}
 fclose(f);
 if(jagm_init(&machine,rom,ROM_BYTES)){free(rom);return 2;}
 machine.on_frame=framepad;
 for(int arg=first;arg<last;arg++){
  unsigned field=(unsigned)strtoul(argv[arg],NULL,10);int found=0;
  if(!field||(arg>first&&field<=(unsigned)strtoul(argv[arg-1],NULL,10))){
     fputs("fields must be positive and strictly increasing\n",stderr);free(rom);return 2;
  }
  while(machine.frames<=field+20u){
   if(!jagm_run_until_gpu_pc(&machine,GPU_ENTRY,(field>20?field-20:0),field+20u))break;
   if(is_front(&machine)&&machine.frames>=field){
     if(capture){
        FILE *o=fopen(argv[4],"wb");
        if(!o){free(rom);return 2;}
        if(fwrite(machine.gpu.ram,1,machine.gpu.ram_size,o)!=machine.gpu.ram_size){fclose(o);free(rom);return 2;}
        fclose(o);
     }
     failures+=compare_job(machine.frames);
     found=1;
     if(!jrisc_step(&machine.gpu)){
         fputs("GPU failed to advance after image entry\n",stderr);failures++;}
     break;
   }
   if(!jrisc_step(&machine.gpu))break;
  }
  if(!found){
     fprintf(stderr,"original front-end decoder not reached around field %u\n",field);
     free(rom);return 3;
  }
 }
 free(rom);return failures?1:0;
}
