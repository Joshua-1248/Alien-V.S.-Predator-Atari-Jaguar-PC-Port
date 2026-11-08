/* C translation of JOYED_exact_sourcefit.s. */
#include "joyed.h"
#include "joypad.h"
#include <stdint.h>

#define EDIT_OBJECT_SPECIAL ((const void *)(uintptr_t)0x000203F8u)
void MoveData(JoyEditRecord *item,const void *edited_object) {
    unsigned control_off=(edited_object==EDIT_OBJECT_SPECIAL)?19u:13u;
    u32 step=(u16)item->magnitude;
    unsigned shift=(unsigned)((u8)(6u-((const u8*)item)[control_off])) & 31u;
    step=(step>>shift)<<3;
    /* The original 68000 ADD/SUB operations on the editor's longword
     * wrap modulo 2^32.  Keep that register behavior without signed-C
     * overflow at the extremes of the editable value. */
    u32 v=(u32)item->value; u32 edge=joy_edge;
    if ((edge&(1u<<20))==0u) v-=step;
    if ((edge&(1u<<21))==0u) v+=step;
    if ((edge&(1u<<22))==0u) --v;
    if ((edge&(1u<<23))==0u) ++v;
    item->value=v<=INT32_MAX?(s32)v:(s32)((int64_t)v-INT64_C(4294967296));
}
void MoveXYFromJoy(JoyEditRecord *item) {
    u32 cur=joy_cur; s16 x=item->x,y=item->y;
    if ((cur&(1u<<20))==0u) --y;
    if ((cur&(1u<<21))==0u) ++y;
    if ((cur&(1u<<22))==0u) --x;
    if ((cur&(1u<<23))==0u) ++x;
    item->x=x; item->y=y;
}
