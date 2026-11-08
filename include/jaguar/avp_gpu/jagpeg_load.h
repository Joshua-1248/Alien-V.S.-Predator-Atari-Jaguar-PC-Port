/* RE24 Session 2 (Claude): native C port of the retail game's JagPEG
 * picture-load path, as used by the level loader for wall/floor panels.
 *
 * Original structure (68000 FILES code exp_jpeg at $BE48-$BF50):
 *   1. publish the picture geometry to 68000 variables $2EF5E/$2EF60/$2EF62
 *      and the job to the GPU mailbox at $F03F40-$F03F54;
 *   2. run the GPU JagPEG decoder ($F03000) until it posts $12345678;
 *   3. in mode 2 (byte $2EF65 == 2) run the second GPU program ($F03A00,
 *      copied from DRAM $1BD30) that converts the decoded picture from the
 *      scratch buffer at [$2EF54] into the destination, parameters in
 *      DRAM $7F80-$7F8C;
 *   4. return the decoded size in bytes.
 * The two GPU programs are ported separately (avp_jagpeg_gpu_decode /
 * avp_jagpeg_gpu_convert); this file is the 68000 side only.  No ROM data. */
#ifndef AVP_JAGPEG_LOAD_H
#define AVP_JAGPEG_LOAD_H
#include "jaguar/avp_gpu/view3d.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 68000 variables used by exp_jpeg and by the panel compositor ($F322). */
#define AVP_JPG_VAR_WIDTH_PX    0x02EF5Eu  /* word: width in pixels */
#define AVP_JPG_VAR_HEIGHT_PX   0x02EF60u  /* word: height in pixels */
#define AVP_JPG_VAR_BLIT_WIDTH  0x02EF62u  /* word: Blitter width code */
#define AVP_JPG_VAR_MODE        0x02EF65u  /* byte: output mode (2 = converted) */
#define AVP_JPG_VAR_SCRATCH     0x02EF54u  /* long: scratch buffer for mode 2 */
#define AVP_JPG_VAR_CONVERT_ARG 0x02EF48u  /* long: converter parameter */
/* GPU mailbox (GPU local RAM in the original) */
#define AVP_JPG_MBOX_RECORD  0xF03F40u
#define AVP_JPG_MBOX_DEST    0xF03F44u
#define AVP_JPG_MBOX_WBLOCKS 0xF03F4Cu
#define AVP_JPG_MBOX_WCODE   0xF03F50u
#define AVP_JPG_MBOX_DONE    0xF03F54u
#define AVP_JPG_DONE_MAGIC   0x12345678u

/* The GPU decoder's job as the 68000 posts it in the mailbox. */
typedef struct AvpJagpegJob {
    uint32_t record;      /* address of the "JPxx" file record */
    uint32_t dest;        /* decode destination (scratch buffer in mode 2) */
    uint32_t width_blocks;
    uint32_t width_code;  /* longword written to $F03F50 */
} AvpJagpegJob;
/* The converter's parameters ($7F80-$7F8C). */
typedef struct AvpJagpegConvert {
    uint32_t pixels,source,dest,arg;
} AvpJagpegConvert;

/* GPU program ports (implemented in jagpeg_gpu.c). */
void avp_jagpeg_gpu_decode(const AvpView3DBus *bus,const AvpJagpegJob *job);
void avp_jagpeg_gpu_convert(const AvpView3DBus *bus,const AvpJagpegConvert *cv);

/* exp_jpeg: decode the JagPEG record at `record` to `dest`.  Returns the
 * decoded size in bytes, exactly as the original returns it in D0.
 * The job / converter parameters are also returned for verification. */
uint32_t avp_jagpeg_exp(const AvpView3DBus *bus,uint32_t record,uint32_t dest,
                        AvpJagpegJob *job_out,AvpJagpegConvert *cv_out);
/* The geometry part of exp_jpeg (before the GPU is started). */
void avp_jagpeg_prepare(const AvpView3DBus *bus,uint32_t record,uint32_t dest,AvpJagpegJob *job);

#ifdef __cplusplus
}
#endif
#endif
