/* RE39 R186 — original AMP.S shield2 generator-exhaustion lifecycle.
 * A Queen shield must clear its mode and grid word, but NOT release the
 * allocation slot. Runs the production make_shield/update path; R185 fails
 * the next-allocation assertion by recycling the shield record. */
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

static void exercise(unsigned count,int side_flags)
{
    AvpAmp owner={0};
    const unsigned gx=12u,gy=13u;
    owner.xpos=(s32)((gx<<16)|0x8000u);
    owner.ypos=(s32)((gy<<16)|0x8000u);
    player_type=PT_ALIEN;
    x_pos=(u32)(2u<<16);y_pos=(u32)(2u<<16);
    player_energy=300;
    InitAMPs();build_level();
    ngens=4u;
    for(unsigned i=0;i<count;i++){
        AvpAmp *s=make_shield(&owner,(s32)(i<<16),0);
        assert(s==&level1amps[i]);
        if(side_flags)s->flags|=(u16)(1u<<AMP_SSHIELD);
        assert(s->mode && s->creature==AC_AQSHIELD);
    }
    /* The first live update invokes amp_shield_start_mode, registers the
     * grid owner, and immediately executes shield2. */
    UpdateAMPs();
    for(unsigned i=0;i<count;i++){
        const unsigned cell=gy*AVP_AMP_GRID_W+(gx+i);
        assert(level1amps[i].mode==shield2);
        assert(collmap[cell]==i+1u);
    }
    /* Generator exhaustion: original code clears active mode and grid, but
     * the AMP remains allocated even when the last shield is removed. */
    ngens=0u;
    UpdateAMPs();
    for(unsigned i=0;i<count;i++){
        const unsigned cell=gy*AVP_AMP_GRID_W+(gx+i);
        assert(level1amps[i].mode==NULL);
        assert(level1amps[i].host_static==0);
        assert(collmap[cell]==0u);
    }
    /* The next real game allocation MUST skip every dormant shield. */
    assert(amp_req()==&level1amps[count]);
    assert(player_energy==300);
}
int main(void)
{
    exercise(1u,0);
    exercise(3u,1);
    puts("R186 original Queen shield shutdown: mode/grid cleared, AMP slot retained PASS");
    return 0;
}
