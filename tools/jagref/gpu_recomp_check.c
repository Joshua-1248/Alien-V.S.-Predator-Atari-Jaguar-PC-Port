/* RE24-S13: equivalence check of a statically recompiled original GPU routine
 * against execution of the same ROM GPU code on the TRM JRISC core.
 * Both start from one identical machine state captured on jagref; every
 * Blitter command (all registers) and the destination frame memory must match.
 *   gpu_recomp_check AVP.jag FIELD [start:len:mask,...]
 * Build: -DRECOMP_SOURCE="x.c" -DRECOMP_LO/HI/ENTRY/FN [-DRECOMP_ENTRY_WORD] [-DRECOMP_EXIT];
 * env BLITS=n stops both sides after n Blitter commands.  The whole-view check
 * is tools/jagref/gpu_pipeline_check.c (driven by gpu_pipeline_parity.sh). */
#include "jaguar/jag_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AvpGpuRecompCtx {
    uint32_t r[2][32]; unsigned bank; int z,c,n,divoff; uint32_t remain; int64_t acc; uint32_t fault; volatile int stop;
    void *ctx; uint32_t (*rd)(void*,uint32_t,int); void (*wr)(void*,uint32_t,uint32_t,int);
} AvpGpuRecompCtx;
#define BANK (g->bank)
#include RECOMP_SOURCE

typedef struct BlitRec { uint32_t v[19]; } BlitRec;
static BlitRec la[4096],lb[4096]; static unsigned na,nb,limit=0xFFFFFFFFu; static volatile int *stopflag;
static void logblit(void *ctx,const JagBlitter *b){
    BlitRec *r=ctx==(void*)la?&la[na<4096?na++:4095]:&lb[nb<4096?nb++:4095];
    uint32_t v[19]={b->cmd,b->count,b->a1_base,b->a1_flags,b->a1_pixel,b->a1_fpixel,b->a1_inc,b->a1_finc,b->a1_step,b->a1_fstep,b->a1_clip,
                    b->a2_base,b->a2_flags,b->a2_pixel,b->a2_step,b->iinc,(uint32_t)(b->patd>>32),(uint32_t)b->patd,b->zinc};
    memcpy(r->v,v,sizeof v);
    if(ctx==(void*)lb&&nb>=limit&&stopflag)*stopflag=1;
}
static uint32_t brd(void *c,uint32_t a,int w){JagMachine *m=c;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void bwr(void *c,uint32_t a,uint32_t v,int w){JagMachine *m=c;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static JagMachine A,B;
static void fix(JagMachine *m){m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}

int main(int argc,char **argv)
{
    FILE *f;long n;uint8_t *rom;unsigned field=argc>2?(unsigned)atoi(argv[2]):2199u;
    static const unsigned presses[]={950,1150,1400,1700,2000};
    const uint32_t lo=RECOMP_LO,hi=RECOMP_HI;
    if(argc<2)return 2;
    f=fopen(argv[1],"rb");if(!f)return 1;fseek(f,0,SEEK_END);n=ftell(f);fseek(f,0,SEEK_SET);
    rom=malloc((size_t)n);if(!rom||fread(rom,1,(size_t)n,f)!=(size_t)n)return 1;fclose(f);
    if(jagm_init(&A,rom,(size_t)n))return 1;
    while(A.frames<field){
        uint32_t mask=0;for(unsigned i=0;i<5;i++)if(A.frames>=presses[i]&&A.frames<presses[i]+6)mask=0x20000u;
        { const char *e=argc>3?argv[3]:NULL;while(e&&*e){unsigned st,ln;unsigned long mk;int used=0;if(sscanf(e,"%u:%u:%lx%n",&st,&ln,&mk,&used)<3)break;if(A.frames>=st&&A.frames<st+ln)mask|=(uint32_t)mk;e+=used;if(*e==',')e++;} }
        jagm_set_pad(&A,0,mask);jagm_run_frames(&A,1);
    }
    for(;;){
        if(!jagm_run_until_gpu_pc(&A,RECOMP_ENTRY,field,field+20)){puts("entry not reached");return 3;}
#ifdef RECOMP_ENTRY_WORD
        /* the same PC can belong to another overlay: require the routine's first word */
        if(jagm_read16(&A,RECOMP_ENTRY)==RECOMP_ENTRY_WORD)break;
        jrisc_step(&A.gpu);
#else
        break;
#endif
    }
    if(getenv("BLITS"))limit=(unsigned)atoi(getenv("BLITS"));
    memcpy(&B,&A,sizeof A);fix(&B);
    A.blit.on_blit=logblit;A.blit.on_blit_ctx=la;B.blit.on_blit=logblit;B.blit.on_blit_ctx=lb;
    /* reference: the ROM GPU code on the JRISC core */
    { unsigned long k=0; while(A.gpu.pc>=lo&&A.gpu.pc<hi&&k<20000000ul&&na<limit
#ifdef RECOMP_EXIT
        &&A.gpu.pc!=RECOMP_EXIT
#endif
        ){jrisc_step(&A.gpu);k++;} }
    /* recompiled routine */
    { AvpGpuRecompCtx g;uint32_t out;memset(&g,0,sizeof g);memcpy(g.r,B.gpu.reg,sizeof g.r);
      g.bank=(B.gpu.flags&8u)?0u:((B.gpu.flags>>14)&1u);g.z=B.gpu.flags&1u;g.c=(B.gpu.flags>>1)&1u;g.n=(B.gpu.flags>>2)&1u;
      g.divoff=B.gpu.divctrl&1u;g.remain=B.gpu.remain;g.ctx=&B;g.rd=brd;g.wr=bwr;g.acc=B.gpu.acc;stopflag=&g.stop;
      out=RECOMP_FN(&g,RECOMP_ENTRY);if(g.fault)printf("recompiled fault at %06x\n",g.fault);
      printf("exit pc: jrisc=%06x recompiled=%06x\n",A.gpu.pc,out);
      { int regdiff=0;for(int bk=0;bk<2;bk++)for(int i=0;i<32;i++)if(g.r[bk][i]!=A.gpu.reg[bk][i])regdiff++;
        printf("register file differences: %d\n",regdiff); } }
    { unsigned bad=0;for(unsigned i=0;i<na&&i<nb;i++)if(memcmp(la[i].v,lb[i].v,sizeof la[i].v))bad++;
      printf("blits: jrisc=%u recompiled=%u, differing=%u\n",na,nb,bad);
      unsigned memdiff=0;for(uint32_t a=0;a<0x200000u;a++)if(A.ram[a]!=B.ram[a])memdiff++;
      printf("DRAM bytes differing: %u (meaningful only for whole-routine runs)\n",memdiff);
      if(limit!=0xFFFFFFFFu)return (na==nb&&!bad)?0:1;
      return (na==nb&&!bad&&!memdiff)?0:1; }
}
