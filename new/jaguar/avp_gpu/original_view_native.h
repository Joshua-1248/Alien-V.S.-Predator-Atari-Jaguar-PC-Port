#ifndef AVP_JAGUAR_ORIGINAL_VIEW_NATIVE_H
#define AVP_JAGUAR_ORIGINAL_VIEW_NATIVE_H
/* RE24 S19: host-independent native C runner for original Jaguar GPU stages.
 * Generated stage bodies are supplied by the user's own ROM, at build time;
 * this library carries no ROM instructions. The callbacks must implement the
 * Jaguar memory map and the TRM Blitter, NOT host virtual addresses. The
 * existing native PC renderer remains the default until parity is proven. */
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_GPU_NATIVE_MAX_PARAMS 256u
#define AVP_GPU_NATIVE_OVERLAY_BASE 0xF03220u
#define AVP_GPU_NATIVE_RESIDENT_BASE 0xF03000u
#define AVP_GPU_NATIVE_RESIDENT_SIZE 0x220u
#define AVP_GPU_NATIVE_DRAM_SIZE 0x200000u

typedef struct AvpGpuRecompCtx {
    uint32_t r[2][32]; unsigned bank; int z,c,n,divoff;
    uint32_t remain; int64_t acc; uint32_t fault;
    volatile int stop; uint32_t param[AVP_GPU_NATIVE_MAX_PARAMS];
    void *ctx;
    uint32_t (*rd)(void*,uint32_t,int);
    void (*wr)(void*,uint32_t,uint32_t,int);
} AvpGpuRecompCtx;

typedef uint32_t (*AvpGpuNativeFunction)(AvpGpuRecompCtx*,uint32_t);
typedef struct AvpGpuNativeStage {
    uint32_t entry,table,src,longs;
    uint64_t image_hash;  /* FNV-1a of overlay, zeroing verified MOVEI operands */
    AvpGpuNativeFunction fn;
} AvpGpuNativeStage;
typedef struct AvpGpuNativeParam {
    uint32_t stage,gpu_pc,variable; /* in generated-code g->param[] index order */
} AvpGpuNativeParam;
typedef struct AvpGpuNativePipeline {
    const AvpGpuNativeStage *stages; size_t stage_count;
    const AvpGpuNativeParam *params; size_t param_count;
    uint64_t resident_hash; /* byte-exact unchanged sequencer; nonzero required */
    uint32_t entry,halt;
} AvpGpuNativePipeline;
typedef enum AvpGpuNativeResult {
    AVP_GPU_NATIVE_OK=0,
    AVP_GPU_NATIVE_BAD_CONFIG,
    AVP_GPU_NATIVE_IMAGE_MISMATCH,
    AVP_GPU_NATIVE_PATCH_MISMATCH,
    AVP_GPU_NATIVE_BAD_CONTROL_FLOW,
    AVP_GPU_NATIVE_EXECUTION_FAULT
} AvpGpuNativeResult;

/* All stage code and data are validated before any generated function runs.
 * Recheck current stage immediately before invocation; a dynamic mismatch
 * aborts instead of executing stale C. On failure after execution has begun,
 * state may already have been written: callers must run on a disposable frame.
 * Not a real-time or cycle-exact GPU scheduler. */
AvpGpuNativeResult avp_gpu_native_run(const AvpGpuNativePipeline *p,
                                      AvpGpuRecompCtx *g,
                                      uint32_t *end_pc,
                                      unsigned *stage_calls);
uint64_t avp_gpu_native_hash_bytes(const uint8_t *bytes,size_t len);
#ifdef __cplusplus
}
#endif
#endif
