/* RE25-S35: isolated composition of native Jaguar JPEG mode-2 preparation.
 * This is NOT the live PC loader or an original-ROM-parity claim. */
#ifndef AVP_JAGFILES_JPEG_MODE2_SETUP_H
#define AVP_JAGFILES_JPEG_MODE2_SETUP_H
#include "jaguar/avp_gpu/files_jpeg_mode2.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete the native $BD92 mode-2 dependency sequence using already
 * reconstructed, separately tested stages:
 *  - load the FILES #239 CRY table if absent (GZIP cartridge callbacks);
 *  - select cached-JP or first-time GPU installation according to the
 *    CURRENT previous-codec tag, after any nested FILES operation;
 *  - select and install the original quality table.
 *
 * The shared previous-tag cache is left as the nested FILES handler set
 * it; the outer FILES dispatcher must commit the JPxx tag on successful
 * initialization. Missing services, unverified metadata and invalid inputs
 * fail closed. GPU copying needs an actual Jaguar Blitter-capable bus.
 * This does not implement the cartridge inflater or prove $BE30 parity. */
int avp_jagfiles_jpeg_mode2_prepare_native(const AvpView3DBus *bus,
    const AvpJagFilesConfig *cfg, const AvpJagFilesGzipDispatch *gzip,
    uint32_t jp_tag);
#ifdef __cplusplus
}
#endif
#endif
