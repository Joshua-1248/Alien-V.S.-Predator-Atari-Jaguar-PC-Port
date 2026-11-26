/* R196: the real TestSpark() must retain Jaguar spread/RNG while optionally
 * suppressing the weapon-sway term. Covers all three campaigns and all
 * independent Enhancements combinations, including unchanged retail default. */
#include "player.h"
#include "maze.h"
#include "mazescrn.h"
#include "amp.h"
#include "weapons.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

AvpAmp amps[AVP_NUM_AMPS],*amps_at=amps;
u32 x_pos=0x100000u,y_pos=0x100000u,centre_angle=0,fire_angle=0;
s16 swing_x=0,cur_wepno=1;
s16 fire_damage=30;
u32 fire_distance=0x90u<<8;
u16 fire_width=20;
u32 FireDistance(void){return 0;}
s32 cos_d0(u16 a){(void)a;return 0x7fff;}
s32 sin_d0(u16 a){(void)a;return 0;}
void make_spark_at(s32 x,s32 y,int blood){(void)x;(void)y;(void)blood;}
void make_spark_hit_at(s32 x,s32 y,const AvpAmp*a){(void)x;(void)y;(void)a;}

static unsigned cases;
static void check(s16 species,s16 weapon,int melee,int all,u16 seed,s16 sway)
{
    const int is_melee=(species==PT_ALIEN && weapon>=1 && weapon<=3) ||
        (species==PT_PREDATOR && (weapon==1 || weapon==4));
    const int centered=all || (melee && is_melee);
    const u32 spread=(u32)((int32_t)(int16_t)seed*512);
    const u32 offset=centered?0u:(u32)((int64_t)sway*0x400000ll);
    const u32 expected=0x23456789u+spread-offset;
    avp_player_set_center_aim_options(melee,all);
    assert(avp_player_center_aim_applies(species,weapon)==centered);
    player_type=species;cur_wepno=weapon;
    centre_angle=0x23456789u;
    seed0=0x1234u;seed1=seed;
    swing_x=sway;wep_fire=0xffu;
    memset(amps,0,sizeof amps);
    assert(TestSpark()==NULL);
    assert(fire_angle==expected);
    assert(seed0==seed); /* same original RNG consumption */
    cases++;
}
int main(void)
{
    const s16 species[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR};
    const s16 weapons[][6]={{1,2,3,4,0,0},{1,2,3,0,0,0},{1,2,3,4,5,6}};
    const unsigned counts[]={4,3,6};
    const u16 seeds[]={0,1,0x7fff,0x8000,0xffff};
    const s16 swings[]={0,1,-1,7,-7,32767,-32768};
    for(unsigned p=0;p<3;p++)for(unsigned w=0;w<counts[p];w++)
    for(int melee=0;melee<2;melee++)for(int all=0;all<2;all++)
    for(unsigned s=0;s<sizeof seeds/sizeof *seeds;s++)
    for(unsigned x=0;x<sizeof swings/sizeof *swings;x++)
        check(species[p],weapons[p][w],melee,all,seeds[s],swings[x]);
    avp_player_set_center_aim_options(0,0);
    puts("R196 center aim: independent melee/all, all species, original spread/sway PASS");
    printf("Checked %u production shots\n",cases);
    return 0;
}
