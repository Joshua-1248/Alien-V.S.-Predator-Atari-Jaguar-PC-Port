/* Historical R185 enemy Predator throw timing regression.
 * R201 preserves this test's animation and spawn assertions, changing only
 * the audio expectation after the user's report of duplicate DISK1 sounds. */
#include <assert.h>
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

/* Deterministically select the throw-disc branch of amp_ranged_predator. */
static unsigned random_at;
static const u16 random_choice[]={0,2,2};
u16 avp_random(void)
{
    return random_at<3u?random_choice[random_at++]:0u;
}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static unsigned sounds[4],nsounds;
static void record(void *user,unsigned id)
{
    (void)user;
    if(nsounds<4u)sounds[nsounds]=id;
    ++nsounds;
}
int main(void)
{
    AvpRuntimeOps ops={0};
    AvpAmp *a;
    ops.play_sfx=record;
    avp_runtime_bind(&ops);
    InitAMPs();build_level();
    a=amp_req();
    x_pos=y_pos=0x00200000u;
    a->xpos=(s32)x_pos+(90<<9);a->ypos=(s32)y_pos;
    a->creature=AC_PREDATOR;a->energy=a->oldenergy=100;
    a->flags=0;a->host_static=0;a->animseq=AS_RUN;
    a->mode=chase_player;discflag=0;
    player_type=PT_HUMAN;invisflag=0;
    chase_player(a);
    assert(random_at==3u);
    assert(nsounds==1u && sounds[0]==AVP_FX_DISK1);
    assert(a->animseq==AS_FIGHT4 && a->animframe==0);
    a->mode(a); /* 0 -> 1 */
    assert(nsounds==1u && a->animframe==1);
    a->mode(a); /* 1 -> 2 */
    assert(nsounds==1u && a->animframe==2);
    a->mode(a); /* 2 -> 3: disc spawns without another audible cue */
    assert(nsounds==1u);
    assert(a->animframe==3 && discflag==1u);
    assert(amp_data[1].creature==AC_SMART);
    a->mode(a); /* 3 -> 4 */
    a->mode(a); /* 4 -> 5 / return to chase */
    assert(nsounds==1u && a->mode==chase_player);
    avp_runtime_reset_ops();
    return 0;
}
