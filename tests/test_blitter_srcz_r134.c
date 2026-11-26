/* RE38 R134: source Z selection for TOM DSTWRZ with ZMODE disabled.
 * Atari Jaguar Technical Reference Rev.8, Blitter data path and B_CMD.
 * Synthetic memory only; no original ROM content.
 */
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
#define PS16 (4u<<3)
#define WIDTH8 (3u<<11)
#define ZOFF1 (1u<<6)
#define XADDPIX (1u<<16)
#define PITCH2 1u
#define SRCEN (1u<<0)
#define SRCENZ (1u<<1)
#define DSTWRZ (1u<<5)
#define GOURZ (1u<<13)
#define DSTA2 (1u<<11)
#define LFUS (0xCu<<21)

static uint8_t ram[0x4000];
static uint8_t rd(void *ctx,uint32_t a){(void)ctx;assert(a<sizeof ram);return ram[a];}
static void wr(void *ctx,uint32_t a,uint8_t v){(void)ctx;assert(a<sizeof ram);ram[a]=v;}
static JagBlitter b;
static void W(uint32_t off,uint32_t v){assert(jag_blitter_write(&b,off,v));}
static void wr16(uint32_t off,uint16_t v){ram[off]=(uint8_t)(v>>8);ram[off+1]=(uint8_t)v;}
static uint16_t rd16(uint32_t off){return (uint16_t)((ram[off]<<8)|ram[off+1]);}

static void setup(int dsta2)
{
    JagBlitBus bus={NULL,rd,wr};
    memset(ram,0,sizeof ram);jag_blitter_init(&b,bus);
    for(unsigned i=0;i<4;i++){
        wr16(SRC+2*i,(uint16_t)(0xA000u+i));
        wr16(SRC+8+2*i,(uint16_t)(0x7200u+i));
        wr16(DST+8+2*i,(uint16_t)(0xBC00u+i));
    }
    W(0x00,dsta2?SRC:DST); W(0x04,PS16|WIDTH8|ZOFF1|PITCH2|XADDPIX);
    W(0x0C,0u); W(0x24,dsta2?DST:SRC); W(0x28,PS16|WIDTH8|ZOFF1|PITCH2|XADDPIX);
    W(0x30,0u);
    W(0x58,0x11112222u); W(0x5C,0x33334444u); /* computed/fallback Z lanes */
    W(0x3C,(1u<<16)|4u);
}

static void check_z(uint16_t first, uint16_t second, uint16_t third, uint16_t fourth)
{
    assert(rd16(DST+8)==first); assert(rd16(DST+10)==second);
    assert(rd16(DST+12)==third); assert(rd16(DST+14)==fourth);
}

int main(void)
{
    /* Ordinary Z copy: with and without DSTA2, no comparator needed. */
    for(int reverse=0;reverse<=1;reverse++){
        setup(reverse);
        W(0x38,SRCEN|SRCENZ|DSTWRZ|LFUS|(reverse?DSTA2:0u));
        check_z(0x7200,0x7201,0x7202,0x7203);
        for(unsigned i=0;i<4;i++) assert(rd16(DST+2*i)==(uint16_t)(0xA000u+i));
    }
    /* SRCENZ alone cannot trigger a source-Z fetch without SRCEN. */
    setup(0);
    W(0x38,SRCENZ|DSTWRZ|LFUS);
    check_z(0x1111,0x2222,0x3333,0x4444);
    /* Without source Z reads or GOURZ, the Z register lane is used. */
    setup(0);
    W(0x38,SRCEN|DSTWRZ|LFUS);
    check_z(0x1111,0x2222,0x3333,0x4444);
    /* GOURZ selects computed Z, not fetched source Z. */
    setup(0);
    W(0x38,SRCEN|SRCENZ|GOURZ|DSTWRZ|LFUS);
    check_z(0x1111,0x2222,0x3333,0x4444);
    puts("RE38 R134 TOM DSTWRZ source-Z selection passed");
    return 0;
}
