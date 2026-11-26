/* RE42 R254: Queen-campaign isolated-test dependency boundary.
 *
 * The test executes the real avp_amp_mp_queen_predator_ending() logic from
 * src/game/amp.c. Under ASan/UBSan, unrelated AMP functions may be retained
 * in that translation unit and require these *link-only* game symbols.
 * They are not an alternative implementation of Queen campaign behavior.
 * Any call to one of these symbols is a test failure, not a silent success.
 */
#include "avp_runtime.h"
#include "collision.h"
#include "maze.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h>

/* Unrelated original player position/state, unused in the verified call path. */
u32 x_pos, y_pos;
s16 player_energy;

static void forbidden(const char *name)
{
    fprintf(stderr, "R254 Queen test called unrelated dependency: %s\n", name);
    abort();
}

const AvpRuntimeOps *avp_runtime_ops(void)
{
    forbidden("avp_runtime_ops");
    return NULL;
}
u8 SafePos(u8 height, s32 xpos, s32 ypos, u16 width)
{
    (void)height; (void)xpos; (void)ypos; (void)width;
    forbidden("SafePos");
    return 0;
}
void AreaAmpDamage(s32 xpos, s32 ypos, s32 radius, s16 damage, u16 flags)
{
    (void)xpos; (void)ypos; (void)radius; (void)damage; (void)flags;
    forbidden("AreaAmpDamage");
}
void AreaDamage(s32 xpos, s32 ypos, s32 radius, s16 damage, u16 flags)
{
    (void)xpos; (void)ypos; (void)radius; (void)damage; (void)flags;
    forbidden("AreaDamage");
}
