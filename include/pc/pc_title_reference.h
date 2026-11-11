#ifndef AVP_PC_TITLE_REFERENCE_H
#define AVP_PC_TITLE_REFERENCE_H
/*
 * PC-only interpretation of the recovered retail title GPU/OP sequence.
 * Field zero corresponds to first visible title OP publication (reference
 * model field 314). These are measured MODELED transitions, not a claim of
 * pixel/cycle-accurate Jaguar hardware execution. Original source controls
 * the title GPU jobs and the two interruptible Pause() calls (30,260).
 * See docs/RE34_R78_TITLE_REFERENCE.md. Never redistribute retail ROM data.
 */
/* Original cartridge: $0087DE JSR $0099B2 with A0=$020918 executes
 * around model field 43, while the OP display remains black. The first
 * nonzero modeled I2S sample arrives ~field 292, before the ALIEN begins
 * at field 317. PC playback must NOT start only after the black preroll. */
#define AVP_PC_TITLE_MUSIC_START_FIELD 43u
#define AVP_PC_TITLE_COLD_BOOT_FIELDS 313u
#define AVP_PC_TITLE_SEQUENCE_FIELDS 493u
#define AVP_PC_TITLE_ALIEN_FIRST 3u       /* field 317: first visible pixel */
#define AVP_PC_TITLE_ALIEN_LAST 72u       /* field 386: GPU phase 35 */
#define AVP_PC_TITLE_ALIEN_STAMP 74u      /* field 388: next GPU job begins */
/* R79: machine-model RGB16 comparisons place the first coloured backdrop
 * near field 350 (local 36), with full brightness around field 386
 * (local 72). The old R78 local 32..47 range completed ~25 fields early.
 * This is a measured model envelope, NOT recovered GPU instructions. */
#define AVP_PC_TITLE_BACKGROUND_START 35u /* field ~349: fade begins */
#define AVP_PC_TITLE_BACKGROUND_FULL 72u /* field ~386: largely complete */
#define AVP_PC_TITLE_PREDATOR_JOB_START 74u
/* R79: GPU-stage opacity is not monotonic; the original reference model
 * shows a first rise, a brief drop, then a second rise to full opacity. */
#define AVP_PC_TITLE_PREDATOR_VISIBLE 82u /* field 396: reveal envelope begins */
#define AVP_PC_TITLE_PREDATOR_FULL 154u /* field 468: full-sized logo */
#define AVP_PC_TITLE_PREDATOR_MOVE 174u  /* field 488, Update2 phase 1 */
#define AVP_PC_TITLE_PREDATOR_MOVE_END 192u /* field 506, phase 35 */
#define AVP_PC_TITLE_PREDATOR_STAMP 195u /* field 509: scaled object hidden */
#define AVP_PC_TITLE_WAIT30_START 195u
#define AVP_PC_TITLE_WAIT260_START 225u
#define AVP_PC_TITLE_WAIT_END 485u
#define AVP_PC_TITLE_LEGAL 225u /* field 539, legal image visible */
#define AVP_PC_TITLE_FINAL_GPU 482u /* field 796 */
/* No unverified extra Fire gate: selector begins automatically. */
#define AVP_PC_TITLE_SKIP_EVENT 0x54534b50u /* 'TSKP': advance display-only clock */

/* Keep VBlank deadlines tied to a nominal 60-Hz video clock rather than
 * sleeping an entire 16.7 ms AFTER software rendering. This scheduler
 * neither skips simulation fields nor changes the clock outside the title.
 * Reset a stale clock (e.g. long window suspend) instead of catch-up spin. */
#include <stdint.h>
static inline uint64_t avp_pc_title_next_deadline(uint64_t previous,uint64_t now)
{
    if(previous==0u || now>previous+1000000000ull ||
       previous>now+1000000000ull)
        return now+16666667ull;
    return previous+16666667ull;
}
#endif
