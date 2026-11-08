/* RE25-S36: isolated original FILES JPEG codec adapter.
 * Routes authentic $BD92/$BE48 FILES handler addresses to the independently
 * reconstructed JPEG initializer and a caller-supplied original JagPEG
 * expansion engine. Never enabled by the live PC loader. */
#ifndef AVP_JAGFILES_JPEG_DISPATCH_H
#define AVP_JAGFILES_JPEG_DISPATCH_H
#include "jaguar/avp_gpu/files_jpeg_mode2_setup.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JAGFILES_JPEG_INIT_PC 0x0000BD92u
#define AVP_JAGFILES_JPEG_EXP_PC  0x0000BE48u

typedef int (*AvpJagFilesJpegExpandInvoke)(void *ctx,
    uint32_t record_payload, uint32_t output_address);
typedef struct AvpJagFilesJpegDispatch {
    const AvpView3DBus *bus;
    const AvpJagFilesConfig *cfg;
    const AvpJagFilesGzipDispatch *gzip; /* mode 2 only, optional if table cached */
    AvpJagFilesJpegExpandInvoke expand;
    void *expand_ctx;
} AvpJagFilesJpegDispatch;

/* This is the bridge for the existing avp_jagfiles_execute() sequence.
 * The expansion callback MUST perform the authentic JPEG expansion and
 * signal failure when it cannot; this adapter does not fake decoded pixels.
 * The outer FILES executor owns the last-codec cache update. */
int avp_jagfiles_jpeg_dispatch_invoke(void *ctx, uint32_t routine_pc,
    uint32_t record_address, uint32_t destination_address);

/* Validate the complete JPEG handler, metadata and expansion provider before
 * the outer executor may change any state. Nested decompression is still
 * nontransactional, exactly as in S35. */
int avp_jagfiles_jpeg_execute(const AvpJagFilesJpegDispatch *dispatch,
    const AvpJagFilesPlan *plan, uint32_t destination_address);
#ifdef __cplusplus
}
#endif
#endif
