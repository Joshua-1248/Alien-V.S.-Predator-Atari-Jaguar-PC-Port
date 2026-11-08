#ifndef AVP_JAGUAR_OBJECT_PROCESSOR_H
#define AVP_JAGUAR_OBJECT_PROCESSOR_H

#include "avp_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TOM bitmap-object phrase sampler: 4-bpp, HILO=1, contiguous data.
 * Each 64-bit phrase contains 16 four-bit pixels.  The next source row
 * begins at the next whole phrase (the OP's DWIDTH is in phrases).
 * This primitive intentionally does not simulate an entire OP object list,
 * scaling, REFLECT, RMW or timing; it is used by AVP's already-decoded pause
 * brushes, whose authored widths are all multiples of 16. */
u8 avp_jaguar_op4_contiguous_pixel(const u8 *src,size_t source_bytes,
                                   unsigned source_width,unsigned x,unsigned y);

/* TOM OP bitmap/scaled-bitmap phrase fields, Atari TRM Rev.8 pp.18-19.
 * The four (or six) 68000 longwords are in Jaguar memory order: the first
 * longword holds bits 63..32 of each phrase and the second bits 31..0.
 * This is a read-only decoder, not an executable OP scanline interpreter. */
typedef struct AvpJaguarOpBitmapHeader {
    unsigned type, ypos, height;
    u32 link_address, data_address;
    int xpos;
    unsigned depth, pitch, dwidth, iwidth, index;
    unsigned reflect, rmw, trans, release, firstpix;
    unsigned hscale, vscale, remainder; /* only valid if type==1 */
} AvpJaguarOpBitmapHeader;

/* Returns 1 for a complete, valid type-0/type-1 header; 0 otherwise.
 * A type-1 object requires six 32-bit words, type-0 requires four. */
int avp_jaguar_op_decode_bitmap_words(const u32 *words, size_t word_count,
                                      AvpJaguarOpBitmapHeader *out);

/* TOM TYPE=3 branch object (TRM Rev.8 p.20). A branch occupies one
 * 64-bit phrase. As for bitmap decoding, the two words are the Jaguar
 * CPU-visible longwords in high/low phrase order. This decodes only the
 * four representable two-bit CC values 0..3. The manual additionally
 * mentions "CC=4" (HC10), inconsistently with its 2-bit field definition;
 * no fifth condition is invented here. */
typedef struct AvpJaguarOpBranchHeader {
    unsigned ypos; /* 11-bit vertical half-line */
    unsigned condition; /* CC 0..3 */
    u32 link_address; /* 8-byte-aligned target */
} AvpJaguarOpBranchHeader;

int avp_jaguar_op_decode_branch_words(const u32 *words, size_t word_count,
                                      AvpJaguarOpBranchHeader *out);
/* Predicate only: does not fetch or mutate the OP object list.
 * vertical_count uses VC's documented low 11 bits; obf uses bit zero. */
int avp_jaguar_op_branch_taken(const AvpJaguarOpBranchHeader *branch,
                               unsigned vertical_count,unsigned obf);

/* Read a logical 4-bpp HILO=1 source pixel using the OP's explicit DWIDTH
 * and PITCH in 64-bit phrases (TRM pp.18-19). This returns the unmodified
 * nibble, before INDEX/CLUT, FIRSTPIX, REFLECT, RMW or TRANS handling. */
u8 avp_jaguar_op4_phrased_pixel(const u8 *src,size_t source_bytes,
                                unsigned width,unsigned dwidth_phrases,
                                unsigned pitch_phrases,unsigned x,unsigned y);

/* Read-only TOM OP scanline object-list traversal (TRM Rev.8 pp.18-20).
 * Jag memory is supplied as CPU-visible BIG-ENDIAN bytes. The window may
 * begin at any physical address; only header bytes are read, never altered.
 * This helper intentionally stops at a matching GPU interrupt object and
 * does NOT simulate OP writeback, display pixels, GPU service or bus timing.
 * The original AVP PRE_LIST can be checked with this API without changing
 * the existing host renderer. */
typedef enum AvpJaguarOpTraceStatus {
    AVP_JAG_OP_TRACE_STOP = 0, /* STOP object reached */
    AVP_JAG_OP_TRACE_GPU = 1, /* matching GPU interrupt: requires service */
    AVP_JAG_OP_TRACE_BAD_INPUT = 2,
    AVP_JAG_OP_TRACE_OUT_OF_RANGE = 3,
    AVP_JAG_OP_TRACE_INVALID_OBJECT = 4,
    AVP_JAG_OP_TRACE_CYCLE = 5,
    AVP_JAG_OP_TRACE_LIMIT = 6
} AvpJaguarOpTraceStatus;

typedef struct AvpJaguarOpTraceStep {
    u32 address;     /* physical address of header phrase */
    u32 next_address; /* next header address, or resume PC after GPU */
    unsigned type;   /* 0 bitmap, 1 scaled, 2 GPU, 3 branch, 4 stop */
    unsigned active; /* bitmap visible OR GPU interrupt requested */
    unsigned branch_taken; /* meaningful only for type 3 */
} AvpJaguarOpTraceStep;

typedef struct AvpJaguarOpTraceResult {
    AvpJaguarOpTraceStatus status;
    size_t count;    /* initialized trace entries; never exceeds capacity */
    u32 address;     /* STOP/GPU/error/next-to-process address */
} AvpJaguarOpTraceResult;

AvpJaguarOpTraceResult avp_jaguar_op_trace_scanline(
    const u8 *memory,size_t memory_size,u32 memory_base,u32 object_list_address,
    unsigned vertical_count,unsigned object_flag,
    AvpJaguarOpTraceStep *steps,size_t step_capacity);

/* TOM unscaled 16-bit CRY line-buffer composition (TRM Rev.8 pp.18-23).
 * This is an isolated original-hardware primitive; it is NOT wired to the
 * live game renderer or to the scanline list trace. Source bytes are the
 * CPU-visible BIG-ENDIAN Jaguar memory image, with the header DATA pointing
 * to the first phrase of the current source row. The caller provides the
 * complete 256-entry physical CRY CLUT for 1/2/4/8-bpp objects.
 * Supports HILO=1, 1/2/4/8-bpp indexed or 16-bpp direct (depth 0..4),
 * FIRSTPIX on unscaled two-pixel boundaries, PITCH, REFLECT, TRANS on
 * indexed logical zero, and CRY RMW. No GPU/OP bus arbitration, raster
 * timing, other VMODEs or scaled objects are inferred. In particular, the
 * manual does not fully specify reserved physical transparent colors;
 * TRANS on direct 16-bpp objects is rejected rather than guessed.
 */
typedef enum AvpJaguarOpPixelStatus {
    AVP_JAG_OP_PIXEL_OK = 0,
    AVP_JAG_OP_PIXEL_BAD_INPUT = 1,
    AVP_JAG_OP_PIXEL_UNSUPPORTED = 2,
    AVP_JAG_OP_PIXEL_OUT_OF_RANGE = 3
} AvpJaguarOpPixelStatus;

typedef struct AvpJaguarOpPixelResult {
    AvpJaguarOpPixelStatus status;
    size_t drawn; /* visible, non-transparent destination pixels written */
} AvpJaguarOpPixelResult;

AvpJaguarOpPixelResult avp_jaguar_op_compose_unscaled_cry16(
    const AvpJaguarOpBitmapHeader *header,
    const u8 *source_memory,size_t source_size,u32 source_base,
    const u16 *clut,size_t clut_entries,
    u16 *line_pixels,size_t line_width);

/* TOM TYPE=1 SCBITOBJ, isolated 16-bit CRY scanline composition.
 * Same source CLUT, 64-bit phrase, REFLECT and CRY RMW domain as the
 * unscaled primitive, but one pixel at a time.  HSCALE is in units of
 * 1/32 source pixel; for output pixel x, source index = floor(x*32/HSCALE).
 * FIRSTPIX includes its lowest significant source-pixel bit for scaled
 * objects (TRM Rev.8 pp.19,22). Does NOT perform vertical scaling,
 * change object headers, or replace AVP's active frontend renderer.
 * A zero HSCALE and direct physical-color TRANS are intentionally rejected.
 */
AvpJaguarOpPixelResult avp_jaguar_op_compose_scaled_cry16(
    const AvpJaguarOpBitmapHeader *header,
    const u8 *source_memory,size_t source_size,u32 source_base,
    const u16 *clut,size_t clut_entries,
    u16 *line_pixels,size_t line_width);

/* Independent TOM 24-bpp physical source path (TRM Rev.8 pp.17,21-23).
 * The Object Processor reads two 32-bit pixel slots per 64-bit phrase
 * (one byte per slot is unused), bypasses the CLUT and emits RGB24
 * into 32-bit physical line-buffer entries. This routine keeps that
 * separate from the CRY16 compositor and host framebuffer. Supports
 * TYPE=0, DEPTH=5, PITCH, REFLECT and display clipping. FIRSTPIX,
 * TRANS and RMW are rejected because their direct-24-bit semantics
 * have not been independently established. Transactional on failure.
 * Not used by the active renderer or CRY16 scanline pipeline. */
AvpJaguarOpPixelResult avp_jaguar_op_compose_unscaled_rgb24(
    const AvpJaguarOpBitmapHeader *header,
    const u8 *source_memory,size_t source_size,u32 source_base,
    u32 *line_pixels,size_t line_width);

/* A single TYPE=1 source-row progression AFTER rendering one scanline.
 * The ROM-authored REMAINDER is 3.5 fixed-point (32 = one line); AVP's
 * reconstructed title compositor uses precisely this accumulator ordering.
 * No DATA/HEIGHT writeback is performed, since object-list memory updates,
 * interlaced field steps and bus scheduling are separate fidelity tasks. */
typedef struct AvpJaguarOpScaledVerticalStep {
    unsigned next_remainder;  /* after subtracting 0x20 */
    unsigned source_lines_advanced;
} AvpJaguarOpScaledVerticalStep;
int avp_jaguar_op_scaled_vertical_step(
    const AvpJaguarOpBitmapHeader *header,
    AvpJaguarOpScaledVerticalStep *out);

/* TOM bitmap-header writeback after one non-interlaced visible source line.
 * TRM Rev.8 pp.18-22: unscaled source DATA advances by DWIDTH phrases;
 * scaled source DATA advances by DWIDTH per source row consumed, with
 * REMAINDER and HEIGHT updated. Input/output longwords are CPU-visible
 * Jaguar BE phrase order; reserved bits and all other fields are preserved.
 * The caller supplies only a bitmap already active on the current scanline.
 * No GPU interrupt, bus timing, interlaced field, or line buffer is modeled.
 * `source_rows` may be NULL. A zero HEIGHT is a successful no-op.
 * Fails without changing any output on invalid inputs/headers.
 */
int avp_jaguar_op_advance_bitmap_words(
    const u32 *words,size_t word_count,u32 *updated_words,
    unsigned *source_rows);

/* Bounded memory-side version for an isolated Jaguar big-endian RAM window.
 * Transactional: fails without changing RAM if the full bitmap header is
 * unavailable or invalid. Not connected to the live PC renderer yet.
 * The data address is a 24-bit Jaguar phrase address, so phrase addition
 * wraps modulo 2^21. Does not update inactive bitmap objects automatically.
 */
int avp_jaguar_op_writeback_bitmap_memory(
    u8 *memory,size_t memory_size,u32 memory_base,u32 object_address,
    unsigned *source_rows);

/* RE24 Sweep 06: isolated, STOP-terminated Object Processor scanline
 * composition over CPU-visible Jaguar RAM and a CRY16 physical line buffer.
 * The walker, source samplers and bitmap writeback primitives are combined
 * in original list order. The caller provides an independent scratch line
 * and trace workspace; nothing is committed until every object has been
 * decoded, composited and validated. The Object Processor GPU-interrupt
 * handoff is an explicit non-committing result, NOT a simulated GPU service.
 * The caller must not overlap scratch_line with line_pixels, or mutate the
 * Jaguar memory/source windows while this routine executes.
 * Supports only the established HILO=1, indexed 1/2/4/8-bpp and CRY16
 * non-interlaced subset; unsuitable/unsupported objects return an error.
 * This test harness is not called by the active PC renderer. */
typedef enum AvpJaguarOpScanlineStatus {
    AVP_JAG_OP_SCANLINE_STOP = 0,
    AVP_JAG_OP_SCANLINE_GPU_PENDING = 1,
    AVP_JAG_OP_SCANLINE_INVALID = 2,
    AVP_JAG_OP_SCANLINE_UNSUPPORTED = 3,
    AVP_JAG_OP_SCANLINE_RANGE = 4,
    AVP_JAG_OP_SCANLINE_TRACE_ERROR = 5
} AvpJaguarOpScanlineStatus;

typedef struct AvpJaguarOpScanlineResult {
    AvpJaguarOpScanlineStatus status;
    AvpJaguarOpTraceStatus trace_status;
    size_t objects;          /* objects in the resolved traversal */
    size_t active_bitmaps;   /* bitmap headers drawn and written back */
    size_t drawn_pixels;     /* nontransparent in-range pixel writes */
    u32 stop_address;        /* terminal STOP or GPU/error address */
} AvpJaguarOpScanlineResult;

AvpJaguarOpScanlineResult avp_jaguar_op_compose_scanline_cry16(
    u8 *object_memory,size_t object_memory_size,u32 object_memory_base,
    u32 list_address,unsigned vertical_count,unsigned object_flag,
    const u8 *source_memory,size_t source_memory_size,u32 source_memory_base,
    const u16 *clut,size_t clut_entries,
    u16 *line_pixels,u16 *scratch_line,size_t line_width,
    AvpJaguarOpTraceStep *steps,size_t step_capacity);

/* TRM Rev.8 pp.21-23: TOM contains two physical 360x32-bit line
 * buffers (720 CRY16 pixels per bank).  The Object Processor prepares
 * one bank while video logic displays the other; they exchange roles at
 * the next line.  In CRY/RGB16 modes, BGEN clears a displayed bank to BG
 * *after* its pixels have been read.  This isolated CRY16 model permits
 * testing that ordering without inventing bus, scan-clock, VMODE, or OP
 * interrupt timing.  Neither a reset pattern nor an initial bank is
 * prescribed here: the caller chooses them explicitly.  Not live-wired. */
#define AVP_JAG_OP_CRY16_LINE_PIXELS 720u
typedef struct AvpJaguarOpCry16LinePair {
    u16 bank[2][AVP_JAG_OP_CRY16_LINE_PIXELS];
    unsigned write_bank; /* bank currently prepared by the OP: 0 or 1 */
} AvpJaguarOpCry16LinePair;

/* Initialize each bank to caller-provided contents, and select the bank
 * that the OP will write first.  Does not claim physical reset state. */
int avp_jaguar_op_cry16_pair_init(AvpJaguarOpCry16LinePair *pair,
    unsigned first_write_bank,u16 bank0_value,u16 bank1_value);
/* Return the physical CRY16 write bank, compatible with the scanline
 * compositor's line_pixels argument (up to 720 physical pixels). */
u16 *avp_jaguar_op_cry16_pair_write(AvpJaguarOpCry16LinePair *pair);
/* End the current OP-prepared line: the bank just written is published to
 * the video output, then optionally BGEN-cleared to background CRY, and
 * the opposite bank becomes available for next-line composition.  Only
 * the supplied output_count prefix is copied; physical BGEN clears the
 * entire 720-pixel bank.  Output must NOT alias pair banks.
 * Returns zero without modifying state on invalid input. */
int avp_jaguar_op_cry16_pair_publish(AvpJaguarOpCry16LinePair *pair,
    u16 *output,size_t output_count,int bgen,u16 background_cry);

/* TOM final video-output mux, TRM Rev.8 pp.12-13 and 22-23.
 * Consumes the already-composed physical 16-bit line-buffer words.
 * VMODE MODE=0: CRY16 (or VARMOD mixed CRY16/RGB16); MODE=1: 24-bit
 * RGB encoded across pairs of source words; MODE=3: RGB16 R5:B5:G6.
 * MODE=2 is an external direct-output format, deliberately unsupported.
 * VARMOD is supported only with MODE=0, where bit 0 of each source word
 * selects CRY (zero) or RGB16 (one) as documented on p.13; for RGB16
 * the format flag does not contribute to the GREEN component.
 * This is a colour-word model, not pixel-clock, blanking, PWIDTH, border,
 * or real DAC emulation. It is isolated from AVP's active renderer.
 * Unsupported/invalid requests leave output untouched. */
typedef enum AvpJaguarOpVideoStatus {
    AVP_JAG_OP_VIDEO_OK = 0,
    AVP_JAG_OP_VIDEO_BAD_INPUT = 1,
    AVP_JAG_OP_VIDEO_UNSUPPORTED = 2
} AvpJaguarOpVideoStatus;
typedef struct AvpJaguarOpVideoResult {
    AvpJaguarOpVideoStatus status;
    size_t pixels;
} AvpJaguarOpVideoResult;
AvpJaguarOpVideoResult avp_jaguar_op_vmode_decode_line(
    const u16 *words,size_t word_count,unsigned mode,unsigned varmod,
    u32 *pixels,size_t pixel_capacity);

typedef struct AvpJaguarPauseOpState {
    const u8 *paused,*pointer,*hud,*bar,*slider;
    const u8 *game,*one,*two,*three,*bsave;
    u8 palette_rgb[16][3];
    int assets_ready;
    int active,menu_visible,cursor_visible;
    int cursor_x,cursor_y,slider_x,bsave_visible;
    int save_flash_slot,save_flash_hidden;
    u32 *base;
    size_t base_count;
} AvpJaguarPauseOpState;

void avp_jaguar_pause_op_init(AvpJaguarPauseOpState *s);
void avp_jaguar_pause_op_destroy(AvpJaguarPauseOpState *s);
void avp_jaguar_pause_op_set_assets(AvpJaguarPauseOpState *s,
    const u8 *paused,const u8 *pointer,const u8 *hud,const u8 *bar,const u8 *slider,
    const u8 *game,const u8 *one,const u8 *two,const u8 *three,const u8 *bsave,
    const u8 palette_rgb[16][3]);
void avp_jaguar_pause_op_event(AvpJaguarPauseOpState *s,unsigned event,u32 a,u32 b,u32 c,
    u32 *pixels,unsigned width,unsigned height);
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s);
void avp_jaguar_pause_op_prepare_frame(const AvpJaguarPauseOpState *s,u32 *pixels,
    unsigned width,unsigned height);

#ifdef __cplusplus
}
#endif
#endif
