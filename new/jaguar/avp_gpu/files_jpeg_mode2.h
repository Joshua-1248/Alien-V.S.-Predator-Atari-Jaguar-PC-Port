/* RE25-S34: isolated original FILES #239 dependency for JPEG mode 2.
 * This is a bounded native composition of the separately recovered FILES
 * dispatcher and GZIP service-vector boundary.  Exact $BE30 68000 parity
 * remains unverified until the user's retail cartridge is available. */
#ifndef AVP_JAGFILES_JPEG_MODE2_H
#define AVP_JAGFILES_JPEG_MODE2_H
#include "jaguar/avp_gpu/files_jpeg_init.h"
#include "jaguar/avp_gpu/files_gzip_vectors.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JPEG_CRY_TABLE_POINTER 0x02EF48u
#define AVP_JPEG_CRY_TABLE_FILE_ID 239u
/* This expected size is a host-side guard grounded in the existing PC
 * resource loader's 0x20000-byte CRY conversion table requirement. */
#define AVP_JPEG_CRY_TABLE_BYTES 0x20000u

/* Prepare the missing mode-2 CRY table through authentic FILES index 239,
 * its GZIP codec dispatch, and original cartridge service callbacks.
 * Already-loaded mode 2 succeeds without touching the bus.  A missing
 * cartridge service, malformed record, unsafe target, short decompression,
 * or other failure leaves the 'loaded' flag unset. Note: successful GZIP
 * initialization may change the shared previous-codec cache and must NOT
 * be undone here. The outer JPEG initializer/dispatcher handles its own
 * subsequent cached/uncached decision. This does not enable live loading.
 * Failure is not transactional: original-style service side effects remain. */
int avp_jagfiles_jpeg_mode2_load_cry(const AvpView3DBus *bus,
    const AvpJagFilesConfig *cfg, const AvpJagFilesGzipDispatch *gzip);
#ifdef __cplusplus
}
#endif
#endif
