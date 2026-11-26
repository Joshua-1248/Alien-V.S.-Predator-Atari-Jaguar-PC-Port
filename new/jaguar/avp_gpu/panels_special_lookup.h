/* Sweep 42: isolated retail $F2D6 special-panel descriptor resolver.
 * This reads original Jaguar DRAM; it does NOT load JP80, schedule a GPU,
 * or issue the $F322 overlay Blitter transaction. No live PC call path. */
#ifndef AVP_JAG_PANEL_SPECIAL_LOOKUP_H
#define AVP_JAG_PANEL_SPECIAL_LOOKUP_H
#include "jaguar/avp_gpu/view3d.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JAG_PANEL_SPECIAL_TABLE 0x023FE2u
#define AVP_JAG_PANEL_SPECIAL_SECONDARY_TABLE 0x024016u
#define AVP_JAG_PANEL_SPECIAL_INDEX 0x02F5AAu
/* Each primary record is one FILES index byte and three bytes of an overlay
 * descriptor. Only validated table geometry is accepted. A 0x80 ID has a
 * separate three-byte secondary descriptor indexed by original DRAM word.
 * The original 68000 does not itself enforce this host-side bound. */
typedef struct AvpJagPanelSpecialLookup {
    uint32_t file_index;
    uint32_t primary_descriptor_address;
    uint8_t primary_descriptor[3];
    uint32_t secondary_descriptor_address; /* 0 when absent */
    uint8_t secondary_descriptor[3];
} AvpJagPanelSpecialLookup;
/* Returns 1 only on the bounded, authenticated retail table geometry.
 * No mutation, no simulated decode, and no fallback for unsupported IDs.
 * Valid primary IDs: $80-$8C; $80 secondary count is caller-provided.
 * For $80, require 1<=number_of_secondary_descriptors<=limit. */
int avp_jagpanel_special_lookup(const AvpView3DBus *bus, uint32_t special_id,
                                unsigned secondary_limit,
                                AvpJagPanelSpecialLookup *out);
#ifdef __cplusplus
}
#endif
#endif
