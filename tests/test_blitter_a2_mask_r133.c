/* R133: A2_MASK & the A2_FLAGS Mask bit, Atari Jaguar TRM rev.8 pp.72-73.
 * Fixture data are synthetic, and no proprietary Jaguar bytes are included.
 * Masking changes the effective A2 ADDRESS; the register pointer continues
 * its original increment and is not replaced with the masked coordinate. */
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
#define PS8 (3u<<3)
#define PS16 (4u<<3)
#define WIDTH(e,m) ((((uint32_t)(e)<<2)|(m))<<9)
#define XADDPIX (1u<<16)
#define MASK (1u<<15)
#define SRCEN 1u
#define DSTA2 (1u<<11)
#define UPDA1 (1u<<9)
#define UPDA2 (1u<<10)
#define LFUS (0xCu<<21)

static uint8_t ram[0x8000];
static uint8_t rd(void *unused,uint32_t addr){(void)unused;assert(addr<sizeof ram);return ram[addr];}
static void wr(void *unused,uint32_t addr,uint8_t v){(void)unused;assert(addr<sizeof ram);ram[addr]=v;}
static JagBlitter b;
static void reset(void){JagBlitBus bus={NULL,rd,wr};memset(ram,0,sizeof ram);jag_blitter_init(&b,bus);}
static void W(uint32_t reg,uint32_t value){assert(jag_blitter_write(&b,reg,value));}
static uint16_t read16(uint32_t addr){return (uint16_t)((ram[addr]<<8)|ram[addr+1]);}
static void write16(uint32_t addr,uint16_t v){ram[addr]=(uint8_t)(v>>8);ram[addr+1]=(uint8_t)v;}
static void setup8(uint32_t flags)
{
    W(0x00,DST);W(0x04,PS8|WIDTH(4,0)|XADDPIX);W(0x0C,0);
    W(0x24,SRC);W(0x28,PS8|WIDTH(3,0)|XADDPIX|flags);
    W(0x2C,(3u<<16)|7u); /* 8x4 repeat window */
    W(0x30,(2u<<16)|6u);
}
static void populate(void)
{
    for(unsigned y=0;y<6;y++)for(unsigned x=0;x<8;x++)
        ram[SRC+y*8u+x]=(uint8_t)(0x10u+y*16u+x);
}
static void test_masked_x(void)
{
    reset();populate();setup8(MASK);
    W(0x3C,(1u<<16)|12u);W(0x38,SRCEN|LFUS);
    for(unsigned i=0;i<12;i++)
        assert(ram[DST+i]==(uint8_t)(0x30u+((6u+i)&7u)));
    /* Source pixel register continues to count beyond the mask. */
    assert(b.a2_pixel==((2u<<16)|18u));
}
static void test_mask_disabled(void)
{
    reset();populate();setup8(0u);
    W(0x3C,(1u<<16)|12u);W(0x38,SRCEN|LFUS);
    for(unsigned i=0;i<12;i++)
        assert(ram[DST+i]==ram[SRC+2u*8u+6u+i]);
}
static void test_masked_y_outer_loop(void)
{
    reset();populate();
    W(0x00,DST);W(0x04,PS8|WIDTH(3,0)|XADDPIX);W(0x0C,0);
    W(0x10,(1u<<16)|(uint16_t)-8);
    W(0x24,SRC);W(0x28,PS8|WIDTH(3,0)|XADDPIX|MASK);
    W(0x2C,(3u<<16)|7u);W(0x30,(3u<<16));
    W(0x34,(1u<<16)|(uint16_t)-8);
    W(0x3C,(3u<<16)|8u);W(0x38,SRCEN|LFUS|UPDA1|UPDA2);
    for(unsigned row=0;row<3;row++)for(unsigned x=0;x<8;x++)
        assert(ram[DST+row*8u+x]==(uint8_t)(0x10u+((3u+row)&3u)*16u+x));
    assert((b.a2_pixel>>16)==6u); /* Unmasked Y readback. */
}
static void test_a2_destination(void)
{
    reset();for(unsigned i=0;i<24;i++)ram[SRC+i]=(uint8_t)(0x40u+i);
    W(0x00,SRC);W(0x04,PS8|WIDTH(4,0)|XADDPIX);W(0x0C,0);
    W(0x24,DST);W(0x28,PS8|WIDTH(3,0)|XADDPIX|MASK);
    W(0x2C,(3u<<16)|7u);W(0x30,6u);
    W(0x3C,(1u<<16)|12u);W(0x38,SRCEN|LFUS|DSTA2);
    for(unsigned i=0;i<8;i++){
        /* Independently calculate last write for this wrapped destination. */
        unsigned j=0;for(unsigned k=0;k<12;k++)if(((6u+k)&7u)==i)j=k;
        assert(ram[DST+i]==(uint8_t)(0x40u+j));
    }
    assert((b.a2_pixel&0xFFFFu)==18u);
}
static void test_negative_and_16bit(void)
{
    reset();
    for(unsigned i=0;i<8;i++)write16(SRC+2*i,(uint16_t)(0x1200u+i));
    W(0x00,DST);W(0x04,PS16|WIDTH(4,0)|XADDPIX);W(0x0C,0);
    W(0x24,SRC);W(0x28,PS16|WIDTH(3,0)|XADDPIX|MASK);
    W(0x2C,(0u<<16)|7u);W(0x30,0xFFFFu);
    W(0x3C,(1u<<16)|3u);W(0x38,SRCEN|LFUS);
    assert(read16(DST)==0x1207u);
    assert(read16(DST+2)==0x1200u);
    assert(read16(DST+4)==0x1201u);
}
int main(void)
{
    test_masked_x();test_mask_disabled();test_masked_y_outer_loop();
    test_a2_destination();test_negative_and_16bit();
    puts("R133 A2_MASK source/destination, X/Y wrap, disabled, 16-bit passed");
    return 0;
}
