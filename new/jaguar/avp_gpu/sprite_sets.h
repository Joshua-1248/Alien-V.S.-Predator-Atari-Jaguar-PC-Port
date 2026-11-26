/* RE25 S48 (Claude): native port of the original 68000 sprite-set loader.
 *   $BCD8  allocate-and-load: FILES D0 to the allocation pointer [$2EF4C],
 *          store the address in (A2) when A2 != 0, advance [$2EF4C] by the
 *          loaded size rounded up to 8 (addq.l #7 / and.b #$F8)
 *   $DB60  one 20-byte set entry: {+0 file, +2 second file (<0 none),
 *          +4 byte offset into the pointer table $2F278}; the second pointer
 *          is cleared when absent
 *   $DB3A  the character's set list [$23330 + word[$2F1DA]], until +0 < 0
 * Not used by live gameplay. */
#ifndef AVP_JAG_SPRITE_SETS_H
#define AVP_JAG_SPRITE_SETS_H
#include "jaguar/avp_gpu/panels_special_load.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JAG_SPRITE_ALLOC_PTR   0x02EF4Cu
#define AVP_JAG_SPRITE_PTR_TABLE   0x02F278u
#define AVP_JAG_SPRITE_CHARACTER   0x02F1DAu
#define AVP_JAG_SPRITE_LIST_TABLE  0x023330u
int avp_jagsprite_alloc_load(const AvpJagFilesJpegDispatch *dispatch,
                             uint32_t file, uint32_t pointer_address);
int avp_jagsprite_set_entry(const AvpJagFilesJpegDispatch *dispatch, uint32_t a6);
int avp_jagsprite_load_sets(const AvpJagFilesJpegDispatch *dispatch);
#ifdef __cplusplus
}
#endif
#endif
