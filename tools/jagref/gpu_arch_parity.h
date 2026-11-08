#ifndef AVP_RE24_GPU_ARCH_PARITY_H
#define AVP_RE24_GPU_ARCH_PARITY_H
/* RE24-S16: compare the architectural state of the original JRISC GPU to
 * the statically recompiled context.  This is verification-only; never part
 * of the playable AVP port.  Do NOT compare the recompiled machine's GPU PC
 * or condition flags directly: generated C tracks these in `endpc` and `g`,
 * not in the JRisc bus adapter.  Caller supplies the original JRisc state. */
#include "jaguar/jrisc.h"
#include <stdint.h>
#include <string.h>

enum {
    AVP_GPU_DIFF_REGS     = 1u << 0,
    AVP_GPU_DIFF_BANK     = 1u << 1,
    AVP_GPU_DIFF_FLAGS    = 1u << 2,
    AVP_GPU_DIFF_ACC      = 1u << 3,
    AVP_GPU_DIFF_REMAIN   = 1u << 4,
    AVP_GPU_DIFF_DIVCTRL  = 1u << 5,
    AVP_GPU_DIFF_PC       = 1u << 6
};
/* AvpGpuRecompCtx must be declared by the including test/checker. */
static uint32_t avp_gpu_arch_diff(const JRisc *original,
                                  const AvpGpuRecompCtx *generated,
                                  uint32_t endpc)
{
    uint32_t diff = 0;
    unsigned original_bank = (original->flags & JRISC_FLAG_IMASK) ? 0u :
        ((original->flags & JRISC_FLAG_REGPAGE) ? 1u : 0u);
    uint32_t zcn = (generated->z ? JRISC_FLAG_Z : 0u) |
                   (generated->c ? JRISC_FLAG_C : 0u) |
                   (generated->n ? JRISC_FLAG_N : 0u);
    if (memcmp(original->reg, generated->r, sizeof original->reg)) diff |= AVP_GPU_DIFF_REGS;
    if (original_bank != generated->bank) diff |= AVP_GPU_DIFF_BANK;
    if ((original->flags & 7u) != zcn) diff |= AVP_GPU_DIFF_FLAGS;
    if (original->acc != generated->acc) diff |= AVP_GPU_DIFF_ACC;
    if (original->remain != generated->remain) diff |= AVP_GPU_DIFF_REMAIN;
    if ((original->divctrl & 1u) != (unsigned)generated->divoff) diff |= AVP_GPU_DIFF_DIVCTRL;
    if (original->pc != endpc) diff |= AVP_GPU_DIFF_PC;
    return diff;
}
#endif
