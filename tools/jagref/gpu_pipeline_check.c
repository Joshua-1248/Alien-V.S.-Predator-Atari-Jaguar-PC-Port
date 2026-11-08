/* RE24-S13: whole 3D-view GPU program equivalence check.
 *
 * Runs the user's ROM on jagref until the GPU starts a view at $F03000 (on or
 * after FIELD), then executes the complete view twice from that identical
 * state:  A - the ROM's GPU code on the TRM JRISC core (overlays loaded by the
 * original sequencer through the Blitter);  B - the statically recompiled
 * stages from tools/jagref/gpu_pipeline_recomp.py.  Every Blitter command (all
 * registers), the final GPU registers, all 2 MiB of DRAM and the GPU local RAM
 * must be identical.
 *   DUMP=1 gpu_pipeline_check AVP.jag FIELD [input]   -> view_gpu.bin, view_dram.bin
 *   gpu_pipeline_check AVP.jag FIELD [input]          (built with PIPELINE_SOURCE)
 * Exit 0 = identical. */
#include "jaguar/jag_machine.h"
#include "jaguar/gpu_view_access.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jaguar/avp_gpu/original_view_native.h"
#define BANK (g->bank)
#include "gpu_arch_parity.h"
#ifdef PIPELINE_SOURCE
#include PIPELINE_SOURCE
#endif

