#include <assert.h>
#include <string.h>
#include "amp.h"
#include "player.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"


#include "collision.h"
#include "hud.h"
s16 cur_level=1,num_cocoons;
u32 ccn_xsave,ccn_ysave;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
s16 game_over,key_lock; u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *from,const AvpXY *to){(void)from;(void)to;return 0;}
u32 x_pos,y_pos;
u16 invisflag;
s16 player_energy,player_type,use_cocoon;
s32 score;
u32 fire_distance;
static const u16 *scripted_random;
static unsigned scripted_random_count,scripted_random_at;
u16 avp_random(void)
{
    return scripted_random_at<scripted_random_count?scripted_random[scripted_random_at++]:0;
}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static unsigned sfx_count,sfx_last,sfx_sequence[16];
static void record_sfx(void *u,unsigned id)
{
    (void)u;sfx_last=id;
    if(sfx_count<16u)sfx_sequence[sfx_count]=id;
    ++sfx_count;
}

int main(void)
{
    AvpAmp *a;
    InitAMPs();
    build_level();
    a=amp_req();

    QFRAME(a,3);
    assert(a->animframe==3);

    memset(collmap,0,sizeof(collmap));
    a->xpos=(s32)(7u<<16);
    a->ypos=(s32)(11u<<16);
    amp_setgrid(a);
    assert(collmap[11u*AVP_AMP_GRID_W+7u]==1u);

    score=0;
    player_type=PT_HUMAN;
    a->creature=AC_ALIEN;
    a->flags=(u16)(1u<<AMP_PHIT);
    do_score(a);
    assert(score==10000);

    score=0;
    player_type=PT_ALIEN;
    a->creature=AC_HUMAN;
    a->flags=(u16)(1u<<AMP_PHIT);
    do_score(a);
    assert(score==20000);

    score=100000;
    player_type=PT_PREDATOR;
    a->creature=AC_HUMAN;
    a->flags=(u16)((1u<<AMP_PHIT)|(1u<<AMP_INVHIT));
    do_score(a);
    assert(score==90000); /* -5000, doubled because not close = -10000 */

    score=100;
    a->flags=(u16)((1u<<AMP_PHIT)|(1u<<AMP_INVHIT));
    a->creature=AC_PREDATOR;
    do_score(a);
    assert(score==0); /* source clamps negative score */

    /* lockxxx: queen_delay=3 means qfight selection occurs on the fourth
     * in-range .lockx update (3,2,1,0,-1/BPL semantics). avp_random()=0
     * selects qswipe: initial frame 2, then 0,1,2,$fe damage 50. */
    a->xpos=a->ypos=0; x_pos=y_pos=0;
    a->energy=a->oldenergy=1000; a->flags=0; a->host_static=0;
    player_type=PT_HUMAN; player_energy=1000;
    lockxxx(a);
    a->mode(a); a->mode(a); a->mode(a);
    assert(a->animseq==AS_STAND);
    a->mode(a);
    assert(a->animseq==AS_FIGHT2 && a->animframe==2);
    a->mode(a); a->mode(a); a->mode(a); a->mode(a);
    assert(player_energy==900); /* 50 * 2 for non-Predator */

    /* Predator queen death: every second play_death update advances a frame;
     * after frame 3 the source locks input for 20 ticks then sets game_over. */
    a->energy=0; a->oldenergy=0; a->flags=0; a->host_static=0;
    player_type=PT_PREDATOR; key_lock=game_over=0;
    lockxxx(a); a->mode(a); /* queendead -> play_death */
    for(int i=0;i<6;i++)a->mode(a);
    assert(a->astype==57 && key_lock==1);
    for(int i=0;i<20;i++)a->mode(a);
    assert(game_over==1 && a->host_static==1);

    /* An egg has one opening cue, not one retrigger per animation frame.
     * Its original runtime descriptor is now resolved by FullSynth's
     * cartridge-backed sample-pointer mapping. */
    {
        AvpRuntimeOps ops={0};
        ops.play_sfx=record_sfx;
        avp_runtime_bind(&ops);
        initialise();
        build_level();
        a=amp_req();
        player_type=PT_HUMAN;
        a->mode=eggwait;
        a->creature=AC_EGG;
        a->energy=a->ldir=20;
        a->xpos=(s32)(6u<<16);
        a->ypos=(s32)(6u<<16);
        x_pos=(u32)a->xpos;y_pos=(u32)a->ypos;
        sfx_count=0u;sfx_last=0u;
        eggwait(a);
        assert(a->mode!=eggwait);
        a->mode(a);
        assert(sfx_count==1u&&sfx_last==AVP_FX_EGGX1);
        a->mode(a);
        assert(sfx_count==1u);
        avp_runtime_reset_ops();
    }
    /* Predator ranged attack sounds must match AMP.S: laser selection plays
     * MOTOR1, laser projectile launch plays ROCKET1, and the disc sound
     * plays only on selection, not again on throwdisc. */
    {
        static const u16 laser_choice[]={0,2,0};
        static const u16 disc_choice[]={0,2,2};
        AvpRuntimeOps ops={0};
        ops.play_sfx=record_sfx;avp_runtime_bind(&ops);
        InitAMPs();build_level();
        a=amp_req();
        x_pos=y_pos=0x00200000u;
        a->xpos=(s32)x_pos+(90<<9);a->ypos=(s32)y_pos;
        a->creature=AC_PREDATOR;a->energy=a->oldenergy=100;
        a->flags=0;a->host_static=0;a->animseq=AS_RUN;
        a->mode=chase_player;discflag=0;
        scripted_random=laser_choice;scripted_random_at=0;scripted_random_count=3;
        sfx_count=0;
        chase_player(a);
        assert(scripted_random_at==3);
        assert(sfx_count==1&&sfx_sequence[0]==AVP_FX_MOTOR);
        for(unsigned i=0;i<7u;i++)a->mode(a);
        assert(sfx_count==1);
        a->mode(a);
        assert(sfx_count==2&&sfx_sequence[1]==AVP_FX_ROCKE);
        /* The disc releases on frame 3 but has no second DISK1 cue. */
        InitAMPs();build_level();
        a=amp_req();
        a->xpos=(s32)x_pos+(90<<9);a->ypos=(s32)y_pos;
        a->creature=AC_PREDATOR;a->energy=a->oldenergy=100;
        a->flags=0;a->host_static=0;a->animseq=AS_RUN;
        a->mode=chase_player;discflag=0;
        scripted_random=disc_choice;scripted_random_at=0;scripted_random_count=3;
        sfx_count=0;chase_player(a);
        assert(scripted_random_at==3);
        assert(sfx_count==1&&sfx_sequence[0]==AVP_FX_DISK1);
        for(unsigned i=0;i<3u;i++)a->mode(a);
        assert(sfx_count==1);
        scripted_random=NULL;scripted_random_count=scripted_random_at=0;
        avp_runtime_reset_ops();
    }
    return 0;
}
