/* Isolated AMP scoring tests only.  AddressSanitizer retains the full
 * function-pointer graph in amp.c; non-scoring callbacks must resolve at
 * link time even though these tests never execute them.  Fail loudly if
 * unexpected code reaches any of these test-only callbacks. */
#include <stdlib.h>
#include "avp_types.h"
#include "collision.h"
#include "maze.h"
#include "player.h"
#include "hud.h"
#include "doors.h"

u16 invisflag;
s16 use_cocoon, num_cocoons, key_lock, game_over;
u8 acs_level;

u16 avp_random(void) { abort(); }
s32 sin_d0(u16 angle) { (void)angle; abort(); }
s32 cos_d0(u16 angle) { (void)angle; abort(); }
int LineOfSight(const AvpXY *from,const AvpXY *to) {
    (void)from; (void)to; abort();
}
void MakeCocoon(u32 x,u32 y) { (void)x; (void)y; abort(); }
