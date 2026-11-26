/* RE39 R178 — original COLLIDE.S 68000 SUB.L / NEG.L arithmetic.
 * Include the production implementation so this test exercises its internal
 * signed-delta helper, without adding any game-visible testing hooks. */
#include "../src/game/collision.c"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static s32 reference_68000_abs(s32 a,s32 b)
{
    /* Independent 64-bit oracle: wrap SUB.L to the signed longword range,
     * then model NEG.L. $80000000 self-negates on the original CPU. */
    int64_t d=(int64_t)a-(int64_t)b;
    if(d>INT32_MAX)d-=INT64_C(4294967296);
    else if(d<INT32_MIN)d+=INT64_C(4294967296);
    if(d<0&&d!=INT32_MIN)d=-d;
    return (s32)d;
}

int main(void)
{
    static const s32 edges[]={
        INT32_MIN,INT32_MIN+1,-65536,-32769,-1,0,1,32767,65535,
        INT32_MAX-1,INT32_MAX
    };
    unsigned count=0;
    for(unsigned i=0;i<sizeof(edges)/sizeof(edges[0]);i++)
        for(unsigned j=0;j<sizeof(edges)/sizeof(edges[0]);j++){
            const s32 a=edges[i],b=edges[j];
            assert(wrapped_abs_delta(a,b)==reference_68000_abs(a,b));
            count++;
        }
    /* Every 16.16 fraction at fixed, valid maze-cell coordinates: parity
     * must remain exact for the normal single-player movement domain. */
    for(unsigned frac=0;frac<65536u;frac++){
        const s32 a=(s32)((15u<<16)|frac);
        const s32 b=(s32)((16u<<16)|((frac*17u)&65535u));
        assert(wrapped_abs_delta(a,b)==reference_68000_abs(a,b));
        count++;
    }
    /* Deterministic synthetic full-longword sweep around wrap edges. */
    {
        u32 x=0x6bbec145u,y=0x981fac2bu;
        for(unsigned k=0;k<100000u;k++){
            x=x*1664525u+1013904223u;
            y=y*22695477u+1u;
            const s32 a=x<=INT32_MAX?(s32)x:(s32)((int64_t)x-INT64_C(4294967296));
            const s32 b=y<=INT32_MAX?(s32)y:(s32)((int64_t)y-INT64_C(4294967296));
            assert(wrapped_abs_delta(a,b)==reference_68000_abs(a,b));
            count++;
        }
    }
    assert(wrapped_abs_delta(INT32_MIN,0)==INT32_MIN);
    assert(wrapped_abs_delta(INT32_MAX,-1)==INT32_MIN);
    assert(wrapped_abs_delta(INT32_MIN,INT32_MAX)==1);
    printf("R178 COLLIDE.S wrapped absolute deltas: %u cases PASS\n",count);
    return 0;
}
