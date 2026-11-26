/* RE39 R179: REAL AMP ranged-Predator Smart Disc launch at the signed-long
 * coordinate edge. Controlled direction replaces only the test's Vector()
 * stub; production attack selection, mode animation and projectile spawn
 * remain the original amp.c code. Run with -DAVP_TEST_NPC_VECTOR_OVERRIDE.
 */
#include "amp.h"
#include "player.h"
#include "collision.h"
#include "hud.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

s16 cur_level=1,num_cocoons;
u32 ccn_xsave,ccn_ysave;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
s16 game_over,key_lock;u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
u32 x_pos,y_pos;u16 invisflag;
s32 score;u32 fire_distance;
s16 player_energy,player_type,use_cocoon;
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static const u16 choices[]={0u,2u,2u};
static unsigned index_random;
u16 avp_random(void){return index_random<3u?choices[index_random++]:0u;}
/* Deliberate known normalised heading: magnitude independent of distance.
 * This tests the 68000 ADD.L position, not the separate Vector algorithm. */
void Vector(const AvpXY *from,const AvpXY *to,s32 *vx,s32 *vy)
{(void)from;(void)to;*vx=0x4000;*vy=-0x4000;}

int main(void)
{
    AvpAmp *a,*disc;
    InitAMPs();build_level();
    player_type=PT_HUMAN;
    invisflag=discflag=0;
    x_pos=y_pos=0x00200000u;
    index_random=0;
    a=amp_req();
    assert(a==&level1amps[0]);
    a->mode=chase_player;a->creature=AC_PREDATOR;
    a->xpos=(s32)x_pos+(90<<9);a->ypos=(s32)y_pos;
    a->energy=a->oldenergy=100;a->animseq=AS_RUN;
    chase_player(a);
    assert(index_random==3u);
    assert(a->mode!=chase_player && a->animseq==AS_FIGHT4);
    /* After attack selection, use a legal 68000 register-edge position.
     * The shot offset must wrap both X positive and Y negative overflow. */
    a->xpos=INT32_MAX-12;
    a->ypos=INT32_MIN+12;
    for(unsigned i=0;i<3u;i++)a->mode(a);
    disc=&level1amps[1];
    assert(disc->mode==pre_discmove && disc->creature==AC_SMART);
    assert(disc->xvector>0 && disc->yvector<0);
    assert((u32)disc->xpos==(u32)a->xpos+(u32)(disc->xvector>>3));
    assert((u32)disc->ypos==(u32)a->ypos+(u32)(disc->yvector>>3));
    assert(disc->xpos<0 && disc->ypos>0);
    puts("R179 Predator Smart Disc ADD.L launch offsets: PASS");
    return 0;
}