#define VIEW_ENTRY 0xF03000u
#define VIEW_HALT  0xF03118u   /* "jr *" after the sequencer writes G_CTRL */
#define MAXB 8192u
typedef struct BlitRec { uint32_t v[19]; } BlitRec;
static BlitRec la[MAXB],lb[MAXB]; static unsigned na,nb;
static void logblit(void *ctx,const JagBlitter *b){
    BlitRec *r=ctx==(void*)la?&la[na<MAXB?na++:MAXB-1]:&lb[nb<MAXB?nb++:MAXB-1];
    uint32_t v[19]={b->cmd,b->count,b->a1_base,b->a1_flags,b->a1_pixel,b->a1_fpixel,b->a1_inc,b->a1_finc,b->a1_step,b->a1_fstep,b->a1_clip,
                    b->a2_base,b->a2_flags,b->a2_pixel,b->a2_step,b->iinc,(uint32_t)(b->patd>>32),(uint32_t)b->patd,b->zinc};
    memcpy(r->v,v,sizeof v);
}
static uint32_t brd(void *c,uint32_t a,int w){JagMachine *m=c;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void bwr(void *c,uint32_t a,uint32_t v,int w){JagMachine *m=c;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static JagMachine A,B;
static void fix(JagMachine *m){m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int dumpf(const char *p,const void *d,size_t n){FILE *o=fopen(p,"wb");if(!o)return 1;fwrite(d,1,n,o);fclose(o);return 0;}

int main(int argc,char **argv)
{
    FILE *f;long n;uint8_t *rom;unsigned field=argc>2?(unsigned)atoi(argv[2]):2199u;
    static const unsigned presses[]={950,1150,1400,1700,2000};
    if(argc<2)return 2;
    f=fopen(argv[1],"rb");if(!f)return 1;fseek(f,0,SEEK_END);n=ftell(f);fseek(f,0,SEEK_SET);
    rom=malloc((size_t)n);if(!rom||fread(rom,1,(size_t)n,f)!=(size_t)n)return 1;fclose(f);
    if(jagm_init(&A,rom,(size_t)n))return 1;
    /* S23: capture actual 68000 writes during boot/menu/level setup, then
     * stop before view execution. No cartridge bytes are retained/exported. */
    JagGpuViewMap *provenance=NULL;
    const char *provenance_path=getenv("GPU_SCENE_PROVENANCE_CSV");
    if(provenance_path&&!*provenance_path)provenance_path=NULL;
    if(provenance_path){
        provenance=(JagGpuViewMap*)calloc(1,sizeof *provenance);
        if(!provenance){fputs("GPU CPU provenance allocation failed\n",stderr);return 4;}
        A.on_cpu_dram_write=jag_gpu_view_cpu_write_observe;
        A.on_cpu_dram_write_ctx=provenance;
    }
    while(A.frames<field){
        uint32_t mask=0;for(unsigned i=0;i<5;i++)if(A.frames>=presses[i]&&A.frames<presses[i]+6)mask=0x20000u;
        { const char *e=argc>3?argv[3]:NULL;while(e&&*e){unsigned st,ln;unsigned long mk;int used=0;if(sscanf(e,"%u:%u:%lx%n",&st,&ln,&mk,&used)<3)break;if(A.frames>=st&&A.frames<st+ln)mask|=(uint32_t)mk;e+=used;if(*e==',')e++;} }
        jagm_set_pad(&A,0,mask);jagm_run_frames(&A,1);
    }
    if(!jagm_run_until_gpu_pc(&A,VIEW_ENTRY,field,field+40)){puts("view start not reached");return 3;}
    A.on_cpu_dram_write=NULL;A.on_cpu_dram_write_ctx=NULL;
    printf("view starts at field %llu\n",(unsigned long long)A.frames);
    if(getenv("DUMP")){return dumpf("view_gpu.bin",A.gpu.ram,4096)||dumpf("view_dram.bin",A.ram,JAGM_RAM_SIZE);}
#ifndef PIPELINE_SOURCE
    puts("built without PIPELINE_SOURCE");return 2;
#else
    memcpy(&B,&A,sizeof A);fix(&B);
    A.blit.on_blit=logblit;A.blit.on_blit_ctx=la;B.blit.on_blit=logblit;B.blit.on_blit_ctx=lb;
    /* S22: capture ONLY original JRISC view execution. No guessed addresses,
     * and never observe the recompiled B path or 68000 boot/menu traffic. */
    { JagGpuViewMap *accesses=NULL;
      const char *path=getenv("GPU_SCENE_ACCESS_CSV");
      if(path||provenance_path){
          accesses=provenance?provenance:(JagGpuViewMap*)calloc(1,sizeof *accesses);
          if(!accesses){fputs("GPU view access map allocation failed\n",stderr);return 4;}
          A.on_gpu_view_dram_access=jag_gpu_view_access_observe;
          A.on_gpu_view_dram_access_ctx=accesses;
      }
      { unsigned long k=0; while(A.gpu.pc!=VIEW_HALT&&k<200000000ul){jrisc_step(&A.gpu);k++;}
        printf("JRISC: %lu instructions to $%06X\n",k,A.gpu.pc);
        if(accesses){FILE *o;
          A.on_gpu_view_dram_access=NULL;A.on_gpu_view_dram_access_ctx=NULL;
          if(A.gpu.pc!=VIEW_HALT||accesses->bad_requests||accesses->counter_overflows){
              fprintf(stderr,"GPU view trace incomplete or invalid (pc=%06x bad=%llu overflow=%llu)\n",
                  A.gpu.pc,(unsigned long long)accesses->bad_requests,
                  (unsigned long long)accesses->counter_overflows);
              free(accesses);return 4;
          }
          if(accesses->cpu_bad_requests){
              fprintf(stderr,"invalid 68000 provenance requests: %llu\n",
                  (unsigned long long)accesses->cpu_bad_requests);
              free(accesses);return 4;
          }
          if(path){
              o=fopen(path,"w");
              if(!o){fprintf(stderr,"could not open GPU view access report\n");free(accesses);return 4;}
              { int valid=jag_gpu_view_access_csv(accesses,o);
                if(fclose(o)!=0)valid=0;
                if(!valid){fprintf(stderr,"could not write GPU view access report\n");
                           free(accesses);return 4;}
              }
          }
          if(provenance_path){
              o=fopen(provenance_path,"w");
              if(!o){fprintf(stderr,"could not open CPU provenance report\n");free(accesses);return 4;}
              { int valid=jag_gpu_view_provenance_csv(accesses,o);
                if(fclose(o)!=0)valid=0;
                if(!valid){fprintf(stderr,"could not write CPU provenance report\n");
                           free(accesses);return 4;}
              }
          }
          printf("GPU view access (GPU=%llu Blitter=%llu, DRAM-wrap=%llu, CPU pre-view=%llu)\n",
              (unsigned long long)accesses->gpu_requests,
              (unsigned long long)accesses->blitter_requests,
              (unsigned long long)accesses->wrap_requests,
              (unsigned long long)accesses->cpu_requests);
          free(accesses);
        }
      }
    }
    { AvpGpuRecompCtx g;uint32_t pc=VIEW_ENTRY;unsigned calls=0;int bad=0;
      memset(&g,0,sizeof g);memcpy(g.r,B.gpu.reg,sizeof g.r);
      g.bank=(B.gpu.flags&8u)?0u:((B.gpu.flags>>14)&1u);g.z=B.gpu.flags&1u;g.c=(B.gpu.flags>>1)&1u;g.n=(B.gpu.flags>>2)&1u;
      g.divoff=B.gpu.divctrl&1u;g.remain=B.gpu.remain;g.ctx=&B;g.rd=brd;g.wr=bwr;g.acc=B.gpu.acc;
#ifdef AVP_GPU_NATIVE_EXEC
      /* S19: invoke exactly the same native-C driver linked into avp_pc.
       * Its resident/overlay hash and 68000 MOVEI checks enforce safe reuse
       * across different level values. This is a ROM-gated verification tool;
       * it does not switch the live native game renderer. */
      { AvpGpuNativeStage stages[AVP_GPU_STAGE_COUNT];
        AvpGpuNativePipeline pipeline;
        AvpGpuNativeResult status;
#ifdef AVP_GPU_PARAM_COUNT
        AvpGpuNativeParam params[AVP_GPU_PARAM_COUNT ? AVP_GPU_PARAM_COUNT : 1u];
#endif
        for(unsigned j=0;j<AVP_GPU_STAGE_COUNT;j++){
            stages[j].entry=avp_gpu_stages[j].entry;
            stages[j].table=avp_gpu_stages[j].table;
            stages[j].src=avp_gpu_stages[j].src;
            stages[j].longs=avp_gpu_stages[j].longs;
            stages[j].fn=avp_gpu_stages[j].fn;
            stages[j].image_hash=avp_gpu_overlay_hashes[j];
        }
        pipeline.stages=stages;pipeline.stage_count=AVP_GPU_STAGE_COUNT;
        pipeline.entry=VIEW_ENTRY;pipeline.halt=VIEW_HALT;
        pipeline.resident_hash=avp_gpu_resident_hash;
#ifdef AVP_GPU_PARAM_COUNT
        for(unsigned j=0;j<AVP_GPU_PARAM_COUNT;j++){
            params[j].stage=avp_gpu_params[j].stage;
            params[j].gpu_pc=avp_gpu_params[j].gpu_pc;
            params[j].variable=avp_gpu_params[j].variable;
        }
        pipeline.params=params;pipeline.param_count=AVP_GPU_PARAM_COUNT;
#else
        pipeline.params=NULL;pipeline.param_count=0u;
#endif
        status=avp_gpu_native_run(&pipeline,&g,&pc,&calls);
        if(status!=AVP_GPU_NATIVE_OK){
            printf("native GPU driver rejected execution: %d\n",(int)status);
            bad=1;
        }
      }
#else
      while(pc!=VIEW_HALT&&!bad){
          unsigned st;
          for(st=0;st<AVP_GPU_STAGE_COUNT&&avp_gpu_stages[st].entry!=pc;st++){}
          if(st==AVP_GPU_STAGE_COUNT){printf("recompiled view left the stage set at $%06X\n",pc);bad=1;break;}
          if((jagm_read32(&B,avp_gpu_stages[st].table)&0x1FFFFFu)!=avp_gpu_stages[st].src){printf("stage %u overlay table changed\n",st);bad=1;break;}
#ifdef AVP_GPU_PARAM_COUNT
          /* S14: source-variable driven MOVEI parameters.  Fail closed if the
           * current memory is not the exact image the 68000 installed. */
          for(unsigned j=0;j<AVP_GPU_PARAM_COUNT;j++)if(avp_gpu_params[j].stage==st){
              uint32_t gpu_pc=avp_gpu_params[j].gpu_pc;
              uint32_t off=gpu_pc-0xF03220u;
              uint32_t src=avp_gpu_stages[st].src;
              uint32_t variable=jagm_read32(&B,avp_gpu_params[j].variable);
              uint32_t encoded=(uint32_t)jagm_read16(&B,src+off+2u)
                             |((uint32_t)jagm_read16(&B,src+off+4u)<<16);
              if(variable!=encoded||encoded!=avp_gpu_params[j].expected){
                  printf("stage %u GPU $%06X patch at RAM $%06X differs from original capture\n",
                         st,gpu_pc,avp_gpu_params[j].variable);
                  bad=1;break;
              }
              g.param[j]=variable;
          }
          if(bad)break;
#endif
          pc=avp_gpu_stages[st].fn(&g,pc);calls++;
          if(g.fault){printf("recompiled fault at $%06X\n",g.fault);bad=1;}
      }
#endif
      printf("recompiled: %u stage calls, ended at $%06X\n",calls,pc);
      { uint32_t archdiff=avp_gpu_arch_diff(&A.gpu,&g,pc);
        unsigned controldiff=0;
        if(A.gpu.go!=B.gpu.go)controldiff++;
        if(A.gpu.latches!=B.gpu.latches)controldiff++;
        if(A.gpu.ctrl_misc!=B.gpu.ctrl_misc)controldiff++;
        if(A.gpu.mtxc!=B.gpu.mtxc||A.gpu.mtxa!=B.gpu.mtxa||A.gpu.bigend!=B.gpu.bigend||
           A.gpu.hidata!=B.gpu.hidata||A.gpu.divctrl!=B.gpu.divctrl)controldiff++;
        unsigned bl=0,dr=0,gl=0;
        for(unsigned i=0;i<na&&i<nb;i++)if(memcmp(la[i].v,lb[i].v,sizeof la[i].v))bl++;
        for(uint32_t a=0;a<JAGM_RAM_SIZE;a++)if(A.ram[a]!=B.ram[a])dr++;
        for(uint32_t a=0;a<4096;a++)if(A.gpu.ram[a]!=B.gpu.ram[a])gl++;
        printf("architectural GPU differences (bitmask): 0x%02x\n"
               "control/MMIO differences: %u\n"
               "blits: jrisc=%u recompiled=%u differing=%u\n"
               "DRAM bytes differing: %u\nGPU RAM bytes differing: %u\n",
               archdiff,controldiff,na,nb,bl,dr,gl);
        return (!bad&&!archdiff&&!controldiff&&na==nb&&!bl&&!dr&&!gl)?0:1; } }
#endif
}
