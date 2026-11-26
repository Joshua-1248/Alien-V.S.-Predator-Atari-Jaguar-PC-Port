/* RE38 R135: independent CRY ADDDSEL saturation regression.
 * TOM Rev.8: ADDDSEL in 16-bit CRY mode (TOPBEN=TOPNEN=0) adds
 * three signed per-field offsets and saturates their destinations.
 * Synthetic memory only: no retail-ROM data, no host renderer.
 */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PS16 (4u<<3)
#define XADDPIX (1u<<16)
#define WIDTH8 (3u<<11)
#define SRCEN (1u<<0)
#define DSTEN (1u<<3)
#define DSTA2 (1u<<11)
#define ADDDSEL (1u<<17)
#define TOPBEN (1u<<14)
#define TOPNEN (1u<<15)
#define SRC 0x1000u
#define DST 0x2000u

static uint8_t mem[0x4000];
static uint8_t rd(void *ctx,uint32_t a){(void)ctx;assert(a<sizeof mem);return mem[a];}
static void wr(void *ctx,uint32_t a,uint8_t v){(void)ctx;assert(a<sizeof mem);mem[a]=v;}
static void wr16(uint32_t a,uint16_t v){mem[a]=(uint8_t)(v>>8);mem[a+1]=(uint8_t)v;}
static uint16_t rd16(uint32_t a){return (uint16_t)((mem[a]<<8)|mem[a+1]);}
static void W(JagBlitter *b,uint32_t off,uint32_t val){assert(jag_blitter_write(b,off,val));}

static void test_cry(int dsta2, int phrase)
{
    static const uint16_t src[8]={0x1180,0xFF01,0x1F7F,0x00FF,0x9180,0x0EFA,0x7F7F,0x0101};
    static const uint16_t dst[8]={0xFF10,0x007F,0x0F10,0x0002,0x7BFE,0xA220,0x1001,0xABFE};
    static const uint16_t want[8]={0xFF00,0x0080,0x1E8F,0x0001,0x0C7E,0xA01A,0x8080,0xACFF};
    JagBlitter b;
    JagBlitBus bus={NULL,rd,wr};
    uint32_t flags=PS16|WIDTH8|(phrase?0u:XADDPIX);
    memset(mem,0,sizeof mem);
    jag_blitter_init(&b,bus);
    for(unsigned i=0;i<8;i++){wr16(SRC+2u*i,src[i]);wr16(DST+2u*i,dst[i]);}
    W(&b,0x00,dsta2?SRC:DST);W(&b,0x04,flags);W(&b,0x0C,0u);
    W(&b,0x24,dsta2?DST:SRC);W(&b,0x28,flags);W(&b,0x30,0u);
    W(&b,0x3C,(1u<<16)|8u);
    W(&b,0x38,SRCEN|DSTEN|ADDDSEL|(dsta2?DSTA2:0u));
    for(unsigned i=0;i<8;i++){
        if(rd16(DST+2u*i)!=want[i]){
            fprintf(stderr,"CRY ADDDSEL DSTA2=%d phrase=%d pixel=%u actual=%04x expected=%04x\n",
                    dsta2,phrase,i,rd16(DST+2u*i),want[i]);
        }
        assert(rd16(DST+2u*i)==want[i]);
        assert(rd16(SRC+2u*i)==src[i]);
    }
    assert(b.unsupported==0u);
}

static void test_16bit_saturation(void)
{
    JagBlitter b;
    JagBlitBus bus={NULL,rd,wr};
    memset(mem,0,sizeof mem);
    jag_blitter_init(&b,bus);
    /* Full-word signed offsets with TOPBEN/TOPNEN: unchanged R134 behavior. */
    wr16(SRC,0x0002);wr16(SRC+2,0xFFFE);
    wr16(DST,0xFFFF);wr16(DST+2,0x0001);
    W(&b,0x00,DST);W(&b,0x04,PS16|WIDTH8|XADDPIX);W(&b,0x0C,0u);
    W(&b,0x24,SRC);W(&b,0x28,PS16|WIDTH8|XADDPIX);W(&b,0x30,0u);
    W(&b,0x3C,(1u<<16)|2u);
    W(&b,0x38,SRCEN|DSTEN|ADDDSEL|TOPBEN|TOPNEN);
    assert(rd16(DST)==0xFFFFu);
    assert(rd16(DST+2)==0x0000u);
    assert(b.unsupported==0u);
}

int main(void)
{
    for(int dsta2=0;dsta2<2;dsta2++)for(int phrase=0;phrase<2;phrase++)test_cry(dsta2,phrase);
    test_16bit_saturation();
    puts("RE38 R135 Jaguar Blitter CRY ADDDSEL signed-field saturation passed");
    return 0;
}
