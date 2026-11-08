#ifndef AVP_JAGUAR_BLITTER_TRM_H
#define AVP_JAGUAR_BLITTER_TRM_H
/* TOM Blitter, implemented from the Atari Jaguar Technical Reference Manual
 * Rev.8 pp.64-82 and bugs 9-11,17-20,26-29 (pp.135-140).
 *
 * The blit is evaluated pixel by pixel.  Phrase-mode blits produce the same
 * memory result as the hardware's phrase transfers because the hardware masks
 * partial first/last phrases and re-aligns source data to the destination
 * (p.67); bus timing is NOT modelled (an estimated tick count is reported).
 * Every command executed is recorded in a usage histogram so that the modes
 * the original game actually issues can be audited individually. */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JagBlitBus {
    void *ctx;
    uint8_t (*read8)(void *ctx,uint32_t addr);
    void (*write8)(void *ctx,uint32_t addr,uint8_t v);
} JagBlitBus;

typedef struct JagBlitter {
    uint32_t a1_base,a1_flags,a1_clip,a1_pixel,a1_step,a1_fstep,a1_fpixel,a1_inc,a1_finc;
    uint32_t a2_base,a2_flags,a2_mask,a2_pixel,a2_step;
    uint32_t cmd,count;
    uint64_t srcd,dstd,dstz,srcz1,srcz2,patd;
    uint32_t iinc,zinc,stop;
    JagBlitBus bus;
    /* status */
    int stopped;
    uint64_t blits, pixels_written, est_ticks;
    /* usage audit: (cmd & mode mask) histogram, small open table */
    struct { uint32_t cmd,a1f,a2f; uint32_t n; } usage[256];
    unsigned usage_n;
    uint32_t unsupported; /* bitmask of feature combinations not modelled */
    /* Atari JDEV0002: observable hardware-risk configuration flags, never
       synthetic clipping, corruption, or bus stalls. Cumulative over blits. */
    uint32_t errata_risks;
    uint64_t errata_counts[4];
    /* optional observer called with the register state at each blit start */
    void (*on_blit)(void *ctx,const struct JagBlitter *b); void *on_blit_ctx;
} JagBlitter;

#define JB_ERRATA_PHANTOM_CLIP   (1u<<0) /* non-phrase-aligned A1_CLIP.x */
#define JB_ERRATA_2BPP_ALIGN     (1u<<1) /* unaligned 2-bpp blit */
#define JB_ERRATA_Z_COLOUR       (1u<<2) /* Z and ADDDSEL/SRCSHADE */
#define JB_ERRATA_PRIORITY       (1u<<3) /* B_CMD.BUSHI vs active OP */
/* Pure classifier for Atari JDEV0002, Blitter items 3-5 and GPU/DSP item 5.
   These are potential hardware hazards, not predictions of bad pixels.
   BUSHI is a conflict only while OP owns the bus, which is not known here. */
uint32_t jag_blitter_errata_risks(const JagBlitter *b);

#define JB_UNSUP_COLLISION   0x01u
#define JB_UNSUP_SRCSHADE    0x02u
#define JB_UNSUP_ADDDSEL_SUB16 0x04u

void jag_blitter_init(JagBlitter *b,JagBlitBus bus);
/* Register write at byte offset 0x00..0x9C from F02200 (32-bit units, high
 * long of 64-bit data registers at the lower address). Writing CMD starts the
 * blit synchronously. Returns 1 if the offset is a blitter register. */
int jag_blitter_write(JagBlitter *b,uint32_t offset,uint32_t value);
int jag_blitter_read(JagBlitter *b,uint32_t offset,uint32_t *value);
/* Pure helper: decode the 6-bit floating-point window width (p.70) */
uint32_t jag_blitter_width(uint32_t flags);

#ifdef __cplusplus
}
#endif
#endif
