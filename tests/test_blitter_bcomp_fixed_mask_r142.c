/* RE38 R142: fixed-source 8-bit character mask, BCOMPEN bit counter.
 * This tests only the documented XADD0 source and XADDPIX destination
 * configuration. No GPU-cycle timing or arbitrary source alignment assumed. */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRC 0x1000u
#define DST 0x2000u
#define SRCEN (1u << 0)
#define DSTEN (1u << 3)
#define DSTA2 (1u << 11)
#define PATDSEL (1u << 16)
#define BCOMPEN (1u << 26)
#define BKGWREN (1u << 28)
#define PS8 (3u << 3)
#define PS16 (4u << 3)
#define WIDTH16 (4u << 11)
#define XADDPIX (1u << 16)
#define XADD0 (2u << 16)

static uint8_t memory[0x4000];
static uint8_t rd(void *ctx,uint32_t a)
{ (void)ctx;assert(a<sizeof memory);return memory[a]; }
static void wr(void *ctx,uint32_t a,uint8_t v)
{ (void)ctx;assert(a<sizeof memory);memory[a]=v; }
static void W(JagBlitter *b,uint32_t o,uint32_t v)
{ assert(jag_blitter_write(b,o,v)); }
static uint32_t rd_pixel(uint32_t offset,uint32_t bits)
{ return bits==8u?memory[offset]:((uint32_t)memory[offset]<<8)|memory[offset+1u]; }
static void wr_pixel(uint32_t offset,uint32_t bits,uint32_t value)
{ memory[offset]=(uint8_t)(value>>(bits-8u));if(bits==16u)memory[offset+1u]=(uint8_t)value; }
static void verify(int reversed,int bits,int paper,int rows,int enabled)
{
    JagBlitter b;
    JagBlitBus bus={NULL,rd,wr};
    uint32_t ps=(bits==8)?PS8:PS16;
    uint32_t draw=bits==8?0x5Au:0xB39Du;
    uint32_t old=bits==8?0x77u:0x7474u;
    uint32_t background=bits==8?0x11u:0x1111u;
    uint32_t dstflags=ps|WIDTH16|XADDPIX;
    uint32_t srcflags=PS8|WIDTH16|XADD0;
    uint32_t cmd=(enabled?SRCEN:0u)|PATDSEL|BCOMPEN|
                 (reversed?DSTA2:0u)|(paper?BKGWREN:0u);
    memset(memory,0,sizeof memory);
    memory[SRC]=0xA5u; /* MSB-first 10100101, not eight copies of LSB=1. */
    for(int i=0;i<rows*8;i++)wr_pixel(DST+(uint32_t)i*(uint32_t)(bits/8),bits,old);
    jag_blitter_init(&b,bus);
    W(&b,0x00,reversed?SRC:DST);
    W(&b,0x04,reversed?srcflags:dstflags);
    W(&b,0x0C,0u);
    W(&b,0x24,reversed?DST:SRC);
    W(&b,0x28,reversed?dstflags:srcflags);
    W(&b,0x30,0u);
    W(&b,0x40,0xA5000000u);W(&b,0x44,0u);
    W(&b,0x48,0x11111111u);W(&b,0x4C,0x11111111u);
    W(&b,0x68,bits==8?0x5A5A5A5Au:0xB39DB39Du);
    W(&b,0x6C,bits==8?0x5A5A5A5Au:0xB39DB39Du);
    /* Test source-register mask while source address is fixed, and verify
     * that the bit index resets for the next inner loop. */
    W(&b,0x3C,((uint32_t)rows<<16)|8u);
    W(&b,0x38,cmd);
    for(int i=0;i<rows*8;i++){
        uint32_t k=(uint32_t)i&7u;
        uint32_t expected=((0xA5u>>(7u-k))&1u)?draw:(paper?background:old);
        assert(rd_pixel(DST+(uint32_t)i*(uint32_t)(bits/8),bits)==expected);
    }
    assert(b.blits==1u&&!b.stopped);
}
int main(void)
{
    unsigned cases=0u;
    for(int reversed=0;reversed<2;reversed++)
        for(int bits=8;bits<=16;bits*=2)
            for(int paper=0;paper<2;paper++)
                for(int rows=1;rows<=2;rows++)
                    for(int enabled=0;enabled<=1;enabled++){
                        verify(reversed,bits,paper,rows,enabled);cases++;
                    }
    printf("RE38 R142 fixed-source eight-bit character BCOMPEN passed (%u cases)\n",cases);
    return 0;
}
