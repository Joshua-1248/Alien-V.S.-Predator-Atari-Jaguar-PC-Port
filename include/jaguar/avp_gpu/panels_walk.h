/* RE25 S48 (Claude): native port of the original 68000 panel walker
 * $F1F6-$F2D4 (with the special-panel path $F2D6 via panels_special_load).
 *
 * Input A5 points at a longword holding the panel-list address.  The list:
 *   word  initial slot index ($2F5B6), negative = keep the current one
 *   then entries until a negative longword:
 *     long  address of a 68000 texture variable (receives the slot address)
 *     long  notice pointer (0 = none; else receives [$2F5BA])
 *     bytes panel IDs ($80+ = special panel), $FF terminated, word aligned
 * Each ID takes the next 32 KiB slot from $0DD000; IDs equal to the cached
 * byte at $309BE+slot are not reloaded (except $80).  With a notice pointer,
 * each ID also produces a 256-byte profile table at [$2F5BA]: loaded from the
 * FILES record paired with the ID in the word table at $23FAC, else zeroed
 * (at the address in A1, as the original does); [$2F5BA] then advances $100.
 * Returns the advanced A5 (input + 4), or 0 on any unsupported state. */
#ifndef AVP_JAG_PANEL_WALK_H
#define AVP_JAG_PANEL_WALK_H
#include "jaguar/avp_gpu/panels_special_load.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JAG_PANEL_SLOT_BASE     0x0DD000u
#define AVP_JAG_PANEL_CACHE         0x0309BEu
#define AVP_JAG_PANEL_SLOT_INDEX    0x02F5B6u
#define AVP_JAG_PANEL_PROFILE_PTR   0x02F5BAu
#define AVP_JAG_PANEL_PROFILE_TABLE 0x023FACu
uint32_t avp_jagpanel_walk(const AvpJagFilesJpegDispatch *dispatch, uint32_t a5);
#ifdef __cplusplus
}
#endif
#endif
