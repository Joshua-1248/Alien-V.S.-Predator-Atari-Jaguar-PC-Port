#ifndef AVP_JAGUAR_JAG_MACHINE_H
#define AVP_JAGUAR_JAG_MACHINE_H
/* RE24-S10 Jaguar reference machine: executes the user's original cartridge
 * ROM on a 68000 core (Musashi, MIT) plus TOM/JERRY models written from the
 * Atari Jaguar Technical Reference Manual Rev.8.  It is a VERIFICATION
 * instrument - it is not linked into the native PC game - used to obtain the
 * original program's own object lists, GPU/DSP/Blitter traffic, video frames
 * and audio for comparison with the native reconstruction.  No boot ROM is
 * used or required: the cartridge entry ($802000, which inflates the program
 * itself) is started directly with the boot-time register state described in
 * jag_machine.c. */
#include <stdint.h>
#include <stddef.h>
#include "jaguar/jrisc.h"
#include "jaguar/blitter_trm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JAGM_RAM_SIZE  (2u*1024u*1024u)
#define JAGM_MAX_W     720u
#define JAGM_MAX_H     288u

typedef struct JagUnhandled { uint32_t addr; uint32_t reads,writes; } JagUnhandled;

/* RE24/S15: optional, read-only observer of TOM GPU PC/CTRL writes.  This is
 * reference-machine verification data, never part of the native PC port.
 * master: 0=68000, 1=GPU, 2=DSP, 3=Blitter, 4=Object Processor.
 * cycles is the most recently completed scheduler slice, not bus-exact time.
 * pc68 is meaningful only for master 0.  The event is delivered after commit. */
typedef struct JagGpuControlEvent {
    uint64_t field, cycles;
    uint32_t vc, addr, value, pc_before, pc_after, pc68;
    uint8_t master, go_before, go_after;
} JagGpuControlEvent;


/* S22: reference-only view access observer. Called for actual bus requests
 * made by the JRISC GPU (master=1) or synchronous Blitter (master=3),
 * only when the requested bus address is in the Jaguar DRAM mirror window.
 * Address is the 24-bit bus request, NOT a guessed resource location.
 * No values from proprietary cartridge data are emitted.
 * NOTE: the event is a request, not a bus-cycle or contention measurement. */
typedef struct JagGpuViewDramAccess {
    uint64_t field;
    uint32_t address, gpu_pc;
    uint8_t master, width, is_write;
} JagGpuViewDramAccess;

/* S23: callback after a real Musashi 68000 DRAM write completes.
 * No data bytes are exported. pc68 is the currently reported Musashi PC,
 * not a proven exact opcode address. Host jagm_write* calls are excluded. */
typedef struct JagCpuDramWriteEvent {
    uint64_t field;
    uint32_t address, pc68;
    uint8_t width;
} JagCpuDramWriteEvent;

