#ifndef AVP_JAGUAR_HOST_DSP_ARITHMETIC_H
#define AVP_JAGUAR_HOST_DSP_ARITHMETIC_H

#include <stdint.h>

/* Jaguar DSP signed arithmetic right shift (sign extending, rounding toward
 * negative infinity).  A negative signed operand shifted right is
 * implementation-defined in ISO C.  Work in unsigned magnitude instead;
 * the result fits int64_t for shifts >= 1, including INT64_MIN.
 * No host compiler assumptions or hardware ROM data are required. */
static inline int64_t avp_jerry_asr64(int64_t value, unsigned bits)
{
    uint64_t magnitude, round, quotient;
    if(bits==0u)return value;
    if(bits>=64u)return value<0?-1:0;
    if(value>=0)return (int64_t)((uint64_t)value>>bits);
    magnitude=UINT64_C(0)-(uint64_t)value;
    round=(UINT64_C(1)<<bits)-1u;
    quotient=(magnitude+round)>>bits;
    return -(int64_t)quotient;
}

/* Jaguar IMULTN/IMACN use two signed low-16 operands and accumulate in
 * a 40-bit MAC. RESMAC transfers only the LOW 32 BITS to a general-purpose
 * register, where the following SHARQ is a signed 32-bit arithmetic shift.
 * With exactly two products the signed sum fits 40 bits; use int64_t for the
 * accumulator, then explicitly narrow the returned register word.  This is
 * intentionally NOT SAT32S (which would clamp using the MAC high eight bits).
 * The function is host-side hardware emulation, not an added ROM resource. */
static inline int64_t avp_jerry_mac2_resmac_asr(int32_t a,int32_t wa,
                                               int32_t b,int32_t wb,
                                               unsigned shift)
{
    int64_t mac=(int64_t)a*wa+(int64_t)b*wb;
    uint32_t word=(uint32_t)(uint64_t)mac;
    int32_t result=word<=UINT32_C(0x7fffffff)?(int32_t)word:
        (int32_t)((int64_t)word-INT64_C(0x100000000));
    return avp_jerry_asr64(result,shift);
}

#endif
