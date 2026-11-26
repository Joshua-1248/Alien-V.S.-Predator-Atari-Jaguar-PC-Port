/* RE38 R138: phrase source data/Z must be read before destination write.
 * Register-level functional fixture, not a bus-cycle/timing claim. */
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
#define DSTWRZ (1u<<5)
#define DSTA2 (1u<<11)
#define LFUS (0xCu<<21)
#define BASE 0x1000u
static uint8_t memory[0x3000];
static uint8_t rb(void *unused,uint32_t a){(void)unused;assert(a<sizeof memory);return memory[a];}
static void wb(void *unused,uint32_t a,uint8_t v){(void)unused;assert(a<sizeof memory);memory[a]=v;}
static void W(JagBlitter *b,uint32_t a,uint32_t v){assert(jag_blitter_write(b,a,v));}
static void put16(uint32_t a,uint16_t v){memory[a]=(uint8_t)(v>>8);memory[a+1]=(uint8_t)v;}
static uint16_t get16(uint32_t a){return (uint16_t)(((uint16_t)memory[a]<<8)|memory[a+1]);}

static void overlapping_phrase(unsigned bits,int swap,int zcopy)
{
    const uint32_t bytes=bits/8u;
    const uint32_t n=bits==8u?5u:3u;
    const uint32_t flags=((bits==8u?3u:4u)<<3)|(4u<<11)|((zcopy?1u:0u)<<6);
    const uint32_t zbase=BASE+8u;
    JagBlitter b;JagBlitBus bus={NULL,rb,wb};
    uint8_t source[6u*2u];
    uint16_t zs[4];
    memset(memory,0,sizeof memory);
    for(uint32_t i=0;i<8u;i++){
        for(uint32_t t=0;t<bytes;t++)memory[BASE+i*bytes+t]=(uint8_t)(0x31u+i*13u+t);
        if(zcopy)put16(zbase+i*2u,(uint16_t)(0x1100u+i));
    }
    memcpy(source,memory+BASE,n*bytes);
    for(uint32_t i=0;i<4u;i++)zs[i]=zcopy?get16(zbase+i*2u):0u;
    jag_blitter_init(&b,bus);
    W(&b,0x00,BASE);W(&b,0x04,flags);W(&b,0x0C,swap?0u:1u);
    W(&b,0x24,BASE);W(&b,0x28,flags);W(&b,0x30,swap?1u:0u);
    W(&b,0x3C,(1u<<16)|n);
    W(&b,0x38,SRCEN|LFUS|(swap?DSTA2:0u)|(zcopy?(SRCENZ|DSTWRZ):0u));
    assert(!b.stopped);
    for(uint32_t i=0;i<n;i++){
        for(uint32_t t=0;t<bytes;t++){
            if(memory[BASE+(i+1u)*bytes+t]!=source[i*bytes+t])
                fprintf(stderr,"overlap failed bits=%u swap=%d z=%d i=%u byte=%u got=%02x want=%02x\n",bits,swap,zcopy,i,t,memory[BASE+(i+1u)*bytes+t],source[i*bytes+t]);
            assert(memory[BASE+(i+1u)*bytes+t]==source[i*bytes+t]);
        }
        if(zcopy)assert(get16(zbase+(i+1u)*2u)==zs[i]);
    }
}
int main(void)
{
    for(int swap=0;swap<=1;swap++){
        overlapping_phrase(8u,swap,0);
        overlapping_phrase(16u,swap,0);
        overlapping_phrase(16u,swap,1);
    }
    puts("RE38 R138 Jaguar TOM phrase source/Z prefetch overlap tests passed");
    return 0;
}
