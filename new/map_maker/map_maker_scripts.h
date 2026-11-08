#ifndef AVP_MAP_MAKER_SCRIPTS_H
#define AVP_MAP_MAKER_SCRIPTS_H
/* PC Map Maker-only snapshot of original LEVDOOR.S scripted offsets.
 * Editable copies are NEVER installed into the Jaguar or live PC game. */
#include "avp_types.h"

enum {
    AVP_MM_SCRIPT_ACCESS=1u,
    AVP_MM_SCRIPT_EXIT=2u,
    AVP_MM_SCRIPT_INTERLOCK=4u,
    AVP_MM_SCRIPT_REQUIRED=8u
};
#define AVP_MM_MAX_SCRIPTS 144u
typedef struct AvpMmScriptRow {
    u8 level,kind;
    u16 offset;
    s16 arg;
    u16 destination_x,destination_y,destination_level;
} AvpMmScriptRow;
typedef struct AvpMmScriptInfo {
    unsigned flags;
    s16 access_code;
    unsigned destination_x,destination_y,destination_level;
    unsigned interlock_required_by;
} AvpMmScriptInfo;
/* Byte offsets are into the 64*64*8 maze cell array, NOT the +8 header. */
int avp_mm_script_at(unsigned level,unsigned offset,AvpMmScriptInfo *out);
unsigned avp_mm_script_catalog_size(void);
unsigned avp_mm_script_copy_level(unsigned level,AvpMmScriptRow *out,unsigned cap);
int avp_mm_script_lookup(const AvpMmScriptRow *rows,unsigned count,
                         unsigned level,unsigned offset,AvpMmScriptInfo *out);
/* Permits only offset changes on a same-level copy of original LEVDOOR.S,
 * preserving all access codes, exits, grouped anchors and interlock links. */
int avp_mm_script_validate(const AvpMmScriptRow *rows,unsigned count,unsigned level);
#endif
