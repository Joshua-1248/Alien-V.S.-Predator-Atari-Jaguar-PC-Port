/* RE24-S16 standalone, synthetic regressions for COMPLETE architectural
 * comparison; no original cartridge or GPU-generated ROM code required. */
#include "jaguar/jrisc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct AvpGpuRecompCtx {
    uint32_t r[2][32]; unsigned bank; int z,c,n,divoff;
    uint32_t remain; int64_t acc;
} AvpGpuRecompCtx;
#include "../tools/jagref/gpu_arch_parity.h"

int main(void)
{
    JRisc a; AvpGpuRecompCtx g;
    memset(&a,0,sizeof a);memset(&g,0,sizeof g);
    a.pc=0xF03118u;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==0u);
    a.reg[1][31]=100u;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==AVP_GPU_DIFF_REGS);
    g.r[1][31]=100u;
    a.flags=JRISC_FLAG_REGPAGE;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==AVP_GPU_DIFF_BANK);
    g.bank=1;
    a.flags|=JRISC_FLAG_C;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==AVP_GPU_DIFF_FLAGS);
    g.c=1;
    a.acc=(int64_t)0x10000000u;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==AVP_GPU_DIFF_ACC);
    g.acc=a.acc;
    a.remain=0xC000u;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==AVP_GPU_DIFF_REMAIN);
    g.remain=a.remain;
    a.divctrl=1;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==AVP_GPU_DIFF_DIVCTRL);
    g.divoff=1;
    assert(avp_gpu_arch_diff(&a,&g,a.pc+2u)==AVP_GPU_DIFF_PC);
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==0u);
    a.flags|=JRISC_FLAG_IMASK;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==AVP_GPU_DIFF_BANK);
    g.bank=0;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==0u);
    a.reg[0][2]=99;a.acc=-1;a.remain=2;a.flags^=JRISC_FLAG_Z;
    assert(avp_gpu_arch_diff(&a,&g,a.pc)==(AVP_GPU_DIFF_REGS|AVP_GPU_DIFF_ACC|
        AVP_GPU_DIFF_REMAIN|AVP_GPU_DIFF_FLAGS));
    puts("Sweep 16 GPU architectural parity (7 categories) passed");
    return 0;
}