typedef struct JagMachine {
    uint8_t ram[JAGM_RAM_SIZE];
    const uint8_t *rom; size_t rom_size;
    JRisc gpu, dsp;
    JagBlitter blit;
    /* TOM */
    uint16_t memcon1,memcon2,vmode,bord1,bord2,hp,hbb,hbe,hs,hvs,hdb1,hdb2,hde;
    uint16_t vp,vbb,vbe,vs,vdb,vde,veb,vee,vi,pit0,pit1,heq,bg;
    uint32_t olp; uint16_t obf; uint64_t ob; /* current object (OB0-3) */
    uint16_t int1_enable,int1_pending;
    uint16_t clut[256];
    uint16_t lbuf[2][720];   /* physical line buffers (16-bit words) */
    unsigned lbuf_write;     /* bank the OP writes */
    uint32_t vc,hc_half;     /* half-line counter / field */
    uint64_t pit_acc;
    /* JERRY */
    uint16_t jpit[4]; uint64_t jpit_acc[2];
    uint16_t jint_enable,jint_pending;
    uint16_t joy_out,joy_select; uint32_t pad_state[2]; /* active-low inputs */
    uint16_t sclk,smode; uint64_t i2s_acc;
    int16_t dac_l,dac_r; uint16_t ltxd,rtxd;
    int16_t *audio; size_t audio_n,audio_cap; int capture_audio;
    uint16_t asictrl,asiclk;
    /* 16-bit half write latches for 32-bit TOM/JERRY internal registers */
    uint32_t hold_addr,hold_val; int hold_valid;
    /* clocks */
    uint64_t sys_cycles; double cpu_cycle_debt;
    uint64_t frames;
    /* output frame (most recent completed field) */
    uint32_t frame[JAGM_MAX_W*JAGM_MAX_H];
    unsigned frame_w,frame_h;
    uint32_t work[JAGM_MAX_W*JAGM_MAX_H];
    /* diagnostics */
    JagUnhandled unhandled[256]; unsigned unhandled_n;
    uint64_t op_lines, op_objects, op_gpu_objects, op_unsupported;
    uint32_t op_unsupported_mask;
    /* JDEV0001: counts OP scanlines with busy 16/24bpp objects and
       those whose authored RELEASE bit permits bus relinquishment.
       These are exposure metrics, NOT stalled DSP clocks or sound errors. */
    uint64_t op_deep_bitmap_visits, op_deep_release_visits;
    /* JDEV0002 OP item 1: consecutive visible RMW and bitmap objects.
       No bogus edge pixel is injected; this is a risk report only. */
    uint64_t op_rmw_neighbor_visits;
    uint32_t last_olp_frame;     /* OLP observed at frame start */
    int halted; char halt_reason[160];
    uint32_t gpu_break_pc; int gpu_break_armed; uint64_t break_min; /* stop when the GPU reaches this PC */
    /* optional GPU-control observer (never modifies register writes) */
    void (*on_gpu_control)(struct JagMachine*, const JagGpuControlEvent*, void*);
    void *on_gpu_control_ctx;
    /* Read-only GPU/Blitter DRAM access observer; off by default. */
    void (*on_gpu_view_dram_access)(struct JagMachine*,const JagGpuViewDramAccess*,void*);
    void *on_gpu_view_dram_access_ctx;
    /* S23: optional write provenance from the 68000 only (not JRISC,
     * Blitter, DSP, OP, or calls made directly by host tests). */
    void (*on_cpu_dram_write)(struct JagMachine*,const JagCpuDramWriteEvent*,void*);
    void *on_cpu_dram_write_ctx;
    /* optional per-frame callback */
    void (*on_frame)(struct JagMachine*,void*); void *on_frame_ctx;
} JagMachine;

/* Returns 0 on success.  `rom` must remain valid. */
int jagm_init(JagMachine *m,const uint8_t *rom,size_t rom_size);
/* Run until `frames` more video fields complete or the machine halts. */
void jagm_run_frames(JagMachine *m,unsigned frames);
uint8_t jagm_read8(JagMachine *m,uint32_t a);
uint16_t jagm_read16(JagMachine *m,uint32_t a);
uint32_t jagm_read32(JagMachine *m,uint32_t a);
void jagm_write8(JagMachine *m,uint32_t a,uint8_t v);
void jagm_write16(JagMachine *m,uint32_t a,uint16_t v);
void jagm_write32(JagMachine *m,uint32_t a,uint32_t v);
/* Joypad: Jaguar button bitmask in AVP pad convention (bit set = pressed). */
void jagm_set_pad(JagMachine *m,unsigned port,uint32_t pressed_mask);
int jagm_write_ppm(const JagMachine *m,const char *path);
/* Wave-ROM image (2 KiB, user supplied) for DSP F1D000.  Optional. */
void jagm_set_wave_rom(JagMachine *m,const uint8_t *rom2k);
/* Run until the GPU is about to execute `pc` (after `min_fields` fields).
 * Returns 1 when stopped there, 0 if `max_fields` elapsed. */
int jagm_run_until_gpu_pc(JagMachine *m,uint32_t pc,uint64_t min_fields,uint64_t max_fields);

#ifdef __cplusplus
}
#endif
#endif
