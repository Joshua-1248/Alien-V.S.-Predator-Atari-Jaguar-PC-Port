/* RE38 R136: missing SRCENX when a phrase read crosses the source boundary.
 * Register-level specification test: still uses idealized byte-addressable
 * bus, so we demand a diagnostic rather than fabricating unknown silicon data.
 */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRCEN  (1u<<0)
#define SRCENX (1u<<2)
#define DSTA2  (1u<<11)
#define LFU_S (0xCu<<21)
#define SRC 0x1000u
#define DST 0x2000u
static uint8_t mem[0x4000];
static uint8_t rd(void *ctx,uint32_t a){(void)ctx;assert(a<sizeof mem);return mem[a];}
static void wr(void *ctx,uint32_t a,uint8_t v){(void)ctx;assert(a<sizeof mem);mem[a]=v;}
static void W(JagBlitter *b,uint32_t off,uint32_t val){assert(jag_blitter_write(b,off,val));}

static void check(unsigned bits,int dsta2,uint32_t sx,uint32_t dx,uint32_t inner,
                  int extra_read,int needs_extra)
{
    JagBlitter b;JagBlitBus bus={NULL,rd,wr};
    const uint32_t pbits=bits==8u?8u:16u;
    const uint32_t pp=64u/pbits;
    const uint32_t flags=((bits==8u?3u:4u)<<3u)|(3u<<11u); /* width=8 */
    memset(mem,0,sizeof mem);jag_blitter_init(&b,bus);
    for(uint32_t i=0;i<32u;i++){
        for(unsigned t=0;t<pbits/8u;t++)mem[SRC+(i*pbits)/8u+t]=(uint8_t)(0x20u+i*2u+t);
    }
    memset(&mem[DST],0xBDu,64u);
    W(&b,0x00,dsta2?SRC:DST);W(&b,0x04,flags);
    W(&b,0x0C,dsta2?sx:dx);
    W(&b,0x24,dsta2?DST:SRC);W(&b,0x28,flags);
    W(&b,0x30,dsta2?dx:sx);
    W(&b,0x3C,(1u<<16)|inner);
    W(&b,0x38,SRCEN|LFU_S|(dsta2?DSTA2:0u)|(extra_read?SRCENX:0u));
    for(uint32_t i=0;i<inner;i++){
        for(unsigned t=0;t<pbits/8u;t++){
            const uint32_t from=SRC+((sx+i)*pbits)/8u+t;
            const uint32_t to=DST+((dx+i)*pbits)/8u+t;
            if(mem[to]!=mem[from])fprintf(stderr,"data mismatch %u %d sx%u dx%u i%u byte%u actual=%02x expected=%02x\n",
                   bits,dsta2,sx,dx,i,t,mem[to],mem[from]);
            assert(mem[to]==mem[from]);
        }
    }
    /* Idealized model must report missing extra phrase fetch, not silently
     * claim the direct read means real Jaguar hardware would have succeeded. */
    assert(!!(b.unsupported&JB_UNSUP_SRCENX_ALIGN)==!!(needs_extra&&!extra_read));
    assert(b.blits==1u);
    (void)pp;
}
int main(void)
{
    for(unsigned bits=8;bits<=16;bits*=2u){
        const uint32_t pp=64u/bits;
        for(int dsta2=0;dsta2<=1;dsta2++){
            check(bits,dsta2,pp-2u,0u,pp,0,1);
            check(bits,dsta2,pp-2u,0u,pp,1,1);
            check(bits,dsta2,0u,2u,pp-2u,0,0);
            check(bits,dsta2,pp-1u,0u,1u,0,0);
            check(bits,dsta2,pp-1u,0u,2u,0,1);
            check(bits,dsta2,pp-1u,0u,2u,1,1);
        }
    }
    puts("RE38 R136 Jaguar Blitter SRCENX alignment diagnostics passed");
    return 0;
}
