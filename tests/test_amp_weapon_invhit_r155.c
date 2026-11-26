/* RE38 R155: original AMP projectile-hit INVHIT persistence.
 * Retail AMP.S applies hit flags with OR.W, never clearing an existing
 * INVHIT when a visible player shot lands. Excludes host/MP attack code. */
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;
u32 ccn_xsave,ccn_ysave,x_pos=0x00088000u,y_pos=0x00098000u,fire_distance;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
s32 score;
u16 invisflag;
s16 player_energy,player_type=PT_HUMAN,use_cocoon;
volatile u16 seed0,seed1;
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){return ++seed0;}
static void active_actor(AvpAmp *a){(void)a;}

int main(void)
{
    static const s16 starts[]={0,5,10,49,50,51,99,100,101,300,1000};
    static const u16 kinds[]={AC_BOLT,AC_SMART,AC_FIRE};
    const u16 phit=(u16)(1u<<AMP_PHIT);
    const u16 invhit=(u16)(1u<<AMP_INVHIT);
    const u16 closehit=(u16)(1u<<AMP_CLOSEHIT);
    const u16 player=(u16)(1u<<AMP_PLAYER);
    unsigned cases=0;
    AvpAmp *victim;
    AvpAmp shot={0};
    InitAMPs();build_level();
    victim=amp_req();
    assert(victim==&level1amps[0]);
    victim->xpos=0x00088000;victim->ypos=0x00098000;
    victim->creature=AC_PREDATOR;victim->mode=active_actor;
    amp_setgrid(victim);
    assert(collmap[9u*AVP_AMP_GRID_W+8u]==1u);
    shot.xpos=victim->xpos;shot.ypos=victim->ypos;
    shot.xvel=-1;  /* no original AMP slot to ignore */
    for(unsigned k=0;k<3u;k++)
    for(unsigned z=0;z<sizeof(starts)/sizeof(starts[0]);z++)
    for(unsigned vf=0;vf<8u;vf++)
    for(unsigned pf=0;pf<8u;pf++){
        const int damage=(k==0?100:(k==1?50:10));
        const s16 expected_energy=(s16)(starts[z]-damage);
        const u16 victim_flags=(u16)(((vf&1u)?phit:0u)|((vf&2u)?invhit:0u)|((vf&4u)?closehit:0u));
        const u16 projectile_flags=(u16)(((pf&1u)?phit:0u)|((pf&2u)?invhit:0u)|((pf&4u)?player:0u));
        const u16 transfer=projectile_flags&(u16)(phit|invhit);
        u16 expected_flags=victim_flags;
        if(expected_energy<=0)expected_flags&=(u16)~phit;
        expected_flags|=transfer;
        expected_flags&=(u16)~closehit;
        victim->energy=starts[z];victim->flags=victim_flags;
        shot.creature=kinds[k];shot.flags=projectile_flags;
        assert(weapon_hit(&shot)==victim);
        if(victim->energy!=expected_energy || victim->flags!=expected_flags){
            fprintf(stderr,"R155 mismatch k=%u health=%d vf=%u pf=%u: energy %d/%d flags %04x/%04x\n",
                k,(int)starts[z],vf,pf,(int)victim->energy,(int)expected_energy,
                victim->flags,expected_flags);
            return 1;
        }
        cases++;
    }
    /* The flag is not merely cosmetic: Predator honour scoring examines the
     * accumulated INVHIT when this same actor is subsequently scored. */
    victim->creature=AC_HUMAN;
    victim->energy=300;
    victim->flags=(u16)(phit|invhit);
    shot.creature=AC_BOLT;
    shot.flags=(u16)(phit|player); /* visible follow-up hit */
    assert(weapon_hit(&shot)==victim);
    assert((victim->flags&invhit)!=0u);
    player_type=PT_PREDATOR;
    score=10000;
    do_score(victim);
    assert(score==0); /* source: -2 * 10000 honour penalty, clamped */
    assert(cases==3u*11u*8u*8u);
    printf("R155 retail projectile INVHIT/PHIT retention: %u cases PASS\n",cases);
    return 0;
}
