/* R68: debug fresh first-visit populations only for current level. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <stdio.h>

s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;
u32 ccn_xsave,ccn_ysave,x_pos=0x00128000u,y_pos=0x00348000u,fire_distance;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
s32 score;
u16 invisflag;
s16 player_energy=75,player_type=PT_HUMAN,use_cocoon;
volatile u16 seed0,seed1;
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){return ++seed0;}

static unsigned live_count(void)
{
    unsigned n=0;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++)if(amp_data[i].mode)n++;
    return n;
}
int main(void)
{
    const AvpAmpPlacement authored_1[]={
        {(s16)(10u<<8),(s16)(10u<<8),0u},
        {(s16)(11u<<8),(s16)(10u<<8),0u},
        {-1,0,0}
    };
    const AvpAmpPlacement authored_2[]={
        {(s16)(12u<<8),(s16)(10u<<8),0u},
        {-1,0,0}
    };
    AvpAmpTemplate tmpl[1]={{0}};
    tmpl[0].mode=chase_player;tmpl[0].creature=AC_ALIEN;
    tmpl[0].energy=100;tmpl[0].oldenergy=100;
    InitAMPs();
    avp_amp_bind_templates(tmpl,1);
    avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,authored_1);
    avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,2,authored_2);
    restore_level();
    assert(live_count()==2u);
    assert((levels_visit&1u)!=0);
    amp_data[0].mode=NULL;  /* Enemy was eliminated before saving level 1. */
    save_level();
    assert(avp_amp_saved_level(1)[0].x!=-1);
    cur_level=2;
    restore_level();
    assert(live_count()==1u);
    save_level();
    const s16 other_first=avp_amp_saved_level(2)[0].x;
    assert(other_first!=-1);
    const u16 other_bit=levels_visit&2u;
    cur_level=1;
    restore_level();  /* Existing save, not first-visit spawn. */
    assert(live_count()==1u);
    avp_amp_dev_freshen_current_level();
    assert(!(levels_visit&1u));
    assert((levels_visit&2u)==other_bit);
    assert(avp_amp_saved_level(1)[0].x==-1);
    assert(avp_amp_saved_level(2)[0].x==other_first);
    restore_level();
    assert(live_count()==2u);
    assert((levels_visit&3u)==3u);
    assert(player_energy==75 && cur_level==1);
    assert(avp_amp_saved_level(2)[0].x==other_first);
    puts("R68 AMP per-level original first-visit population restore PASS");
    return 0;
}
