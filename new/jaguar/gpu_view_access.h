#ifndef AVP_JAGUAR_GPU_VIEW_ACCESS_H
#define AVP_JAGUAR_GPU_VIEW_ACCESS_H
/* S22: read-only, view-scoped reference-machine DRAM access aggregation.
 * NOT a resource locator by itself; no guesses about level addresses.
 * All output is counters and addresses, never ROM or RAM content. */
#include "jaguar/jag_machine.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
#define JAG_GPU_VIEW_PAGE_SHIFT 8u
#define JAG_GPU_VIEW_PAGE_BYTES (1u << JAG_GPU_VIEW_PAGE_SHIFT)
#define JAG_GPU_VIEW_PAGE_COUNT (JAGM_RAM_SIZE / JAG_GPU_VIEW_PAGE_BYTES)
typedef struct JagGpuViewPage {
    uint64_t gpu_reads, gpu_writes, blitter_reads, blitter_writes;
    uint32_t first_pc, last_pc;
    uint8_t visited;
} JagGpuViewPage;
/* Provenance is physical-page-level, not individual-byte attribution.
 * This is an observed 68000 write before the view, not proof that the CPU
 * created every byte later consumed by the GPU/Blitter. */
typedef struct JagCpuWriterPage {
    uint64_t writes, first_field, last_field;
    uint32_t first_pc68, last_pc68;
    uint8_t seen;
} JagCpuWriterPage;
typedef struct JagGpuViewMap {
    JagGpuViewPage page[JAG_GPU_VIEW_PAGE_COUNT];
    JagCpuWriterPage writer[JAG_GPU_VIEW_PAGE_COUNT];
    uint64_t cpu_requests, cpu_bad_requests;
    uint64_t gpu_requests, blitter_requests;
    uint64_t wrap_requests, bad_requests, counter_overflows;
} JagGpuViewMap;

/* Caller installs this callback on JagMachine only while a GPU view is
 * executing. Every event must come from the reference machine's bus hooks. */
void jag_gpu_view_access_observe(JagMachine *machine,
    const JagGpuViewDramAccess *event, void *user);
/* Sorted sparse CSV, only touched pages. Zero content or data values. */
int jag_gpu_view_access_csv(const JagGpuViewMap *map, FILE *out);
/* Install from jagm_init() onward; detach at view entry so the CPU writer
 * window is exactly 68000 writes observed before the sampled GPU view.
 * Only pages that the view subsequently reads appear in the output CSV. */
void jag_gpu_view_cpu_write_observe(JagMachine *machine,
    const JagCpuDramWriteEvent *event, void *user);
int jag_gpu_view_provenance_csv(const JagGpuViewMap *map, FILE *out);
#ifdef __cplusplus
}
#endif
#endif
