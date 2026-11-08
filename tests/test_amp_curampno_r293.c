/* R293 production-linked regression: original AMP.S amp_setgrid writes the
 * current UpdateAMPs() loop number (CURAMPNO), not the supplied record's ID.
 * Outside an update the host's template-registration helper uses record ID. */
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <string.h>

u32 x_pos,y_pos;
s16 player_type,player_energy,use_cocoon,num_cocoons,game_over,key_lock,cur_level=1;
u8 acs_level;
u16 invisflag;
s32 score;
u16 avp_random(void){return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
s32 sin_d0(u16 a){(void)a;return 0;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}

static unsigned calls;
static void noop(AvpAmp *a){(void)a;}
static void write_other(AvpAmp *a)
{
    AvpAmp *other=(a==&level1amps[0])?&level1amps[1]:&level1amps[0];
    ++calls;
    amp_setgrid(other);
}
static unsigned at(const AvpAmp *a)
{
    return ((u32)a->ypos>>16)*AVP_AMP_GRID_W+((u32)a->xpos>>16);
}
int main(void)
{
    AvpAmp *a,*b;
    player_type=PT_HUMAN;
    InitAMPs();build_level();
    a=amp_req();b=amp_req();
    assert(a==&level1amps[0]);assert(b==&level1amps[1]);
    a->xpos=(s32)((7u<<16)|0x8000u);a->ypos=(s32)((8u<<16)|0x8000u);
    b->xpos=(s32)((9u<<16)|0x8000u);b->ypos=(s32)((10u<<16)|0x8000u);
    a->mode=noop;b->mode=noop;
    /* Spawn/setup is outside UpdateAMPs, and registers the target slot. */
    amp_setgrid(b);
    assert(collmap[at(b)]==2u);
    memset(collmap,0,sizeof collmap);
    /* In the retail actor loop, actor #1 registering actor #2 writes #1. */
    a->mode=write_other;
    UpdateAMPs();
    assert(calls==1u);
    assert(collmap[at(b)]==1u);
    /* After the loop, no stale CURAMPNO can contaminate host setup. */
    amp_setgrid(b);
    assert(collmap[at(b)]==2u);
    memset(collmap,0,sizeof collmap);
    a->mode=noop;b->mode=write_other;
    UpdateAMPs();
    assert(calls==2u);
    assert(collmap[at(a)]==2u);
    /* Ordinary same-actor writes are unchanged in the actor loop. */
    memset(collmap,0,sizeof collmap);
    a->mode=noop;b->mode=noop;
    amp_setgrid(a);amp_setgrid(b);
    assert(collmap[at(a)]==1u && collmap[at(b)]==2u);
    return 0;
}
