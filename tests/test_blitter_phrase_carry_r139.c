/* RE38 R139: a source phrase read for one destination phrase must remain
 * available to the next destination phrase when their alignment differs.
 * This tests logical read-before-write data, NOT bus-cycle timing. */
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
#define BASE 0x1200u

static uint8_t memory[0x5000];
static uint8_t rb(void *unused,uint32_t a)
{ (void)unused; assert(a<sizeof memory);return memory[a]; }
static void wb(void *unused,uint32_t a,uint8_t v)
{ (void)unused; assert(a<sizeof memory);memory[a]=v; }
static void W(JagBlitter *b,uint32_t off,uint32_t val)
{ assert(jag_blitter_write(b,off,val)); }
static uint32_t addr(unsigned bits,unsigned pitch,int z,uint32_t x)
{
    const uint32_t pp=64u/bits,byte=bits/8u;
    return BASE+(x/pp)*(8u*pitch)+(z?8u:0u)+(x%pp)*byte;
}
static uint32_t read_bits(unsigned bits,unsigned pitch,int z,uint32_t x)
{
    uint32_t a=addr(bits,pitch,z,x),v=0;
    for(unsigned t=0;t<bits/8u;t++)v=(v<<8)|memory[a+t];
    return v;
}
static void write_bits(unsigned bits,unsigned pitch,int z,uint32_t x,uint32_t v)
{
    uint32_t a=addr(bits,pitch,z,x);
    for(unsigned t=0;t<bits/8u;t++){
        unsigned shift=8u*((bits/8u)-t-1u);
        memory[a+t]=(uint8_t)(v>>shift);
    }
}
static void one_case(unsigned bits,int dsta2,int zcopy)
{
    JagBlitter b;JagBlitBus bus={NULL,rb,wb};
    const unsigned pp=64u/bits,pitch=zcopy?2u:1u;
    const uint32_t n=pp*3u+1u;
    const uint32_t pixel_bits_log=bits==8u?3u:bits==16u?4u:5u;
    const uint32_t flags=(pixel_bits_log<<3)|(4u<<11)|((pitch==2u)?1u:0u)|((zcopy?1u:0u)<<6);
    uint32_t color_before[64],z_before[64];
    assert(n+1u<64u);
    memset(memory,0,sizeof memory);
    for(uint32_t i=0;i<n+3u;i++){
        uint32_t v=bits==32u?0xA5340000u+i*0x173u:
                    bits==16u?0x3100u+i*17u:0x31u+i*9u;
        write_bits(bits,pitch,0,i,v);
        if(zcopy)write_bits(16u,pitch,1,i,(uint16_t)(0x4000u+i*7u));
    }
    for(uint32_t i=0;i<n;i++){
        color_before[i]=read_bits(bits,pitch,0,i);
        if(zcopy)z_before[i]=read_bits(16u,pitch,1,i);
    }
    jag_blitter_init(&b,bus);
    W(&b,0x00,BASE);W(&b,0x04,flags);W(&b,0x0C,dsta2?0u:1u);
    W(&b,0x24,BASE);W(&b,0x28,flags);W(&b,0x30,dsta2?1u:0u);
    W(&b,0x3C,(1u<<16)|n);
    W(&b,0x38,SRCEN|LFUS|(dsta2?DSTA2:0u)|(zcopy?(SRCENZ|DSTWRZ):0u));
    assert(!b.stopped);
    assert(!(b.unsupported&JB_UNSUP_SRCENX_ALIGN));
    for(uint32_t i=0;i<n;i++){
        uint32_t got=read_bits(bits,pitch,0,i+1u);
        if(got!=color_before[i])
            fprintf(stderr,"phrase carry color: bits=%u swap=%d z=%d i=%u got=%x want=%x\n",
                    bits,dsta2,zcopy,i,got,color_before[i]);
        assert(got==color_before[i]);
        if(zcopy){
            uint32_t gz=read_bits(16u,pitch,1,i+1u);
            if(gz!=z_before[i])
                fprintf(stderr,"phrase carry Z: bits=%u swap=%d i=%u got=%x want=%x\n",
                        bits,dsta2,i,gz,z_before[i]);
            assert(gz==z_before[i]);
        }
    }
    assert(b.blits==1u);
}
int main(void)
{
    for(int swap=0;swap<=1;swap++){
        one_case(8u,swap,0);
        one_case(16u,swap,0);
        one_case(16u,swap,1);
        one_case(32u,swap,0);
    }
    puts("RE38 R139 Jaguar TOM cross-phrase source / Z carry tests passed");
    return 0;
}
