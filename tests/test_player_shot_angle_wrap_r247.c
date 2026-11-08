/* RE #22 R247.  Test the original 68000 TestSpark d0.w random spread,
 * 32-bit modulo ADD/SUB, and swing multiplication without changing RNG or
 * shot routing.  The Oracle deliberately uses unsigned 32-bit arithmetic. */
#include "player.h"
#include "maze.h"
#include "mazescrn.h"
#include "amp.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

AvpAmp amps[AVP_NUM_AMPS],*amps_at=amps;
u32 x_pos=0x100000u,y_pos=0x100000u,centre_angle=0,fire_angle=0;
s16 swing_x=0;
s16 fire_damage=30;
u32 fire_distance=0x90u<<8;
u16 fire_width=20;
u32 FireDistance(void){return 0;}
s32 cos_d0(u16 a){(void)a;return 0x7fff;}
s32 sin_d0(u16 a){(void)a;return 0;}
void make_spark_at(s32 x,s32 y,int blood){(void)x;(void)y;(void)blood;}
void make_spark_hit_at(s32 x,s32 y,const AvpAmp*a){(void)x;(void)y;(void)a;}

static void check(u32 centre,u16 old_seed1,s16 swing)
{
    const u32 spread=(u32)((int32_t)(int16_t)old_seed1*512);
    const u32 swing_term=(u32)((int64_t)swing*0x400000ll);
    const u32 expected=centre+spread-swing_term;
    centre_angle=centre;
    seed0=0x1234u;
    seed1=old_seed1;
    swing_x=swing;
    wep_fire=0xffu;
    memset(amps,0,sizeof(amps));
    assert(TestSpark()==NULL);
    assert(fire_angle==expected);
    assert(seed0==old_seed1); /* exactly one call to avp_random */
}

int main(void)
{
    static const u32 centres[]={0u,1u,0x7fffffffu,0x80000000u,0xffffffffu,0x1234abcd};
    static const u16 seeds[]={0u,1u,7u,0x7fffu,0x8000u,0xfff9u,0xffffu};
    static const s16 swings[]={0,1,-1,3,-7,32767,-32768};
    for(unsigned a=0;a<sizeof(centres)/sizeof(centres[0]);++a)
        for(unsigned b=0;b<sizeof(seeds)/sizeof(seeds[0]);++b)
            for(unsigned c=0;c<sizeof(swings)/sizeof(swings[0]);++c)
                check(centres[a],seeds[b],swings[c]);
    return 0;
}
