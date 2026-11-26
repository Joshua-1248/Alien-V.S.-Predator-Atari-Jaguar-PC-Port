/* R140: SRCENX performs an extra source phrase fetch at the start of each
 * inner loop, even when the destination's first pixel does not need its data.
 * The byte callbacks observe data-read ordering, NOT individual Jaguar bus
 * transactions. Test 16-bit color/Z and DSTA2 for both address generators. */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRC 0x1000u
#define DST 0x3000u
#define SRCEN (1u<<0)
#define SRCENZ (1u<<1)
#define SRCENX (1u<<2)
#define DSTA2 (1u<<11)
#define LFU_S (0xCu<<21)
static uint8_t m[0x5000];
static unsigned source_color[2],source_z[2],destination_writes,first_write_color[2],first_write_z[2];
static uint8_t rb(void *c,uint32_t a)
{
    (void)c;assert(a<sizeof m);
    for(unsigned i=0;i<2u;i++){
        if(a>=SRC+i*16u&&a<SRC+i*16u+8u)source_color[i]++;
        if(a>=SRC+i*16u+8u&&a<SRC+i*16u+16u)source_z[i]++;
    }
    return m[a];
}
static void wb(void *c,uint32_t a,uint8_t v)
{
    (void)c;assert(a<sizeof m);
    if(a>=DST&&a<DST+0x100u){
        if(!destination_writes){
            for(unsigned i=0;i<2u;i++){
                first_write_color[i]=source_color[i];
                first_write_z[i]=source_z[i];
            }
        }
        destination_writes++;
    }
    m[a]=v;
}
static void W(JagBlitter *b,uint32_t off,uint32_t v){assert(jag_blitter_write(b,off,v));}
static void check(int dsta2,int extra,int z)
{
    JagBlitter b;JagBlitBus bus={NULL,rb,wb};
    /* 16 bpp, pitch 2, Z offset 1, 16-pixel window; one pixel written. */
    const uint32_t flags=(4u<<3)|(4u<<11)|1u|(1u<<6);
    memset(m,0,sizeof m);memset(source_color,0,sizeof source_color);
    memset(source_z,0,sizeof source_z);memset(first_write_color,0,sizeof first_write_color);
    memset(first_write_z,0,sizeof first_write_z);destination_writes=0;
    for(unsigned i=0;i<2u;i++)for(unsigned t=0;t<8u;t++){
        m[SRC+i*16u+t]=(uint8_t)(0x42u+i*16u+t);
        m[SRC+i*16u+8u+t]=(uint8_t)(0x60u+i*16u+t);
    }
    jag_blitter_init(&b,bus);
    W(&b,0x00,dsta2?SRC:DST);W(&b,0x04,flags);W(&b,0x0C,0u);
    W(&b,0x24,dsta2?DST:SRC);W(&b,0x28,flags);W(&b,0x30,0u);
    W(&b,0x3C,(1u<<16)|1u);
    W(&b,0x38,SRCEN|LFU_S|(dsta2?DSTA2:0u)|(extra?SRCENX:0u)|(z?SRCENZ:0u));
    assert(destination_writes>0u);
    assert(first_write_color[0]==8u);
    assert(first_write_color[1]==(extra?8u:0u));
    /* R141: SRCENZ enables the ordinary Z read even when no Z operation
     * consumes it; SRCENX adds a *second* prefetched source-Z phrase. */
    assert(first_write_z[0]==(z?8u:0u));
    assert(first_write_z[1]==(extra&&z?8u:0u));
    assert(m[DST]==0x42u&&m[DST+1u]==0x43u);
    assert(b.blits==1u&&b.stopped==0);
}
int main(void)
{
    for(int dsta2=0;dsta2<=1;dsta2++){
        check(dsta2,0,0);check(dsta2,1,0);
        check(dsta2,0,1);check(dsta2,1,1);
    }
    puts("RE38 R140 SRCENX extra color/Z source-prefetch ordering passed");
    return 0;
}
