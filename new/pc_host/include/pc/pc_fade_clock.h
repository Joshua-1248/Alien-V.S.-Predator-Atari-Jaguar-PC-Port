#ifndef AVP_PC_FADE_CLOCK_H
#define AVP_PC_FADE_CLOCK_H
#include <stdint.h>

/* One source-derived FadeDown increment (+2 toward $80) per NTSC
 * VBlank. The portable presenter must not add its own fixed per-frame
 * delay after expensive framebuffer work; elapsed time owns the tick. */
enum { AVP_PC_FADE_TICKS = 0x80 / 2 };
#define AVP_PC_NTSC_VBL_NS (1000000000ull / 60ull)
static inline unsigned avp_pc_fade_ticks_due(uint64_t elapsed_ns)
{
    uint64_t n=elapsed_ns/AVP_PC_NTSC_VBL_NS;
    return n>=AVP_PC_FADE_TICKS?AVP_PC_FADE_TICKS:(unsigned)n;
}
#endif
