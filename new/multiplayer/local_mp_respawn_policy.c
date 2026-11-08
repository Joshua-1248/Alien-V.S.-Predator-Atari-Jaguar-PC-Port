#include "local_mp_respawn_policy.h"
#include "player.h"
#include "pc/pc_marine_start.h"
#include <string.h>
int avp_mp_respawn_plan(const AvpMpActorNativeState *actor,
                        int shotgun,unsigned campaign,int campaign_start_enabled,
                        AvpMpRespawnPlan *out)
{
    AvpMpRespawnPlan result;
    int16_t species;
    if(!actor||!out||!avp_mp_actor_native_valid(actor))return 0;
    memset(&result,0,sizeof(result));
    if(actor->player.player_energy>0){*out=result;return 1;}
    species=actor->player.player_type;
    if(species!=PT_HUMAN && species!=PT_PREDATOR && species!=PT_ALIEN)return 0;
    if(campaign_start_enabled){
        /* New co-op-only preference. Destination is the campaign beginning;
         * all other restart/weapon/HUD rules use this actor's own species. */
        static const unsigned start_level[]={3u,14u,15u};
        if(campaign>2u)return 0;
        result.kind=AVP_MP_RESPAWN_SPECIES_START;
        result.target_level=start_level[campaign];
        result.new_x=-1;result.new_y=-1;
        result.reset_inventory=species!=PT_ALIEN;
        result.reset_honor=species==PT_PREDATOR;
        result.start_marine_shotgun=species==PT_HUMAN && shotgun!=0;
        *out=result;return 1;
    }
    if(species==PT_ALIEN){
        /* Original HUD.S CheckCocoon compares the oldest frame with 17.
         * Original UseCocoon, not this function, must consume the cocoon,
         * restore health, and compute its destination coordinates. */
        const AvpCocoonState *oldest=&actor->hudview.cocoon_data[AVP_MAX_COCOONS-1];
        if(oldest->frame==17 && oldest->level>=1 && oldest->level<=15){
            result.kind=AVP_MP_RESPAWN_COCOON;
            result.target_level=(unsigned)oldest->level;
            result.new_x=(int32_t)((oldest->x&0xffff0000u)|0x8000u);
            result.new_y=(int32_t)((oldest->y&0xffff0000u)|0x8000u);
            *out=result;return 1;
        }
    }
    result.kind=AVP_MP_RESPAWN_SPECIES_START;
    result.new_x=-1;result.new_y=-1;
    switch(species){
        case PT_HUMAN:
            result.target_level=3u;
            result.reset_inventory=1;
            result.start_marine_shotgun=shotgun!=0;
            break;
        case PT_PREDATOR:
            /* Original LEVELS.S starts[] assigns Predator to level 15. */
            result.target_level=15u;
            result.reset_inventory=1;
            result.reset_honor=1;
            break;
        case PT_ALIEN:
            /* Original LEVELS.S starts[] assigns Alien to level 14. */
            result.target_level=14u;
            break;
        default:return 0;
    }
    *out=result;return 1;
}

int avp_mp_respawn_restart(AvpMpActorNativeState *target,
    const AvpMpActorNativeState *original_start,uint32_t spawn_x,
    uint32_t spawn_y,int start_shotgun)
{
    AvpMpActorNativeState fresh;
    if(!target||!original_start||!avp_mp_actor_native_valid(original_start))return 0;
    if(original_start->player.player_type!=PT_HUMAN &&
       original_start->player.player_type!=PT_PREDATOR &&
       original_start->player.player_type!=PT_ALIEN)return 0;
    fresh=*original_start;
    fresh.player.x_pos=spawn_x;fresh.player.y_pos=spawn_y;
    fresh.player.x_vel=0;fresh.player.y_vel=0;
    /* Only the Predator loses honor on death. Marine/Alien score is not
     * inventory, so preserve it as requested rather than resetting it. */
    fresh.player.score=fresh.player.player_type==PT_PREDATOR?
        0:target->player.score;
    fresh.player.player_dead=0;fresh.player.game_over=0;
    fresh.player.key_lock=0;fresh.player.launch_flag=0;
    fresh.player.player_energy=fresh.player.max_energy;
    fresh.player.old_energy=fresh.player.max_energy;
    if(fresh.player.player_type==PT_HUMAN){
        if(start_shotgun){
            (void)avp_pc_marine_start_shotgun_apply(1,0,PT_HUMAN,
                &fresh.player.cur_weps,&fresh.player.new_wepno,
                fresh.weapon.ammo_info);
        }else{
            fresh.player.cur_weps=(u8)(fresh.player.cur_weps&~(1u<<1));
            fresh.player.new_wepno=0;
            fresh.weapon.ammo_info[0].cur=0;
        }
        fresh.weapon.cur_weps=fresh.player.cur_weps;
        fresh.weapon.new_wepno=fresh.player.new_wepno;
    }
    if(!avp_mp_actor_native_valid(&fresh))return 0;
    *target=fresh;
    return 1;
}
