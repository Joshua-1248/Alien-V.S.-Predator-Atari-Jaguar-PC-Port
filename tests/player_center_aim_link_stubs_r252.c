/* Test-only link dependencies retained under ASan/UBSan when the real
 * player.c movement functions become reachable via sanitizer metadata.
 * The TestSpark/center-aim regression does not call these functions; abort
 * immediately if that assumption ever changes. */
#include <stdlib.h>
#include "avp_types.h"
#include "joypad.h"
#include "maze.h"
#include "collision.h"
#include "avp_runtime.h"

volatile u32 joy_cur;
s32 sin_ang,cos_ang;
s16 hugkill;
u8 lift_key;

const AvpRuntimeOps *avp_runtime_ops(void) { abort(); }
u8 SafePos(u8 height,s32 x,s32 y,u16 width) {
    (void)height; (void)x; (void)y; (void)width; abort();
}
int AMPCollisions(s32 x,s32 y) { (void)x; (void)y; abort(); }
