/* S22: run synthetic 68K+JRISC machine, prove the real GPU's bus requests
 * enter the observer and CPU/other accesses do not. No retail content. */
#include "jaguar/gpu_view_access.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JagMachine mach;
static JagGpuViewMap *map;
static uint8_t cart[0x4000];
static void inst(unsigned off,unsigned op,unsigned r1,unsigned r2)
{
    unsigned v=(op<<10)|(r1<<5)|r2;
    assert(off+2u<=4096u);
    mach.gpu.ram[off]=(uint8_t)(v>>8);mach.gpu.ram[off+1]=(uint8_t)v;
}
static void movei(unsigned off,unsigned dst,uint32_t imm)
{
    inst(off,38,0,dst);
    mach.gpu.ram[off+2]=(uint8_t)(imm>>8);
    mach.gpu.ram[off+3]=(uint8_t)imm;
    mach.gpu.ram[off+4]=(uint8_t)(imm>>24);
    mach.gpu.ram[off+5]=(uint8_t)(imm>>16);
}
int main(void)
{
    cart[0x404]=0x00;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0x00;
    cart[0x2000]=0x60;cart[0x2001]=0xFE; /* 68000 BRA.S * */
    assert(jagm_init(&mach,cart,sizeof cart)==0);
    map=calloc(1,sizeof *map);assert(map);
    mach.on_gpu_view_dram_access=jag_gpu_view_access_observe;
    mach.on_gpu_view_dram_access_ctx=map;
    movei(0,14,0x1020u); inst(6,41,14,1); /* LOAD (r14),r1 */
    movei(8,15,0x1320u); inst(14,47,15,1); /* STORE r1,(r15) */
    movei(16,20,0xF03020u);inst(22,52,20,0); /* JUMP r20 */
    inst(24,57,0,0); /* delay-slot NOP */
    mach.ram[0x1020]=0x01;mach.ram[0x1021]=0x23;
    mach.ram[0x1022]=0x45;mach.ram[0x1023]=0x67;
    mach.gpu.pc=0xF03000u;mach.gpu.go=1u;
    assert(jagm_run_until_gpu_pc(&mach,0xF03020u,0,1));
    assert(map->gpu_requests==2u);
    assert(map->blitter_requests==0u);
    assert(map->page[0x10].gpu_reads==1u);
    assert(map->page[0x13].gpu_writes==1u);
    assert(map->page[0x10].first_pc==0xF03006u);
    assert(map->page[0x13].first_pc==0xF0300Eu);
    assert(mach.ram[0x1320]==0x01 && mach.ram[0x1323]==0x67);
    /* No callback from direct host-side CPU memory operations. */
    jagm_write32(&mach,0x1540u,0xABCDEF01u);
    assert(jagm_read32(&mach,0x1540u)==0xABCDEF01u);
    assert(map->gpu_requests==2u);
    /* Synchronous TRM Blitter copy: capture actual physical source/dest
     * requests separately from GPU LOAD/STORE. No need for a retail ROM. */
    mach.ram[0x1500]=0x91;mach.ram[0x1501]=0x92;
    jagm_write32(&mach,0xF02200u,0x1600u); /* A1 base */
    jagm_write32(&mach,0xF02204u,3u<<3);  /* A1 8bpp */
    jagm_write32(&mach,0xF0220Cu,0u);
    jagm_write32(&mach,0xF02224u,0x1500u); /* A2 base */
    jagm_write32(&mach,0xF02228u,3u<<3);  /* A2 8bpp */
    jagm_write32(&mach,0xF02230u,0u);
    jagm_write32(&mach,0xF0223Cu,(1u<<16)|2u);
    jagm_write32(&mach,0xF02238u,1u|(0xCu<<21));
    assert(mach.ram[0x1600]==0x91 && mach.ram[0x1601]==0x92);
    assert(map->blitter_requests>0u);
    assert(map->page[0x15].blitter_reads>0u);
    assert(map->page[0x16].blitter_writes>0u);
    assert(map->gpu_requests==2u); /* blitter not mistaken for GPU */
    mach.on_gpu_view_dram_access=NULL;mach.on_gpu_view_dram_access_ctx=NULL;
    free(map);
    puts("RE24 S22 authentic JRISC bus-hook synthetic cartridge PASS");
    return 0;
}
