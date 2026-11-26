/* R133: CLIP_A1 must clip A1-generated SOURCE coordinates with DSTA2, even
 * inside one destination phrase. Atari Jaguar TRM rev.8 pp.71,74. */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char ram[0x4000];
static unsigned char read8(void *ignored,uint32_t a){(void)ignored;assert(a<sizeof ram);return ram[a];}
static void write8(void *ignored,uint32_t a,uint8_t v){(void)ignored;assert(a<sizeof ram);ram[a]=v;}
static void set(JagBlitter *b,uint32_t r,uint32_t v){assert(jag_blitter_write(b,r,v));}
int main(void)
{
    JagBlitter blit;JagBlitBus bus={NULL,read8,write8};
    const uint32_t ps8=3u<<3u,width8=(3u<<11u),source_read=1u,source_lfu=0xCu<<21;
    for(unsigned mode=0;mode<2;mode++){
        memset(ram,0,sizeof ram);
        for(unsigned i=0;i<16;i++)ram[0x1000+i]=(uint8_t)(0x31+i);
        for(unsigned i=0;i<8;i++)ram[0x2000+i]=0xABu;
        jag_blitter_init(&blit,bus);
        set(&blit,0x00,0x1000);set(&blit,0x04,ps8|width8|(1u<<16));
        set(&blit,0x0C,1u);set(&blit,0x08,(1u<<16)|3u); /* 0<=x<3 */
        set(&blit,0x24,0x2000);set(&blit,0x28,ps8|width8);
        set(&blit,0x30,0u);set(&blit,0x3C,(1u<<16)|8u);
        set(&blit,0x38,source_read|source_lfu|(1u<<11)|((mode==0)?(1u<<6):0u));
        for(unsigned i=0;i<8;i++){
            uint8_t want=(mode==0 && i>=2)?0xABu:(uint8_t)(0x32+i);
            assert(ram[0x2000+i]==want);
        }
    }
    puts("R133 A1 clipping across DSTA2 phrase source passed");
    return 0;
}
