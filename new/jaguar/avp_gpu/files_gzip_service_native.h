/* RE25 S48 (Claude): native stand-in for the cartridge GZIP services behind
 * the FILES vectors ($802004/$802008 init, $80200C expand), for use with
 * avp_jagfiles_gzip_dispatch_invoke (files_gzip_vectors.h).
 *
 * OUTPUT-LEVEL equivalent only: expand reads the gzip member at [$7FF4]
 * (length from the FILES record header at payload-4), inflates it with an
 * RFC 1951 inflater (zlib) and writes the bytes to [$7FF8], reporting the
 * size at $7FFC and leaving [$7FF4]/[$7FF8] advanced past the consumed
 * DEFLATE input (the 8-byte gzip trailer is not consumed) and the written output, as the cartridge does.  It does NOT reproduce the cartridge
 * inflater's own side effects: its heap ($1E0000-$1EA000), its 68000 stack
 * frames, or its GPU helper program in GPU local RAM.  The init vectors are
 * accepted without effect for the same reason.  Verified against the
 * original $BF6E path by tools/jagref/panels_special_load_rom_check.c. */
#ifndef AVP_JAGFILES_GZIP_SERVICE_NATIVE_H
#define AVP_JAGFILES_GZIP_SERVICE_NATIVE_H
#include "jaguar/avp_gpu/files_gzip_vectors.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AvpJagFilesGzipServiceNative {
    const AvpView3DBus *bus;
    uint32_t archive_begin, archive_end;   /* compressed input bounds */
    uint32_t output_begin, output_end;     /* writable output bounds (exclusive) */
    uint32_t last_size;
    unsigned expands;
} AvpJagFilesGzipServiceNative;
/* AvpJagFilesGzipVectorInvoke */
int avp_jagfiles_gzip_service_native(void *ctx, uint32_t vector_address);
#ifdef __cplusplus
}
#endif
#endif
