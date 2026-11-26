/* RE44 R287: progressive Blitter and 68000 shared-DRAM grant regression.
 * Synthetic cartridge is self-contained; --retail test uses user-owned ROM. */
#include "jaguar/jag_machine.h"
#include "m68k.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD 0xF02238u
#define PS8 (3u<<3)
#define W16 (4u<<11)
#define XADD1 (1u<<16)
static JagMachine m;
static uint8_t cart[0x4000];

static void write32(uint32_t a,uint32_t v){jagm_write32(&m,a,v);}
static void put16(unsigned *addr,uint16_t value)
{
    assert(*addr+2u <= sizeof cart);
    cart[*addr]=(uint8_t)(value>>8);
    cart[*addr+1u]=(uint8_t)value;
    *addr+=2u;
}
static void put32(unsigned *addr,uint32_t value)
{
    put16(addr,(uint16_t)(value>>16));
    put16(addr,(uint16_t)value);
}
static void init_synthetic(int async)
{
    unsigned pos=0x2000u;
    memset(cart,0,sizeof cart);
    /* ROM entry $802000: write PATDSEL B_CMD, then the marker write, then
     * wait forever. Both writes are 68000 instructions, not host writes. */
    cart[0x404u]=0x00u;cart[0x405u]=0x80u;
    cart[0x406u]=0x20u;cart[0x407u]=0x00u;
    put16(&pos,0x23FCu);put32(&pos,0x00010000u);put32(&pos,CMD);
    put16(&pos,0x23FCu);put32(&pos,0x12345678u);put32(&pos,0x00005200u);
    put16(&pos,0x60FEu);
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_set_blitter_async(&m,async);
    write32(0xF02200u,0x00005000u);
    write32(0xF02204u,PS8|W16|XADD1);
    write32(0xF0220Cu,0u);
    write32(0xF0223Cu,(1u<<16)|4096u);
    write32(0xF02268u,0u);
    write32(0xF0226Cu,0u);
    memset(m.ram+0x5000u,0xA5u,4096u);
}
static void test_cpu_order(int async)
{
    init_synthetic(async);
    jagm_run_frames(&m,1u);
    assert(m.blit.blits==1u);
    assert(!m.blit.running);
    assert(m.ram[0x5200u]==0x12u && m.ram[0x5201u]==0x34u &&
           m.ram[0x5202u]==0x56u && m.ram[0x5203u]==0x78u);
    assert(m.ram[0x51FFu]==0u && m.ram[0x5204u]==0u);
    assert(m68k_get_reg(NULL,M68K_REG_PC)==0x802014u);
}
static void test_retail(const char *path)
{
    FILE *f=fopen(path,"rb");
    uint8_t *rom;
    long n;
    assert(f);
    assert(fseek(f,0,SEEK_END)==0);
    n=ftell(f);
    assert(n==4194304L && fseek(f,0,SEEK_SET)==0);
    rom=(uint8_t*)malloc((size_t)n);
    assert(rom);
    assert(fread(rom,1,(size_t)n,f)==(size_t)n);
    fclose(f);
    assert(jagm_init(&m,rom,(size_t)n)==0);
    jagm_set_blitter_async(&m,1);
    jagm_run_frames(&m,60u);
    /* This failed in R286: the Blitter overwrote the live stack frame at
     * $753C, RTS reached $000000 and bootstrap never configured video. */
    assert(m.blit.blits>=27u);
    assert(m.vmode==0x06C7u);
    assert(m.frame_w==320u && m.frame_h==288u);
    assert(m.gpu.go==0 && m.dsp.go!=0);
    assert(m68k_get_reg(NULL,M68K_REG_PC)==0x009202u);
    free(rom);
}
int main(int argc,char **argv)
{
    if(argc==2){test_retail(argv[1]);puts("R287 retail ROM progressive Blitter bootstrap PASS");return 0;}
    assert(argc==1);
    test_cpu_order(0);
    test_cpu_order(1);
    puts("R287 synthetic 68000/Blitter shared-DRAM grant PASS");
    return 0;
}
