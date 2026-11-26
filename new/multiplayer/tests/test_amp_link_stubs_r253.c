/* RE42 R253: link-only dependencies for instrumented AMP allocator tests.
 * The original allocator, world capture/restore and AMP state are REAL.
 * These functions belong to unrelated combat/UI paths and must never run in
 * this isolated test. Abort if sanitizers unexpectedly expose such a call. */
#include "collision.h"
#include "doors.h"
#include "hud.h"
#include "maze.h"
#include "mazescrn.h"
#include "music.h"
#include "player.h"
#include "weapons.h"
#include <stdio.h>
#include <stdlib.h>

#define UNREACHABLE(name) do { \
    fprintf(stderr,"Unexpected AMP allocator test dependency: %s\n",(name)); \
    abort(); \
} while(0)

/* Link-only original globals, never used by the allocator test. */
u8 acs_level, cur_weps;
u16 invisflag;
s16 game_over, hugkill, key_lock, num_cocoons, player_type, use_cocoon;
s32 score;

u32 DoEffect(unsigned id) {(void)id;UNREACHABLE("DoEffect");}
void HumanPain(void) {UNREACHABLE("HumanPain");}
void KillAlar(void) {UNREACHABLE("KillAlar");}
int LineOfSight(const AvpXY *a,const AvpXY *b)
{(void)a;(void)b;UNREACHABLE("LineOfSight");}
void MakeCocoon(u32 x,u32 y) {(void)x;(void)y;UNREACHABLE("MakeCocoon");}
void StartAla(unsigned id) {(void)id;UNREACHABLE("StartAla");}
void Vector(const AvpXY *a,const AvpXY *b,s32 *vx,s32 *vy)
{(void)a;(void)b;(void)vx;(void)vy;UNREACHABLE("Vector");}
u8 *avp_collision_maze_data(void) {UNREACHABLE("avp_collision_maze_data");}
u16 avp_collision_maze_height(void) {UNREACHABLE("avp_collision_maze_height");}
u16 avp_collision_maze_width(void) {UNREACHABLE("avp_collision_maze_width");}
int avp_doors_cell_is_liftside(s32 x,s32 y)
{(void)x;(void)y;UNREACHABLE("avp_doors_cell_is_liftside");}
u16 avp_random(void) {UNREACHABLE("avp_random");}
s32 cos_d0(u16 a) {(void)a;UNREACHABLE("cos_d0");}
s32 sin_d0(u16 a) {(void)a;UNREACHABLE("sin_d0");}
