/* RE24-S31: ROM-derived FILES JagPEG quality-table selection ($BE00).
 * This is the 68000 QUANTIZATION TABLE operation, not the complete $BD92
 * initializer (which also installs a GPU program and may load FILES #239).
 * No cartridge bytes or quantization constants are embedded here. */
#ifndef AVP_JAGFILES_JPEG_INIT_H
#define AVP_JAGFILES_JPEG_INIT_H
#include "jaguar/avp_gpu/view3d.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JPEG_QUALITY_VARIABLE 0x02EF5Cu
#define AVP_JPEG_QUALITY_LIST     0x022B68u
#define AVP_JPEG_DEFAULT_TABLE    0x02E518u
#define AVP_JPEG_GPU_QUANT_TABLE  0xF03E00u
#define AVP_JPEG_QUANT_BYTES      0x140u
#define AVP_JPEG_MODE_VARIABLE    0x02EF65u
#define AVP_JPEG_CRY_LOADED       0x02EF64u
#define AVP_JPEG_PREVIOUS_TAG     0x02EF50u

typedef struct AvpJagFilesJpegQuant {
    uint32_t source, target;
    uint16_t quality;
    unsigned matching_entry;
} AvpJagFilesJpegQuant;
/* Reads 68000 memory's 6-byte {quality-word, source-pointer} list.
 * Unknown qualities select the original default table. Does not write. */
int avp_jagfiles_jpeg_quant_plan(const AvpView3DBus *bus, uint16_t quality,
                                AvpJagFilesJpegQuant *result);
/* Revalidates the plan and copies exactly 0x140 bytes to the GPU RAM.
 * The callback bus must implement byte accesses to both DRAM/GPU RAM. */
int avp_jagfiles_jpeg_quant_apply(const AvpView3DBus *bus,
                                 const AvpJagFilesJpegQuant *plan);
/* Exact original $BD92 cached-JP initializer case: when the GPU decoder
 * is already installed and mode 2 does not require nested FILES #239.
 * Rejects, WITHOUT WRITES, cases still requiring the original GPU loader
 * ($8940) or GZIP-backed color table.  The caller's FILES dispatcher updates
 * the previous-tag cache only after this returns successfully. */
int avp_jagfiles_jpeg_init_cached(const AvpView3DBus *bus,uint32_t tag);
/* Original $8940 loader: the source record has {GPU destination, length,
 * program bytes}; $8C28 copies the 4-byte-rounded program using the Blitter
 * through the GPU 32-bit alias, not through host memcpy. */
#define AVP_JPEG_FRONTEND_PROGRAM 0x02CA38u
#define AVP_JPEG_LEVEL_PROGRAM    0x02D3E8u
#define AVP_JPEG_GPU_ENTRY        0xF03000u
#define AVP_JPEG_GPU_ALIAS_DELTA  0x8000u
#define AVP_JPEG_GPU_MAILBOX      0xF03F48u
#define AVP_JPEG_GPU_CTRL         0xF02114u

typedef struct AvpJagFilesJpegInstall {
    uint32_t header, source, gpu_target, alias_target, length, rounded_length;
    unsigned mode;
} AvpJagFilesJpegInstall;

/* Read-only checked plan for the authentic first-time program installation.
 * The GPU loader uses the original $8940/$8C28 Blitter register protocol.
 * No program bytes or quantization values are embedded in native source. */
int avp_jagfiles_jpeg_install_plan(const AvpView3DBus *bus,unsigned mode,
                                   AvpJagFilesJpegInstall *result);
/* Issues the original GPU STOP and Blitter copy register commands.
 * A functional Jaguar Blitter bus is mandatory; this is not host memcpy. */
int avp_jagfiles_jpeg_install_apply(const AvpView3DBus *bus,
                                    const AvpJagFilesJpegInstall *plan);
/* Original $BD92 first-time setup, excluding the nested FILES #239 branch.
 * Valid only when previous codec is not JPxx, and when mode 2's color table
 * is already installed. Invalid/unsupported cases make NO writes. */
int avp_jagfiles_jpeg_init_first(const AvpView3DBus *bus,uint32_t tag);
#ifdef __cplusplus
}
#endif
#endif
