/* Original Jaguar AMP enemy-reaction sequencing. This is a regression, not a
 * gameplay instrumentation or an artificial pain-probability implementation.
 * Compare each branch against the original C reconstruction in the project
 * reference and retain the cocoon priority for stunned Marines. */
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>

s16 cur_level=1,num_cocoons,game_over,key_lock,player_energy,player_type,use_cocoon;
u8 acs_level;
u32 ccn_xsave,ccn_ysave,x_pos,y_pos,fire_distance;
u16 invisflag;
s32 score;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static void waiting(AvpAmp*a){(void)a;}

static AvpAmp*new_actor(u16 creature,s16 energy,s16 oldenergy)
{
    AvpAmp*a;
    InitAMPs();build_level();
    a=amp_req();a->creature=creature;a->mode=chase_player;
    a->animseq=AS_RUN;a->animframe=0;
    a->xpos=0x000a8000;a->ypos=0x000a8000;
    a->energy=energy;a->oldenergy=oldenergy;a->timer=0;
    x_pos=0x000d8000u;y_pos=0x000a8000u;
    player_type=PT_HUMAN;invisflag=discflag=0;
    return a;
}
int main(void)
{
    const u16 types[]={AC_ALIEN,AC_PREDATOR,AC_HUMAN,AC_ACRAWL};
    for(unsigned i=0;i<sizeof(types)/sizeof(types[0]);i++){
        AvpAmp*a=new_actor(types[i],120,150);
        chase_player(a);
        assert(a->mode!=chase_player);
        assert(a->animseq==AS_KNOCKBACK);
        assert(a->oldenergy==150); /* retail recoil never resets snapshot */
        assert(a->energy==120);
        a=new_actor(types[i],120,120);
        chase_player(a);
        assert(a->animseq!=AS_KNOCKBACK);
    }
    /* Retail RDB.TXT at CPU $154CC-$15534 (RAM $8000 mapping):
     *   ADDI.W #1,$2A6A8; ANDI.W #7,$2A6A8;
     *   TST.W $2A6A8; BEQ.W skip_copy;
     *   MOVE.W $26(A0),D7; MOVE.W D7,$28(A0).
     * Thus energy snapshots happen at phases 1..7, NOT at phase zero.
     * The previous C reconstruction reversed BEQ and kept damage stale for
     * seven ticks, causing repeat knockback and apparent guaranteed pain.
     */
    {
        AvpAmp*a=new_actor(AC_HUMAN,120,150);
        a->mode=waiting;
        for(unsigned j=1;j<=8;j++){
            UpdateAMPs();
            if(j==1u)assert(a->oldenergy==120);
            if(j==8u)assert(a->oldenergy==120);
        }
        /* After eight calls, entime is aligned at phase zero. */
    }
    for(unsigned i=0;i<sizeof(types)/sizeof(types[0]);i++){
        AvpAmp*a=new_actor(types[i],120,150);
        /* Damage before an ordinary phase is snapshotted BEFORE AI runs:
         * no new recoil, and no invented random-chance calculation. */
        UpdateAMPs(); /* phase 1 */
        assert(a->oldenergy==120);
        assert(a->animseq!=AS_KNOCKBACK);
        a->mode=waiting;
        for(unsigned tick=2;tick<=7u;tick++){
            UpdateAMPs();
            assert(a->oldenergy==120);
        }
        /* Another REAL hit before phase 0 remains visible to the AI. */
        a->energy=90;
        a->mode=chase_player;
        UpdateAMPs(); /* phase 0: no snapshot */
        assert(a->animseq==AS_KNOCKBACK);
        assert(a->timer==2);
        assert(a->oldenergy==120);
        UpdateAMPs(); /* phase 1: snapshot catches up DURING recovery */
        assert(a->oldenergy==90);
        assert(a->animseq==AS_KNOCKBACK);
        assert(a->timer==1);
        UpdateAMPs(); /* phase 2: return to chase */
        assert(a->animseq==AS_RUN);
        assert(a->mode==chase_player);
        UpdateAMPs(); /* phase 3: same hit must NOT trigger again */
        assert(a->animseq!=AS_KNOCKBACK);
        a->mode=waiting;
        for(unsigned tick=12;tick<=16u;tick++)UpdateAMPs();
        /* Sixteen calls end on phase 0, so the next species starts aligned. */
    }
    /* Alien tail-stunned Marines enter the source's stun/cocoon mode BEFORE
     * generic energy-decrease recoil; this must not be altered by pain work. */
    {
        AvpAmp*a=new_actor(AC_HUMAN,5,55);
        a->flags=(1u<<AMP_STUN);
        chase_player(a);
        assert(a->mode==stun_mode);
        assert(a->animseq==AS_KNOCKBACK);
        assert(a->oldenergy==55);
    }
    /* The original Predator melee mode does not abort on an energy drop.
     * The Alien's close-fight mode DOES; this is state-specific, not random. */
    {
        AvpAmp *a=new_actor(AC_PREDATOR,150,150);
        x_pos=0x000a9000u;
        chase_player(a);
        for(unsigned j=0;j<4;j++)a->mode(a);
        assert(a->mode!=chase_player);
        a->energy=140;
        a->mode(a);
        assert(a->animseq!=AS_KNOCKBACK);
        a=new_actor(AC_ALIEN,150,150);
        x_pos=0x000a9000u;
        chase_player(a);
        for(unsigned j=0;j<4;j++)a->mode(a);
        a->energy=140;
        a->mode(a);
        assert(a->animseq==AS_KNOCKBACK);
    }
    puts("Retail 68000 BEQ snapshot timing, one-shot recoil, state gates and cocoon priority PASS");
    return 0;
}
