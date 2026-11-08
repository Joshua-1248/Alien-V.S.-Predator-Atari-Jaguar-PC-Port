/* R290 retail-sequence regression for LEVELS.S::KillBastards.
 * The R278 original-assembly audit records mode-only removal.  The freed
 * slot must NOT be recycled; grids remain unchanged by KillBastards. */
#include "amp.h"
#include "levels.h"
#include "player.h"
#include "collision.h"
#include "hud.h"
#include <assert.h>

u32 x_pos, y_pos;
s16 player_type, player_energy, use_cocoon, num_cocoons, game_over, key_lock;
u8 acs_level;
u16 invisflag;
s32 score;
u16 avp_random(void){return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
s32 sin_d0(u16 a){(void)a;return 0;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}

static void live_mode(AvpAmp *a) {(void)a;}

int main(void)
{
    AvpAmp *victim, *bystander, *reused;
    const unsigned gx=6, gy=7, other=8;
    const unsigned pos=gy*AVP_AMP_GRID_W+gx;
    const unsigned opos=gy*AVP_AMP_OBJ_ROW_BYTES+gx*2u;

    player_type=PT_HUMAN;
    cur_level=3;
    initialise();
    build_level();
    victim=amp_req();
    bystander=amp_req();
    assert(victim==&level1amps[0]);
    assert(bystander==&level1amps[1]);
    victim->mode=live_mode; victim->creature=AC_HUMAN;
    victim->xpos=(s32)(gx<<16); victim->ypos=(s32)(gy<<16);
    bystander->mode=live_mode; bystander->creature=AC_HUMAN;
    bystander->xpos=(s32)(other<<16); bystander->ypos=(s32)(gy<<16);
    collmap[pos]=1;objmap[opos]=0;objmap[opos+1u]=1;
    collmap[gy*AVP_AMP_GRID_W+other]=2;
    x_pos=victim->xpos;y_pos=victim->ypos;

    KillBastards();
    assert(victim->mode==NULL && !victim->host_static);
    assert(collmap[pos]==1 && objmap[opos]==0 && objmap[opos+1u]==1);
    assert(collmap[gy*AVP_AMP_GRID_W+other]==2 && bystander->mode==live_mode);
    /* Mode-only removal keeps victim slot 0 allocated, so slot 2 is next.
     * This assertion fails against the R289 source. */
    reused=amp_req();
    assert(reused==&level1amps[2]);
    assert(reused->mode==NULL);
    reused->mode=live_mode; reused->xpos=(s32)(other<<16);reused->ypos=(s32)(gy<<16);

    /* All normal occupants sharing the next spawn cell lose mode, but the
     * original routine does not clear the cell's one-based grid owner. */
    x_pos=(u32)(other<<16); y_pos=(u32)(gy<<16);
    KillBastards();
    assert(reused->mode==NULL && bystander->mode==NULL);
    assert(collmap[gy*AVP_AMP_GRID_W+other]==2);
    assert(objmap[opos+1u]==1);
    assert(amp_req()==&level1amps[3]);
    return 0;
}
