/* Use syntactically valid native actor shape generated from source-native
 * snapshots? This focused policy test links the real actor validity checker
 * separately, ensuring layout is not guessed. */
#include "local_mp_respawn_policy.h"
#include "player.h"
#include "local_mp.h"
#include <stdio.h>
#include <string.h>
#define CK(e) do{if(!(e)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
/* Focus only on plan logic in this standalone test: original actor state
 * validation is tested comprehensively by test_local_mp_actor_native. */
int avp_mp_actor_native_valid(const AvpMpActorNativeState *a){return a!=NULL;}
int main(void)
{
    AvpMpActorNativeState a={0}; AvpMpRespawnPlan p;
    a.player.player_type=PT_HUMAN;a.player.player_energy=1000;
    CK(avp_mp_respawn_plan(&a,1,AVP_MP_MARINE,0,&p)&&p.kind==AVP_MP_RESPAWN_NONE);
    a.player.player_energy=0;
    CK(avp_mp_respawn_plan(&a,0,AVP_MP_MARINE,0,&p)&&p.kind==AVP_MP_RESPAWN_SPECIES_START);
    CK(p.target_level==3&&p.reset_inventory&& !p.reset_honor&&!p.start_marine_shotgun);
    CK(avp_mp_respawn_plan(&a,1,AVP_MP_MARINE,0,&p)&&p.start_marine_shotgun);
    a.player.player_type=PT_PREDATOR;a.player.score=9000;
    CK(avp_mp_respawn_plan(&a,1,AVP_MP_MARINE,0,&p)&&p.target_level==15&&p.reset_honor&&p.reset_inventory&&!p.start_marine_shotgun);
    a.player.player_type=PT_ALIEN;
    CK(avp_mp_respawn_plan(&a,0,AVP_MP_MARINE,0,&p)&&p.target_level==14&&!p.reset_inventory&&!p.reset_honor);
    a.hudview.cocoon_data[2].frame=17;
    a.hudview.cocoon_data[2].level=8;
    a.hudview.cocoon_data[2].x=0x00241234;
    a.hudview.cocoon_data[2].y=0x000abcde;
    CK(avp_mp_respawn_plan(&a,1,AVP_MP_MARINE,0,&p)&&p.kind==AVP_MP_RESPAWN_COCOON);
    CK(p.target_level==8 && (unsigned)p.new_x==0x00248000u && (unsigned)p.new_y==0x000a8000u);
    CK(!p.reset_inventory&&!p.reset_honor&&!p.start_marine_shotgun);
    a.hudview.cocoon_data[2].frame=-2;
    CK(avp_mp_respawn_plan(&a,0,AVP_MP_MARINE,0,&p)&&p.kind==AVP_MP_RESPAWN_SPECIES_START && p.target_level==14);
    /* Campaign override must use campaign start without changing actor species,
     * and must supersede Alien cocoon only when explicitly enabled. */
    for(unsigned campaign=0;campaign<3u;campaign++){
        const unsigned destinations[]={3u,14u,15u};
        const s16 species[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR};
        for(unsigned slot=0;slot<3u;slot++){
            a.player.player_type=species[slot];
            a.player.player_energy=0;
            a.hudview.cocoon_data[2].frame=17;
            a.hudview.cocoon_data[2].level=8;
            CK(avp_mp_respawn_plan(&a,1,campaign,1,&p));
            CK(p.kind==AVP_MP_RESPAWN_SPECIES_START && p.target_level==destinations[campaign]);
            CK(p.start_marine_shotgun==(species[slot]==PT_HUMAN));
            CK(p.reset_honor==(species[slot]==PT_PREDATOR));
        }
    }
    CK(!avp_mp_respawn_plan(&a,0,5u,1,&p));
    {
        AvpMpActorNativeState original={0},other={0},out={0};
        original.player.player_type=PT_HUMAN;
        original.player.max_energy=700;original.player.player_energy=700;
        original.player.new_wepno=0;original.player.cur_weps=0;
        original.weapon.ammo_info[0].max=44;
        original.weapon.ammo_info[0].cur=0;
        out.player.player_energy=0;out.player.game_over=1;out.player.score=502;
        other.player.player_energy=314;other.player.score=1234;
        CK(avp_mp_respawn_restart(&out,&original,0x98000u,0xa8000u,1));
        CK(out.player.x_pos==0x98000u&&out.player.y_pos==0xa8000u);
        CK(out.player.player_energy==700&&!out.player.game_over && out.player.score==502);
        CK(out.weapon.ammo_info[0].cur==44);
        CK(out.player.cur_weps==(1u<<1)&&out.weapon.cur_weps==(1u<<1));
        CK(out.player.new_wepno==1&&out.weapon.new_wepno==1);
        CK(other.player.player_energy==314&&other.player.score==1234);
        CK(avp_mp_respawn_restart(&out,&original,0x38000u,0x48000u,0));
        CK(!(out.player.cur_weps&(1u<<1)) && out.weapon.ammo_info[0].cur==0);
        original.player.player_type=PT_PREDATOR;
        original.player.score=0;original.player.max_energy=2000;
        out.player.score=9000;
        CK(avp_mp_respawn_restart(&out,&original,0x18000u,0x28000u,1));
        CK(out.player.score==0&&out.player.player_energy==2000);
        CK(other.player.player_energy==314&&other.player.score==1234);
    }
    puts("PASS: Marine/Predator/Alien original starting levels (3/15/14), honor, optional shotgun, cocoon precedence, independent restart");
    return 0;
}
