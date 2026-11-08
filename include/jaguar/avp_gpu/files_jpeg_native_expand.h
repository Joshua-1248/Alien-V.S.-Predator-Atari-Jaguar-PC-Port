/* RE25 Sweep 37: bounded FILES $BE48 to the original native JagPEG GPU port.
 * Isolated Jaguar bus adapter; NEVER selected by the playable PC loader.
 * Does not contain game content, a fake decompressor, or a JPEG substitute. */
#ifndef AVP_JAGFILES_JPEG_NATIVE_EXPAND_H
#define AVP_JAGFILES_JPEG_NATIVE_EXPAND_H
#include "jaguar/avp_gpu/files_jpeg_dispatch.h"
#include "jaguar/avp_gpu/jagpeg_load.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AvpJagFilesJpegNativeExpand {
    const AvpView3DBus *bus;
    uint32_t archive_begin, archive_end;
    uint32_t output_begin, output_end;  /* allocated destination bytes, exclusive */
    uint32_t scratch_begin, scratch_end; /* mode 2 RGB intermediate allocation */
    uint32_t last_size;                 /* only updated after successful expansion */
    AvpJagpegJob last_job;
    AvpJagpegConvert last_convert;
} AvpJagFilesJpegNativeExpand;

/* For AvpJagFilesJpegDispatch.expand. The original decompressor may read a
 * variable-length entropy stream; record bounds alone cannot prove that the
 * stream terminates inside the archive. Therefore use ONLY a trusted Jaguar
 * bus/retail-ROM image, not an arbitrary input or the live host loader.
 * All unsupported/malformed geometry is rejected before any bus mutation. */
int avp_jagfiles_jpeg_native_expand(void *ctx, uint32_t payload,
                                    uint32_t destination);
#ifdef __cplusplus
}
#endif
#endif
