/* RE39 R184: original start_hug Alien branch is MODE-ONLY removal.
 * Exercises the real AMP pool and UpdateAMPs, not a fake lifecycle helper.
 * Negative control on untouched R183 must fail when slot zero is recycled. */
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>

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

int main(void)
{
    AvpAmp *a, *b, *c, *d;
    const unsigned sx=9u,sy=11u,cell=sy*AVP_AMP_GRID_W+sx;
    AvpAmpModeFn initial=avp_amp_builtin_mode_for_def(4);
    assert(initial!=NULL);

    player_type=PT_ALIEN;
    InitAMPs();
    build_level();
    a=amp_req();
    assert(a==&level1amps[0]);
    a->xpos=(s32)((sx<<16)|0x8000u);
    a->ypos=(s32)((sy<<16)|0x8000u);
    a->mode=initial;
    a->host_static=0;
    a->astype=4;
    amp_setgrid(a);
    assert(collmap[cell]==1u);
    /* The frame loop applies the actual initial facehugger mode. */
    UpdateAMPs();
    assert(a->mode==NULL && !a->host_static);
    /* A mode-only Jaguar shutdown must not erase an existing grid word. */
    assert(collmap[cell]==1u);
    /* Nor may it place the inactive AMP on the allocator free list. */
    b=amp_req();
    assert(b==&level1amps[1]);
    b->mode=initial;b->astype=4;b->host_static=0;
    b->xpos=(s32)(12u<<16);b->ypos=(s32)(13u<<16);
    UpdateAMPs();
    assert(b->mode==NULL && !b->host_static);
    c=amp_req();
    assert(c==&level1amps[2]);

    /* In both other campaigns the starter becomes a real chase state.
     * No slot release/host_static conversion is permitted here either. */
    player_type=PT_HUMAN;
    c->mode=initial;c->astype=4;c->host_static=0;
    c->xpos=(s32)(14u<<16);c->ypos=(s32)(13u<<16);
    UpdateAMPs();
    assert(c->mode==chase_player && !c->host_static);
    player_type=PT_PREDATOR;
    d=amp_req();
    assert(d==&level1amps[3]);
    d->mode=initial;d->astype=4;d->host_static=0;
    d->xpos=(s32)(16u<<16);d->ypos=(s32)(13u<<16);
    UpdateAMPs();
    assert(d->mode==chase_player && !d->host_static);
    assert(amp_req()==&level1amps[4]);
    puts("R184 AMP facehugger start: original mode-only Alien slot and grid lifecycle PASS");
    return 0;
}
