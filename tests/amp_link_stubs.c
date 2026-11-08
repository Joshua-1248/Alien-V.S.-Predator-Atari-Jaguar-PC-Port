#include "avp_types.h"
#include "collision.h"
u8 cur_weps;
s16 hugkill;
#ifndef AVP_TEST_REAL_COLLISION
void Vector(const AvpXY *from,const AvpXY *to,s32 *vx,s32 *vy)
{
    *vx=to->x-from->x;*vy=to->y-from->y;
}

void HumanPain(void){}

void AreaAmpDamage(s32 xpos,s32 ypos,s32 radius,s16 damage,u16 damage_flags)
{(void)xpos;(void)ypos;(void)radius;(void)damage;(void)damage_flags;}
void AreaDamage(s32 xpos,s32 ypos,s32 radius,s16 damage,u16 damage_flags)
{
    (void)xpos;(void)ypos;(void)radius;(void)damage;(void)damage_flags;
}

u8 SafePos(u8 height,s32 xpos,s32 ypos,u16 width)
{
    (void)height;(void)xpos;(void)ypos;(void)width;
    return AVP_COLL_SAFE;
}
#endif
int avp_doors_cell_is_liftside(s32 xpos,s32 ypos){(void)xpos;(void)ypos;return 0;}

/* AMP-only test executables do not link the full AVPSOUND module. */
unsigned amp_test_alarm_starts,amp_test_alarm_kills,amp_test_alarm_sound;
void StartAla(unsigned id){++amp_test_alarm_starts;amp_test_alarm_sound=id;}
void KillAlar(void){++amp_test_alarm_kills;}

/* Isolated AMP regressions supply the two retail effect-handle primitives.
 * Full game builds use music.c instead. */
unsigned amp_test_effect_starts,amp_test_effect_stops;
unsigned amp_test_last_effect;
u32 amp_test_last_released;
static u32 amp_test_next_effect=200;
u32 DoEffect(unsigned id)
{
    amp_test_last_effect=id;
    ++amp_test_effect_starts;
    return ++amp_test_next_effect;
}
void StopLoop(u32 handle)
{
    amp_test_last_released=handle;
    ++amp_test_effect_stops;
}

/* Test-only open-door predicate; production reads the real maze panels. */
int avp_test_open_door;
#ifndef AVP_TEST_REAL_COLLISION
int avp_collision_open_door_ahead(s32 x,s32 y,int dx,int dy,u8 height)
{
    (void)y;(void)dy;(void)height;
    return avp_test_open_door && dx>0 && ((u32)x>>16)==1u;
}

/* Runtime placement-grid fixture: walkable floors may be marked $ffff for
 * random-spawn exclusion, but that sentinel is not a real AMP occupant. */
static u8 amp_grid_fixture[8u*8u*8u];
static int amp_grid_fixture_bound;
void avp_test_bind_grid_fixture(int enable)
{
    amp_grid_fixture_bound=enable;
    for(unsigned i=0;i<sizeof(amp_grid_fixture);++i)amp_grid_fixture[i]=0;
    if(enable)for(unsigned y=0;y<8u;++y)for(unsigned x=0;x<8u;++x)
        amp_grid_fixture[(y*8u+x)*8u+5u]=1u;
}
void avp_test_floorless_cell(unsigned x,unsigned y)
{
    if(x<8u&&y<8u)amp_grid_fixture[(y*8u+x)*8u+5u]=0;
}
u8 *avp_collision_maze_data(void)
{
    return amp_grid_fixture_bound?amp_grid_fixture:NULL;
}
u16 avp_collision_maze_width(void){return amp_grid_fixture_bound?8u:0u;}
u16 avp_collision_maze_height(void){return amp_grid_fixture_bound?8u:0u;}

#endif /* AVP_TEST_REAL_COLLISION */
