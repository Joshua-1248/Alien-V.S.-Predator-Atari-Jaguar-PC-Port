/* RE25-S33: original AVP FILES GZIP cartridge-vector handoff.
 *
 * This models the 68000 mailbox/vector boundary ($BF52 and $BF6E), NOT
 * the cartridge's separate inflater, allocator, or a host zlib substitute.
 * Original-ROM parity of these wrappers is still required before enabling
 * them in the live resource loader. */
#ifndef AVP_JAGFILES_GZIP_VECTORS_H
#define AVP_JAGFILES_GZIP_VECTORS_H

#include "jaguar/avp_gpu/files_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AVP_JAGFILES_GZIP_WORK_VARIABLE 0x02EF58u
#define AVP_JAGFILES_GZIP_WORK_MAILBOX  0x007FE8u
#define AVP_JAGFILES_GZIP_INPUT        0x007FF4u
#define AVP_JAGFILES_GZIP_OUTPUT       0x007FF8u
#define AVP_JAGFILES_GZIP_RESULT       0x007FFCu
#define AVP_JAGFILES_GZIP_INIT_VECTOR1 0x802004u
#define AVP_JAGFILES_GZIP_INIT_VECTOR2 0x802008u
#define AVP_JAGFILES_GZIP_EXP_VECTOR   0x80200Cu
#define AVP_JAGFILES_GZIP_INIT_PC      0x00BF52u
#define AVP_JAGFILES_GZIP_EXP_PC       0x00BF6Eu

/* Invoke the original Jaguar cartridge service at the specified vector
 * address, in the supplied bus context; 0 means the service was unavailable
 * or could not be performed. The callback is responsible for vector
 * resolution, original hardware side effects and decompression. */
typedef int (*AvpJagFilesGzipVectorInvoke)(void *ctx, uint32_t vector_address);

/* Original $BF52: write the scratch/work pointer, then invoke BOTH
 * cartridge initialization services in order. Not transactional: if an
 * external service fails, earlier original-style side effects remain. */
int avp_jagfiles_gzip_init_vectors(const AvpView3DBus *bus,
                                   AvpJagFilesGzipVectorInvoke invoke,
                                   void *invoke_ctx);

/* Original $BF6E: post the compressed payload and output pointers, invoke
 * the cartridge expansion service, then read the reported output size.
 * The supplied output address MUST refer to a destination that the caller
 * has made safe for the unknown decompressed length. No host inflater and
 * no unverified destination-capacity inference are performed here. */
int avp_jagfiles_gzip_expand_vector(const AvpView3DBus *bus,
                                    uint32_t payload_address,
                                    uint32_t output_address,
                                    AvpJagFilesGzipVectorInvoke invoke,
                                    void *invoke_ctx,
                                    uint32_t *reported_size);


/* Optional adapter for avp_jagfiles_execute(). Only the two verified GZIP
 * 68000 dispatcher PCs are recognized; an unsupported codec must fail.
 * The native FILES plan supplies the payload address to the expand call. */
typedef struct AvpJagFilesGzipDispatch {
    const AvpView3DBus *bus;
    AvpJagFilesGzipVectorInvoke invoke;
    void *invoke_ctx;
    uint32_t *reported_size;
} AvpJagFilesGzipDispatch;

int avp_jagfiles_gzip_dispatch_invoke(void *context,uint32_t routine_pc,
                                      uint32_t record_addr,uint32_t dest_addr);

#ifdef __cplusplus
}
#endif
#endif
