/* Jaguar signed DSP arithmetic shifts, entirely synthetic test vectors. */
#include "jaguar/host/dsp_arithmetic.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static int64_t reference(int64_t v,unsigned bits)
{
    int64_t divisor,q,r;
    if(bits==0)return v;
    if(bits>=63u)return v<0?-1:0;
    divisor=(int64_t)(UINT64_C(1)<<bits);
    q=v/divisor;r=v%divisor;
    return q-(v<0&&r!=0);
}
int main(void)
{
    static const int64_t values[]={0,1,-1,127,-127,128,-128,255,-255,256,-256,32767,-32768,-32769,
        INT32_MIN,INT32_MAX,INT64_MIN,INT64_MAX,INT64_MIN+1,INT64_MAX-1};
    uint64_t state=UINT64_C(0x9ecf57abbad00291);
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++)
        for(unsigned shift=0;shift<=64;shift++)
            assert(avp_jerry_asr64(values[i],shift)==reference(values[i],shift));
    for(unsigned i=0;i<20000;i++){
        state^=state<<13;state^=state>>7;state^=state<<17;
        /* Construct signed negatives without an out-of-range unsigned cast. */
        uint64_t magnitude=UINT64_C(0)-state;
        int64_t v=state<=INT64_MAX?(int64_t)state:
            (magnitude==UINT64_C(0x8000000000000000)?INT64_MIN:-(int64_t)magnitude);
        unsigned shift=(unsigned)(state&63u);
        assert(avp_jerry_asr64(v,shift)==reference(v,shift));
    }
    assert(avp_jerry_asr64(-1,8)==-1);
    assert(avp_jerry_asr64(-129,7)==-2);
    assert(avp_jerry_asr64(INT64_MIN,1)==(INT64_MIN/2));
    /* Two extreme MAC products can cross signed-32 without overflowing
     * JERRY's 40-bit accumulator. RESMAC truncates the 40-bit result to a
     * 32-bit register BEFORE SHARQ; shifting an int64_t MAC is incorrect.
     * These are synthetic hardware-boundary vectors, not retail wavetable
     * interpolation coefficients or game audio samples. */
    assert(avp_jerry_mac2_resmac_asr(-32768,-32768,-32768,-32768,8u)==-8388608);
    assert(avp_jerry_mac2_resmac_asr(-32768,32767,-32768,32767,8u)==-8388352);
    assert(avp_jerry_mac2_resmac_asr(32767,255,-32768,0,8u)==32639);
    assert(avp_jerry_mac2_resmac_asr(-32768,255,0,0,8u)==-32640);
    assert(avp_jerry_mac2_resmac_asr(-1,1,0,0,8u)==-1);
    assert(((int64_t)-32768*-32768+(int64_t)-32768*-32768)>>8 == 8388608);

    puts("JERRY signed DSP arithmetic shifts: PASS");
    return 0;
}
