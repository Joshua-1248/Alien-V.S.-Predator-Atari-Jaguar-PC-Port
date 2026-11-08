/* PC host-only opt-in game micro-optimizations.
 * First target: the reconstructed original COLLIDE.S SafePos.
 * GNU ld --wrap keeps the original, always callable as __real_SafePos.
 * No gameplay or src/game code is modified.
 */
#include "game_micro_optimization.h"
#ifdef AVP_GAME_MICRO_OPT_WRAP
#include "collision.h"
#include "amp.h"
#if defined(AVP_LOCAL_MULTIPLAYER)
#include "local_mp_amp_native.h"
#include "amp_free_list.h"
#endif

static int enabled;
void avp_game_micro_opt_set(int value)
{
    enabled = !!value;
#if defined(AVP_LOCAL_MULTIPLAYER)
    avp_mp_amp_native_set_free_list_validator(
        enabled ? avp_game_micro_amp_free_list_valid : NULL);
    avp_mp_amp_native_set_direct_capture(enabled);
#endif
}
int avp_game_micro_opt_get(void) { return enabled; }
int avp_game_micro_opt_available(void) { return 1; }

/* Linker rewrites external references to SafePos; the original object is
 * preserved as-is and is called for every case not proven equivalent. */
u8 __real_SafePos(u8 height,s32 xpos,s32 ypos,u16 width);
int __real_LineOfSight(const AvpXY *from,const AvpXY *to);
int __real_AMPCollisions(s32 xpos,s32 ypos);
/* Native AMP collision scans a 3-by-3 grid around the queried world cell.
 * For an interior cell with nine empty owner IDs, its original collision
 * result is provably zero: there is no actor to inspect.  Never cache the
 * grid: AMP movement, deaths and level changes can update it every tick.
 * Retain the original routine for any occupied or boundary neighborhood. */
int __wrap_AMPCollisions(s32 xpos,s32 ypos)
{
    if (enabled) {
        const u32 x=(u32)xpos>>16, y=(u32)ypos>>16;
        if (x>0u && x+1u<AVP_AMP_GRID_W &&
            y>0u && y+1u<AVP_AMP_GRID_H) {
            const u16 *row=&collmap[(y-1u)*AVP_AMP_GRID_W+(x-1u)];
            const u16 occupied=(u16)(row[0]|row[1]|row[2]|
                row[AVP_AMP_GRID_W]|row[AVP_AMP_GRID_W+1u]|row[AVP_AMP_GRID_W+2u]|
                row[2u*AVP_AMP_GRID_W]|row[2u*AVP_AMP_GRID_W+1u]|
                row[2u*AVP_AMP_GRID_W+2u]);
            if (!occupied) return 0;
        }
    }
    return __real_AMPCollisions(xpos,ypos);
}

/* Enemy AI repeatedly asks whether its player target is visible. For two
 * valid positions inside a single map cell, the straight segment cannot
 * cross a cell boundary. The native maze scan consequently reports clear.
 * Do not cache visibility across ticks: doors, moving objects and targets
 * must always be observed at their original cadence. */
int __wrap_LineOfSight(const AvpXY *from,const AvpXY *to)
{
    if (enabled && from && to) {
        const u32 x=(u32)from->x>>16, y=(u32)from->y>>16;
        if (x==((u32)to->x>>16) && y==((u32)to->y>>16) &&
            avp_collision_maze_data() &&
            x<avp_collision_maze_width() && y<avp_collision_maze_height() &&
            (u16)from->x>=0x6000u && (u16)from->x<=0x9fffu &&
            (u16)from->y>=0x6000u && (u16)from->y<=0x9fffu &&
            (u16)to->x>=0x6000u && (u16)to->x<=0x9fffu &&
            (u16)to->y>=0x6000u && (u16)to->y<=0x9fffu)
            return 0;
    }
    return __real_LineOfSight(from,to);
}

u8 __wrap_SafePos(u8 height,s32 xpos,s32 ypos,u16 width)
{
    if (enabled) {
        /* Original COLLIDE.S: all tests check only wall proximity. If both
         * 16.16 fractions lie between the exact original near/far limits,
         * no wall test runs and the result is always AVP_COLL_SAFE.
         * The original inside_cell() guard still must hold. */
        const u16 near = (u16)(width << 9);
        const u16 far = (u16)~near;
        const u16 fx = (u16)xpos, fy = (u16)ypos;
        if (fx >= near && fx <= far && fy >= near && fy <= far) {
            if (avp_collision_maze_data() &&
                ((u32)xpos >> 16) < avp_collision_maze_width() &&
                ((u32)ypos >> 16) < avp_collision_maze_height())
                return AVP_COLL_SAFE;
        }
    }
    return __real_SafePos(height,xpos,ypos,width);
}
#else
/* On unsupported linkers leave the original engine untouched. */
void avp_game_micro_opt_set(int value) { (void)value; }
int avp_game_micro_opt_get(void) { return 0; }
int avp_game_micro_opt_available(void) { return 0; }
#endif
