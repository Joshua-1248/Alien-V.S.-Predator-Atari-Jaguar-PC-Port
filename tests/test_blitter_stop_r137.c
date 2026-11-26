/* RE38 R137: Jaguar TOM Blitter collision stop, resume and abort.
 * Atari software reference B_STOP ($F02278): stop only on a comparator
 * write inhibit in pixel-XADD=1 mode when !BKGWREN && STOPEN.
 * These checks validate the synchronous register-level checkpoint, not
 * original silicon's bus cycle, collision readback latency or scheduling.
 */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRCEN (1u<<0)
#define SRCENZ (1u<<1)
#define DSTENZ (1u<<4)
#define DSTA2 (1u<<11)
#define PATDSEL (1u<<16)
#define ZLESS (1u<<18)
#define LFU_SRC (0xCu<<21)
#define BCOMPEN (1u<<26)
#define DCOMPEN (1u<<27)
#define BKGWREN (1u<<28)
#define UPDA1 (1u<<9)
#define UPDA2 (1u<<10)
#define A1 0x2000u
#define A2 0x1000u
#define PS8 (3u<<3)
#define PS16 (4u<<3)
#define XADD (1u<<16)
#define W8 (3u<<11)
#define W16 (4u<<11)

static uint8_t mem[0x5000];
static uint32_t writes;
static uint8_t rd(void *ctx,uint32_t addr){(void)ctx;assert(addr<sizeof mem);return mem[addr];}
static void wr(void *ctx,uint32_t addr,uint8_t v){(void)ctx;assert(addr<sizeof mem);mem[addr]=v;writes++;}
static void W(JagBlitter *b,uint32_t off,uint32_t val){assert(jag_blitter_write(b,off,val));}
static uint32_t R(JagBlitter *b,uint32_t off){uint32_t val=0;assert(jag_blitter_read(b,off,&val));return val;}
static void new_blit(JagBlitter *b,unsigned ps,int dsta2,int pixel_mode,uint32_t count){
    JagBlitBus bus={NULL,rd,wr};
    uint32_t flags=(ps==8u?PS8:PS16)|(ps==8u?W8:W16)|(pixel_mode?XADD:0u);
    memset(mem,0,sizeof mem);writes=0;jag_blitter_init(b,bus);
    W(b,0x00,dsta2?A2:A1);W(b,0x04,flags);W(b,0x0C,0);
    W(b,0x24,dsta2?A1:A2);W(b,0x28,flags);W(b,0x30,0);
    W(b,0x3C,count);
}
static void normal_copy(int dsta2){
    JagBlitter b;
    new_blit(&b,8,dsta2,1,(1u<<16)|5u);
    mem[A2]=0x11u;mem[A2+1]=0u;mem[A2+2]=0x22u;mem[A2+3]=0u;mem[A2+4]=0x33u;
    memset(&mem[A1],0xA5,5u);
    W(&b,0x68,0);W(&b,0x6C,0);
    W(&b,0x78,4u);
    W(&b,0x38,SRCEN|LFU_SRC|DCOMPEN|(dsta2?DSTA2:0u));
    assert(R(&b,0x38)==2u&&b.stopped);
    assert(mem[A1]==0x11u&&mem[A1+1]==0xA5u&&mem[A1+2]==0xA5u);
    assert(R(&b,dsta2?0x30u:0x0Cu)==2u);
    assert(writes==1u&&b.blits==1u);
    W(&b,0x78,4u);assert(R(&b,0x38)==2u); /* zero RESUME */
    W(&b,0x38,0u);assert(b.blits==1u);      /* new CMD can't restart mid-transfer */
    W(&b,0x78,5u);                         /* RESUME, keep STOPEN */
    assert(R(&b,0x38)==2u);
    assert(mem[A1+2]==0x22u&&mem[A1+3]==0xA5u&&mem[A1+4]==0xA5u);
    assert(writes==2u&&b.blits==1u);
    W(&b,0x78,1u);                         /* RESUME, clear STOPEN */
    assert((R(&b,0x38)&3u)==1u);
    assert(mem[A1+4]==0x33u&&writes==3u);
    assert(R(&b,dsta2?0x30u:0x0Cu)==5u);
}
static void abort_and_restart(void){
    JagBlitter b;
    new_blit(&b,8,0,1,(1u<<16)|5u);
    mem[A2]=1u;mem[A2+1]=0u;mem[A2+2]=2u;mem[A2+3]=3u;
    memset(&mem[A1],0xCC,5u);
    W(&b,0x68,0);W(&b,0x78,4u);
    W(&b,0x38,SRCEN|LFU_SRC|DCOMPEN);
    assert(b.stopped&&writes==1u);
    W(&b,0x78,6u);             /* ABORT precedence, clear STOPPED */
    assert((R(&b,0x38)&3u)==1u&&writes==1u&&mem[A1+2]==0xCCu);
    W(&b,0x0C,2u);W(&b,0x30,2u);W(&b,0x3C,(1u<<16)|2u);
    W(&b,0x38,SRCEN|LFU_SRC);
    assert((R(&b,0x38)&3u)==1u&&b.blits==2u);
    assert(mem[A1+2]==2u&&mem[A1+3]==3u);
}
static void bit_comparator(void){
    JagBlitter b;
    new_blit(&b,8,0,1,(1u<<16)|4u);
    mem[A2]=1u;mem[A2+1]=2u;mem[A2+2]=3u;mem[A2+3]=4u;
    memset(&mem[A1],0xCC,4u);
    W(&b,0x78,4u);W(&b,0x38,SRCEN|BCOMPEN|LFU_SRC);
    assert(b.stopped&&mem[A1]==1u&&mem[A1+1]==0xCCu);
    W(&b,0x78,5u);
    assert(b.stopped&&mem[A1+2]==3u&&mem[A1+3]==0xCCu);
    W(&b,0x78,1u);assert(!b.stopped);
}
static void z_comparator(void){
    JagBlitter b;
    new_blit(&b,16,0,1,(1u<<16)|3u);
    memset(&mem[A1],0xCC,6u);
    W(&b,0x68,0x00F000F0u);W(&b,0x6C,0x00F000F0u);
    W(&b,0x58,0x00090003u);W(&b,0x5C,0x00090009u);
    W(&b,0x50,0x00080008u);W(&b,0x54,0x00080008u);
    W(&b,0x78,4u);
    W(&b,0x38,PATDSEL|ZLESS);
    assert(b.stopped);
    assert(mem[A1]==0x00u&&mem[A1+1]==0xF0u);
    assert(mem[A1+2]==0xCCu&&mem[A1+3]==0xCCu);
    W(&b,0x78,1u);
    assert((R(&b,0x38)&3u)==1u&&mem[A1+4]==0x00u&&mem[A1+5]==0xF0u);
}
static void inhibited_without_stop(void){
    JagBlitter b;
    for(int mode=0;mode<4;mode++){
        /* No STOPEN, BKGWREN set, phrase mode, A1_CLIP alone. */
        new_blit(&b,8,0,mode==2?0:1,(1u<<16)|3u);
        mem[A2]=1u;mem[A2+1]=0u;mem[A2+2]=3u;
        memset(&mem[A1],0xAC,3u);
        W(&b,0x78,mode==0?0u:4u);
        if(mode==1)W(&b,0x38,SRCEN|LFU_SRC|DCOMPEN|BKGWREN);
        else if(mode==3){
            W(&b,0x08,(2u<<16)|1u);
            W(&b,0x38,SRCEN|LFU_SRC|(1u<<6));
        }else W(&b,0x38,SRCEN|LFU_SRC|DCOMPEN);
        assert((R(&b,0x38)&3u)==1u&&!b.stopped);
        if(mode==3)assert(mem[A1]==1u&&mem[A1+1]==0xACu);
    }
}
static void outer_loop_last_pixel_stop(void){
    JagBlitter b;
    new_blit(&b,8,0,1,(2u<<16)|3u);
    mem[A2]=1u;mem[A2+1]=2u;mem[A2+2]=0u;
    mem[A2+8]=4u;mem[A2+9]=5u;mem[A2+10]=6u;
    memset(&mem[A1],0xCC,16u);
    W(&b,0x10,(1u<<16)|((uint16_t)-3));
    W(&b,0x34,(1u<<16)|((uint16_t)-3));
    W(&b,0x68,0);W(&b,0x78,4u);
    W(&b,0x38,SRCEN|LFU_SRC|DCOMPEN|UPDA1|UPDA2);
    assert(b.stopped&&R(&b,0x0C)==3u);
    W(&b,0x78,1u);
    assert(!b.stopped&&b.blits==1u);
    assert(mem[A1]==1u&&mem[A1+1]==2u&&mem[A1+2]==0xCCu);
    assert(mem[A1+8]==4u&&mem[A1+9]==5u&&mem[A1+10]==6u);
}
int main(void){
    normal_copy(0);normal_copy(1);
    abort_and_restart();bit_comparator();z_comparator();
    inhibited_without_stop();outer_loop_last_pixel_stop();
    puts("RE38 R137 Jaguar TOM B_STOP collision/RESUME/ABORT passed");
    return 0;
}
