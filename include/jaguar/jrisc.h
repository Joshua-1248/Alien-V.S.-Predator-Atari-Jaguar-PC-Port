#ifndef AVP_JAGUAR_JRISC_H
#define AVP_JAGUAR_JRISC_H
/* Atari Jaguar GPU (TOM) / DSP (JERRY) RISC instruction-set core.
 *
 * Source: Atari "Jaguar Technical Reference Manual - Tom & Jerry", Revision 8
 * (28 Feb 2001): GPU pp.29-63, DSP pp.97-111, bugs list pp.133-141.
 *
 * Model: architectural (score-boarded, in-order) semantics.  The manual states
 * the score-board makes register/flag reads wait for pending results, so a
 * program observes sequential semantics except for the documented hazards the
 * score-board does not cover (bugs 2, 13, 16, 25).  Those hazards are NOT
 * reproduced: AVP code that relies on them would be a defect and is reported
 * by the optional hazard checker rather than silently emulated.
 * Delay slot: "The transfer of control does not occur until the instruction
 * after the jump instruction has been executed" (p.35/p.39) - modelled exactly.
 * Interrupts: p.38 - vector RAMBASE+16*n, IMASK set, bank 0 forced, R31-=4 and
 * the address of the last executed instruction stored at (R31), R30 used as
 * the scratch the hardware corrupts.  Never taken between a jump and its delay
 * slot, inside MOVEI, or inside an IMULTN/IMACN..RESMAC group (p.39).
 * Cycle accounting is a documented-cost estimate (1 tick per instruction, +2
 * indexed address, +3 after a taken jump, divide 16), not a pipeline model. */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { JRISC_GPU=0, JRISC_DSP=1 };
/* Atari JDEV0002 1995 hardware errata: potential-risk observations only.
 * The cycle-approximate interpreter does NOT simulate unreliable results. */
#define JRISC_ERRATA_EXT_BRANCH       (1u<<0) /* GPU/DSP JR/JUMP in ext RAM */
#define JRISC_ERRATA_DMAEN            (1u<<1) /* high priority; conflicts with OP */
#define JRISC_ERRATA_HIDATA_CHANGED   (1u<<2) /* GPU external load after LOADP */
#define JRISC_ERRATA_DSP_STORE        (1u<<3) /* DSP ext write without preceding ext read */
#define JRISC_ERRATA_FLAGS_PIPELINE   (1u<<4) /* less than 2 NOPs after STORE to FLAGS */

/* External (main bus) access.  Width in bytes: 1,2,4. Big-endian. */
typedef struct JRiscBus {
    void *ctx;
    uint32_t (*read)(void *ctx,uint32_t addr,int width);
    void (*write)(void *ctx,uint32_t addr,uint32_t value,int width);
} JRiscBus;

/* Flags register bits (p.58, DSP p.106) */
#define JRISC_FLAG_Z        0x00000001u
#define JRISC_FLAG_C        0x00000002u
#define JRISC_FLAG_N        0x00000004u
#define JRISC_FLAG_IMASK    0x00000008u
#define JRISC_FLAG_REGPAGE  0x00004000u
#define JRISC_FLAG_DMAEN    0x00008000u
/* Control/status (p.60) */
#define JRISC_CTRL_GO          0x00000001u
#define JRISC_CTRL_CPUINT      0x00000002u
#define JRISC_CTRL_INT0        0x00000004u
#define JRISC_CTRL_SINGLE_STEP 0x00000008u
#define JRISC_CTRL_SINGLE_GO   0x00000010u
#define JRISC_CTRL_BUS_HOG     0x00000800u

typedef struct JRisc {
    int kind;                 /* JRISC_GPU / JRISC_DSP */
    uint32_t reg[2][32];      /* bank 0 primary, bank 1 secondary */
    uint32_t pc;
    uint32_t flags;           /* Z C N IMASK INT_ENA REGPAGE DMAEN (stored form) */
    uint32_t latches;         /* interrupt latches bits 0..5 */
    uint32_t go;              /* GPUGO/DSPGO */
    uint32_t ctrl_misc;       /* SINGLE_STEP, BUS_HOG as written */
    uint32_t mtxc, mtxa, bigend, hidata, remain, divctrl, modulo;
    int64_t acc;              /* MAC accumulator (GPU 32-bit, DSP 40-bit) */
    int mac_active;           /* inside IMULTN/IMACN group */
    uint32_t base;            /* F02000 (GPU) / F1A000 (DSP) */
    uint32_t ram_base, ram_size;
    uint8_t ram[8192];        /* big-endian byte image of local RAM */
    const uint8_t *wave_rom;  /* DSP F1D000..F1DFFF (user supplied), or NULL */
    JRiscBus bus;
    /* pipeline-visible state */
    int delay_pending;        /* a taken jump waits for its delay slot */
    uint32_t delay_target;
    uint32_t last_pc;         /* address of last executed instruction */
    uint64_t cycles;
    uint64_t instructions;
    /* Raised towards the host (CPUINT write). Cleared by the host. */
    int cpu_irq;
    /* diagnostics (not architectural) */
    uint32_t fault;           /* nonzero after an undefined-condition stop */
    uint32_t fault_pc;
    int check_hazards;        /* report score-board-uncovered hazards */
    uint32_t hazards;
    uint32_t errata_risks; /* JDEV0002 risk bitset; diagnostic only */
    uint64_t errata_counts[5];
    unsigned preceding_external_read, gpu_hidata_loaded, flags_nops_pending;
    int executing; /* distinguish processor STORE from host MMIO write */
    int last_was_div; uint32_t last_div_dst;
    uint64_t wave_rom_reads;  /* DSP reads of F1D000..F1DFFF */
    uint64_t op_count[64];    /* executed-opcode histogram (usage audit) */
} JRisc;

void jrisc_init(JRisc *j,int kind,JRiscBus bus);
void jrisc_reset(JRisc *j);
/* Register-file view (current bank) */
uint32_t *jrisc_regs(JRisc *j);
/* Access to the processor's own local space (control regs + RAM) as seen by
 * itself or by another bus master (32-bit, big-endian).  Returns 1 if handled. */
int jrisc_local_read(JRisc *j,uint32_t addr,uint32_t *out);
int jrisc_local_write(JRisc *j,uint32_t addr,uint32_t value);
int jrisc_in_local(const JRisc *j,uint32_t addr);
/* Assert interrupt source n (GPU 0..4, DSP 0..5): sets its latch. */
void jrisc_raise(JRisc *j,unsigned n);
/* Execute one instruction (or take an interrupt). Returns cycles used,
 * 0 if stopped. */
unsigned jrisc_step(JRisc *j);
/* Run until at least `cycles` ticks elapse or the processor stops. */
uint64_t jrisc_run(JRisc *j,uint64_t cycles);

/* Pure helpers shared with tests */
uint32_t jrisc_div_quotient(uint32_t dividend,uint32_t divisor,int offset_mode,
                            uint32_t *remainder_reg);
const char *jrisc_mnemonic(int kind,unsigned opcode,unsigned reg1);

#ifdef __cplusplus
}
#endif
#endif
