/* RE24-S10: TOM Blitter model against explicit statements of the Atari Jaguar
 * Technical Reference Manual Rev.8 (page numbers inline). */
#include "jaguar/blitter_trm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t mem[0x40000];
static uint8_t rd(void *c,uint32_t a){(void)c;return mem[a&0x3FFFFu];}
static void wr(void *c,uint32_t a,uint8_t v){(void)c;mem[a&0x3FFFFu]=v;}
static JagBlitter B;
static void reset(void){JagBlitBus b={0,rd,wr};jag_blitter_init(&B,b);memset(mem,0,sizeof mem);}
static void W(uint32_t off,uint32_t v){assert(jag_blitter_write(&B,off,v));}
static uint16_t rd16(uint32_t a){return (uint16_t)((mem[a]<<8)|mem[a+1]);}
static void wr16(uint32_t a,uint16_t v){mem[a]=(uint8_t)(v>>8);mem[a+1]=(uint8_t)v;}
#define WIDTH(e,m) ((((uint32_t)(e)<<2)|(m))<<9)
#define PS(n) ((uint32_t)(n)<<3)

int main(void)
{
    /* p.78 window width encodings */
    assert(jag_blitter_width(WIDTH(4,1))==20u);
    assert(jag_blitter_width(WIDTH(6,1))==80u);
    assert(jag_blitter_width(WIDTH(7,0))==128u);
    assert(jag_blitter_width(WIDTH(9,1))==640u);
    assert(jag_blitter_width(WIDTH(11,3))==3584u);

    /* p.78 block move: phrase mode, SRCEN, LFU=1100 (source), 8-bit pixels */
    reset();
    for(int i=0;i<64;i++)mem[0x1000+i]=(uint8_t)(i*7+1);
    W(0x00,0x2000);W(0x04,PS(3));W(0x0C,0);       /* A1 dst */
    W(0x24,0x1000);W(0x28,PS(3));W(0x30,0);       /* A2 src */
    W(0x3C,(1u<<16)|40u);
    W(0x38,1u|(0xCu<<21));
    for(int i=0;i<40;i++)assert(mem[0x2000+i]==(uint8_t)(i*7+1));
    assert(mem[0x2000+40]==0);                    /* last partial phrase masked */
    { uint32_t p;assert(jag_blitter_read(&B,0x04,&p));assert(p==40u); } /* bug 10: A1 pointer read at F02204 */

    /* pattern fill, 16-bit pixels, phrase mode starting mid-phrase: pixels
     * outside x=1..5 keep their contents (partial phrases are masked). */
    reset();
    for(int i=0;i<8;i++)wr16(0x3000+2u*i,0xEEEEu);
    W(0x00,0x3000);W(0x04,PS(4)|WIDTH(4,1));W(0x0C,1);
    W(0x68,0x11112222u);W(0x6C,0x33334444u);       /* PATD: pixel0..3 high->low */
    W(0x3C,(1u<<16)|5u);
    W(0x38,1u<<16);                                /* PATDSEL */
    assert(rd16(0x3000)==0xEEEEu);
    assert(rd16(0x3002)==0x2222u&&rd16(0x3004)==0x3333u&&rd16(0x3006)==0x4444u);
    assert(rd16(0x3008)==0x1111u&&rd16(0x300A)==0x2222u&&rd16(0x300C)==0xEEEEu);

    /* p.74 LFU minterms: 0011 = NOT source */
    reset();
    mem[0x1000]=0x0F;W(0x00,0x2000);W(0x04,PS(3)|(1u<<16));W(0x0C,0);W(0x24,0x1000);W(0x28,PS(3)|(1u<<16));W(0x30,0);
    W(0x3C,(1u<<16)|1u);W(0x38,1u|(0x3u<<21));assert(mem[0x2000]==0xF0u);

    /* p.75 DCOMPEN, pixel mode, BKGWREN clear: source == pattern not written */
    reset();
    wr16(0x1000,0x1234);wr16(0x1002,0x0000);wr16(0x1004,0x5678);
    for(int i=0;i<3;i++)wr16(0x2000+2u*i,0xAAAAu);
    W(0x00,0x2000);W(0x04,PS(4)|(1u<<16));W(0x0C,0);W(0x24,0x1000);W(0x28,PS(4)|(1u<<16));W(0x30,0);
    W(0x68,0x00000000u);W(0x6C,0x00000000u);
    W(0x3C,(1u<<16)|3u);W(0x38,1u|(0xCu<<21)|(1u<<27));
    assert(rd16(0x2000)==0x1234u&&rd16(0x2002)==0xAAAAu&&rd16(0x2004)==0x5678u);

    /* p.71/p.74 CLIP_A1: writes with the A1 pointer outside its window are inhibited */
    reset();
    W(0x00,0x2000);W(0x04,PS(3)|WIDTH(4,1)|(1u<<16));W(0x08,(5u<<16)|3u);W(0x0C,0);
    W(0x68,0x5A5A5A5Au);W(0x6C,0x5A5A5A5Au);
    W(0x3C,(1u<<16)|6u);W(0x38,(1u<<16)|(1u<<6));
    assert(mem[0x2000]==0x5A&&mem[0x2002]==0x5A&&mem[0x2003]==0&&mem[0x2005]==0);

    /* p.73 outer loop: UPDA1/UPDA2 step to the next line of a rectangle */
    reset();
    for(int y=0;y<3;y++)for(int x=0;x<4;x++)mem[0x1000+y*8+x]=(uint8_t)(0x10*y+x+1);
    W(0x00,0x2000);W(0x04,PS(3)|WIDTH(4,1)|(1u<<16));W(0x0C,0);W(0x10,(1u<<16)|(uint16_t)-4);
    W(0x24,0x1000);W(0x28,PS(3)|WIDTH(3,0)|(1u<<16));W(0x30,0);W(0x34,(1u<<16)|(uint16_t)-4);
    W(0x3C,(3u<<16)|4u);W(0x38,1u|(0xCu<<21)|(1u<<9)|(1u<<10));
    for(int y=0;y<3;y++)for(int x=0;x<4;x++)assert(mem[0x2000+y*20+x]==(uint8_t)(0x10*y+x+1));

    /* p.71 A1 X add control 11: add the 16.16 increment (DDA) */
    reset();
    for(int i=0;i<16;i++)mem[0x1000+i]=(uint8_t)i;
    W(0x00,0x1000);W(0x04,PS(3)|WIDTH(4,1)|(3u<<16));W(0x0C,0);W(0x18,0);W(0x1C,1u);W(0x20,0x8000u); /* +1.5 */
    W(0x24,0x2000);W(0x28,PS(3)|(1u<<16));W(0x30,0);
    W(0x3C,(1u<<16)|4u);W(0x38,1u|(0xCu<<21)|(1u<<11)); /* DSTA2: A1 is the source */
    assert(mem[0x2000]==0&&mem[0x2001]==1&&mem[0x2002]==3&&mem[0x2003]==4);

    /* p.75 SRCSHADE (with GOURZ, bug 9): IINC integer part added to the
     * source intensity, saturating; colour byte unchanged */
    reset();
    wr16(0x1000,0x4A10);wr16(0x1002,0x4AF0);
    W(0x00,0x2000);W(0x04,PS(4)|(1u<<16));W(0x0C,0);W(0x24,0x1000);W(0x28,PS(4)|(1u<<16));W(0x30,0);
    W(0x70,0x00E95000u);                           /* -23 */
    W(0x3C,(1u<<16)|2u);W(0x38,1u|(0xCu<<21)|(1u<<13)|(1u<<30));
    assert(rd16(0x2000)==0x4A00u&&rd16(0x2002)==0x4AD9u);
    assert(!(B.unsupported&JB_UNSUP_SRCSHADE));

    puts("Blitter TRM tests passed");
    return 0;
}
