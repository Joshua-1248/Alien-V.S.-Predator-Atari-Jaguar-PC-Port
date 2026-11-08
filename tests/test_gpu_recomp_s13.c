/* RE24-S13: the JRISC->C static recompiler (tools/jagref/gpu_recomp.py) must
 * reproduce the TRM JRISC core exactly: registers, both banks, flags,
 * remainder, MAC and local memory, on a synthetic program covering ALU,
 * shifts/rotates, DIV, MULT/IMULT/IMULTN/IMACN/RESMAC, SAT, flags-conditional
 * JR/JUMP with delay slots, MOVETA/MOVEFA and all load/store forms.
 * Build: -DGEN_C="\"/tmp/x.c\"" -DGEN_BIN="\"/tmp/x.bin\"" (files from
 * tests/gen_gpu_recomp_synthetic_s13.py).  No ROM data is involved. */
#include "jaguar/jrisc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct AvpGpuRecompCtx {
    uint32_t r[2][32]; unsigned bank; int z,c,n,divoff; uint32_t remain; int64_t acc; uint32_t fault; volatile int stop;
    void *ctx; uint32_t (*rd)(void*,uint32_t,int); void (*wr)(void*,uint32_t,uint32_t,int);
} AvpGpuRecompCtx;
#define BANK (g->bank)
#include "../tools/jagref/gpu_arch_parity.h"
#include GEN_C
static uint32_t nobus_r(void *c,uint32_t a,int w){(void)c;(void)a;(void)w;assert(0);return 0;}
static void nobus_w(void *c,uint32_t a,uint32_t v,int w){(void)c;(void)a;(void)v;(void)w;assert(0);}
static JRisc A,Bj;
/* the recompiled routine accesses B's local RAM exactly as the core would */
static uint32_t rd(void *c,uint32_t a,int w){JRisc *j=c;uint32_t l=0;jrisc_local_read(j,a&~3u,&l);
    if(w==4)return l;
    if(w==2)return (a&2u)?(l&0xFFFFu):(l>>16);
    return (l>>(8u*(3u-(a&3u))))&0xFFu;}
static void wr(void *c,uint32_t a,uint32_t v,int w){JRisc *j=c;uint32_t l=0,sh;jrisc_local_read(j,a&~3u,&l);
    if(w==4)l=v; else if(w==2){sh=(a&2u)?0u:16u;l=(l&~(0xFFFFu<<sh))|((v&0xFFFFu)<<sh);} else {sh=8u*(3u-(a&3u));l=(l&~(0xFFu<<sh))|((v&0xFFu)<<sh);}
    jrisc_local_write(j,a&~3u,l);}
int main(void)
{
    JRiscBus b={0,nobus_r,nobus_w};FILE *f=fopen(GEN_BIN,"rb");uint8_t img[4096];
    AvpGpuRecompCtx g;uint32_t out;unsigned long k=0;
    assert(f&&fread(img,1,4096,f)==4096);fclose(f);
    jrisc_init(&A,JRISC_GPU,b);memcpy(A.ram,img,4096);A.pc=0xF03000u;A.go=1;
    memcpy(&Bj,&A,sizeof A);
    while(A.pc!=0xF03FF0u&&k<100000ul){jrisc_step(&A);k++;}
    assert(A.pc==0xF03FF0u);
    memset(&g,0,sizeof g);memcpy(g.r,Bj.reg,sizeof g.r);g.bank=(Bj.flags&8u)?0u:((Bj.flags>>14)&1u);
    g.ctx=&Bj;g.rd=rd;g.wr=wr;
    out=synthetic_recomp(&g,0xF03000u);
    assert(!g.fault);assert(out==0xF03FF0u);
    assert(avp_gpu_arch_diff(&A,&g,out)==0u);
    for(unsigned i=0;i<4096;i++)if(Bj.ram[i]!=A.ram[i])fprintf(stderr,"diff %03x core=%02x recomp=%02x\n",i,A.ram[i],Bj.ram[i]);
    assert(!memcmp(Bj.ram,A.ram,4096));
    printf("test_gpu_recomp_s13: %lu JRISC instructions reproduced exactly\n",k);
    return 0;
}
