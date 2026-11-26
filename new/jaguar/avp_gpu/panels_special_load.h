/* RE25 S48 (Claude): native port of the original special-panel transaction
 * $F2D6-$F320 with its overlay routine $F322-$F3D8:
 *   1. descriptor = $23FE2 + (id & $7F) * 4  -> main FILES id + 3-byte overlay
 *   2. $BD44 load main FILES (JP80) into the 32 KiB panel slot
 *   3. $F322 overlay: FILES id (0 = none) loaded to $1FA000, then the
 *      original $F34E-$F3D8 Blitter composite at the signed x/y bytes
 *   4. id $80 only: second overlay from $24016 + 3*(word[$2F5AA]-1)
 * GZIP records (e.g. the live $80 secondary) go through dispatch->gzip.
 * Composes the separately verified native modules (lookup, FILES dispatch,
 * JagPEG decode/convert, overlay Blitter tail).  Not used by live gameplay. */
#ifndef AVP_JAG_PANEL_SPECIAL_LOAD_H
#define AVP_JAG_PANEL_SPECIAL_LOAD_H
#include "jaguar/avp_gpu/files_jpeg_dispatch.h"
#ifdef __cplusplus
extern "C" {
#endif
/* $BD44: load FILES `index` (JPxx, GZIP or NONE record) to `dest`. 1 = ok. */
int avp_jagpanel_files_load(const AvpJagFilesJpegDispatch *dispatch,
                            uint32_t index, uint32_t dest);
/* As above, also returning the size the original returns in D0. */
int avp_jagpanel_files_load_sized(const AvpJagFilesJpegDispatch *dispatch,
                                  uint32_t index, uint32_t dest, uint32_t *size);
/* Diagnostics only: last stage reached and FILES tag/plan result. */
extern int avp_jagpanel_special_stage,avp_jagpanel_special_detail;
/* Returns 1 when every original step completed; 0 on any unsupported state. */
int avp_jagpanel_special_load(const AvpJagFilesJpegDispatch *dispatch,
                              uint32_t special_id, uint32_t destination,
                              unsigned secondary_limit);
#ifdef __cplusplus
}
#endif
#endif
