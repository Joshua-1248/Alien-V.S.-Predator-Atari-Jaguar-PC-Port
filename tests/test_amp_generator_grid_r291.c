/* R291: verified AMP.S .xgen2 / ?genmode mode-only generator exits.
 * Linked to actual production amp.c; negative baseline must fail R290. */
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>

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
    static const unsigned defs[]={43u,44u,45u,46u,47u,48u};
    const unsigned gy=9u;
    player_type=PT_HUMAN;
    InitAMPs();build_level();
    ngens=0u;
    for(unsigned i=0;i<6u;i++) {
        AvpAmp *a=amp_req();
        unsigned gx=12u+i, at=gy*AVP_AMP_GRID_W+gx;
        assert(a==&level1amps[i]);
        a->xpos=(s32)((gx<<16)|0x8000u);
        a->ypos=(s32)((gy<<16)|0x8000u);
        a->creature=AC_GEN;
        a->mode=avp_amp_builtin_mode_for_def(defs[i]);
        a->host_static=1;
        assert(a->mode);
        amp_setgrid(a);
        assert(collmap[at]==i+1u);
        /* Definition 43 first enters generate to install .xgen2. */
        if(i==0u){ a->mode(a); assert(a->mode); }
        a->mode(a);
        assert(a->mode==NULL);
        assert(!a->host_static);
        /* R290 zeroed this owner; the 1994 ASM does not. */
        assert(collmap[at]==i+1u);
    }
    /* Under retail rules no generator AMP was released. */
    assert(amp_req()==&level1amps[6]);
    return 0;
}
