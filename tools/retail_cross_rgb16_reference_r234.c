/* R234 standalone original-ROM-derived 32-bit RGB16 CROSS arithmetic.
 * Independent offline verification reference; NOT linked to the shipped game.
 * Each table entry expresses exact original RISC LOAD / MOVE / SHRQ / AND /
 * SUB / ADD / STORE operations at CROSS $F03170..$F0342A.
 * Phase 7 uses the incoming word as the base. Phases 10-16 also use the
 * incoming base. In particular, do NOT rewrite these as 16-bit linear cross
 * fractions: the original packed 32-bit arithmetic/truncations are different.
 * Original 68000/GPU source/art is not embedded here.
 */
#include <stdint.h>

typedef struct R234CrossKernel {
    uint8_t base_is_new;
    uint8_t terms;
    uint8_t shift_a;
    uint8_t shift_b;
} R234CrossKernel;

static const R234CrossKernel k[17] = {
    {0,0,0,0}, {0,1,4,0}, {0,1,3,0}, {0,2,3,4},
    {0,1,2,0}, {0,2,2,4}, {0,2,2,3}, {1,2,1,4},
    {0,1,1,0}, {0,2,1,4}, {1,2,2,3}, {1,2,2,4},
    {1,1,2,0}, {1,2,3,4}, {1,1,3,0}, {1,1,4,0},
    {1,0,0,0}
};

static uint32_t channel_mask(unsigned shift)
{
    switch (shift) {
    case 1: return UINT32_C(0x7BDF7BDF);
    case 2: return UINT32_C(0x39CF39CF);
    case 3: return UINT32_C(0x18C718C7);
    case 4: return UINT32_C(0x08430843);
    default: return 0;
    }
}

uint32_t r234_original_cross_word(uint32_t old_word,uint32_t new_word,unsigned phase)
{
    if (phase>16u) return 0; /* invalid dispatch; caller must validate */
    const R234CrossKernel spec=k[phase];
    const uint32_t base=spec.base_is_new?new_word:old_word;
    const uint32_t other=spec.base_is_new?old_word:new_word;
    uint32_t out=base;
    for (unsigned i=0;i<spec.terms;i++) {
        const unsigned shift=i?spec.shift_b:spec.shift_a;
        const uint32_t mask=channel_mask(shift);
        out-=((base>>shift)&mask);
        out+=((other>>shift)&mask);
    }
    return out;
}
