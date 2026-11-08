#include "amp.h"
#include "player.h"
#include "mazescrn.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include <assert.h>

#include "collision.h"
#include "hud.h"
s16 cur_level=1,num_cocoons;
u32 ccn_xsave,ccn_ysave;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
s16 game_over,key_lock; u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *from,const AvpXY *to){(void)from;(void)to;return 0;}
s32 score;
u32 fire_distance;
u32 x_pos,y_pos;u16 invisflag; s16 player_energy,player_type,use_cocoon;
static int deterministic_spawn_random;
static u16 spawn_random_calls;
u16 avp_random(void){
    if(deterministic_spawn_random)return (u16)(spawn_random_calls++/2u);
    return 0;
} s32 sin_d0(u16 a){(void)a;return 0;} s32 cos_d0(u16 a){(void)a;return 0x4000;}
extern int avp_test_open_door;
static int amp_door_safe_pos(void *u,s32 x,s32 y,s16 h,u16 w)
{
    (void)u;(void)h;(void)w;
    /* Door at the east edge of cell (1,1), with a narrow clear centre. */
    if((u32)x>=0x0001a400u){
        u16 frac=(u16)y;
        if(!avp_test_open_door || frac<0x6000u || frac>0xa000u)
            return AVP_COLL_DOOR;
    }
    return AVP_COLL_SAFE;
}
static void dummy(AvpAmp*a){(void)a;}
static int amp_always_safe(void *u,s32 x,s32 y,s16 h,u16 w)
{(void)u;(void)x;(void)y;(void)h;(void)w;return AVP_COLL_SAFE;}
static int host_mode_calls;
static void counted_mode(AvpAmp*a){(void)a;++host_mode_calls;}
extern unsigned amp_test_alarm_starts,amp_test_alarm_kills,amp_test_alarm_sound;
extern unsigned amp_test_effect_starts,amp_test_effect_stops,amp_test_last_effect;
extern u32 amp_test_last_released;
static unsigned gen_updates,gen_ambient_kills;
static s32 gen_last_volume,gen_last_pitch;
static void gen_mod(void *u,unsigned id,s32 vol,s32 pitch)
{
    (void)u;assert(id==AVP_FX_FORCE);
    ++gen_updates;gen_last_volume=vol;gen_last_pitch=pitch;
}
static void gen_amb(void *u){(void)u;++gen_ambient_kills;}
static unsigned queen_attack_sound_count;
static void queen_attack_sound(void *u,unsigned id)
{(void)u;assert(id==AVP_FX_ASHRI);++queen_attack_sound_count;}
static unsigned explosion_sounds;
static s32 explosion_rate;
static void explosion_mod(void *u,unsigned id,s32 rate,s32 unused)
{
    (void)u;assert(id==AVP_FX_XP1B1);assert(unused==0);
    ++explosion_sounds;explosion_rate=rate;
}
static u32 sampled_beam;
static u32 beam_after_los(void){return sampled_beam;}
static int marine_fight_action;
static void capture_marine_fight(AvpAmp *a,int action)
{
    (void)a;
    marine_fight_action=action;
}
int main(void){unsigned i;for(i=0;i<AVP_NUM_AMPS;++i){level1amps[i].mode=dummy;level1amps[i].flags=0xffff;}InitAMPs();assert(levels_visit==0);for(i=0;i<AVP_NUM_AMPS;++i)assert(level1amps[i].mode==0);build_level();{AvpAmp*a=amp_req();assert(a==&level1amps[0]);assert(a->flags==0&&a->astype==-1);} {AvpAmp*b=amp_req();assert(b==&level1amps[1]);amp_release(2);assert(amp_req()==&level1amps[1]);}
    {
        const AvpAmpSavedPlacement *saved;
        AvpAmp *dead=&level1amps[0], *live=&level1amps[1];
        dead->mode=NULL;
        dead->host_static=1;
        dead->astype=53;
        dead->xpos=0x00108000;
        dead->ypos=0x00118000;
        dead->flags=(u16)(1u<<AMP_DEAD);
        live->mode=dummy;
        live->host_static=0;
        live->astype=4;
        live->xpos=0x00124000;
        live->ypos=0x00156000;
        live->flags=0;
        ram_save();
        saved=avp_amp_saved_level(1);
        assert(saved&&saved[0].astype==53);
        assert(saved[0].flags&(1u<<AMP_DEAD));
        assert(saved[0].x==(s16)(dead->xpos>>8));
        assert(saved[0].y==(s16)(dead->ypos>>8));
        assert(saved[1].astype==4);
        assert(saved[1].x==(s16)(live->xpos>>8));
        assert(saved[1].y==(s16)(live->ypos>>8));
        assert(saved[2].x==-1);
    }
    /* Two release calls on the same projectile/actor slot must not make
     * subsequent allocations alias and overwrite each other. */
    {
        AvpAmp *one,*two;
        InitAMPs();build_level();
        one=amp_req();assert(one==&level1amps[0]);
        amp_release(1);
        amp_release(1); /* must be ignored */
        amp_release(AVP_NUM_AMPS+1); /* invalid owner must be ignored */
        one=amp_req();two=amp_req();
        assert(one==&level1amps[0]&&two==&level1amps[1]);
    }
    /* A static prop was visible across level changes because renderer draw
     * eligibility includes host_static, while initialise cleared only mode.
     * Preserve the current-level static slots until a level reset; clear
     * every old slot on reset, including one that won't be reallocated. */
    {
        AvpAmp *prop=&level1amps[AVP_NUM_AMPS-1u];
        prop->mode=NULL;
        prop->host_static=1;
        prop->aux_ptr=(void*)prop;
        prop->creature=AC_TABLE1;
        initialise();
        assert(prop->mode==NULL);
        assert(prop->host_static==0);
        assert(prop->aux_ptr==NULL);
        for(unsigned k=0;k<AVP_NUM_AMPS;k++)
            assert(!level1amps[k].host_static);
    }
    /* The PC must supply a progressing Jaguar beam phase: an enemy Marine
     * in range with unobstructed sight can enter its authored firing mode at
     * the correct $f00004 & 7 == 2 phase. */
    {
        AvpAmp *marine=amp_req();
        marine->mode=chase_player;
        marine->creature=AC_HUMAN;
        marine->xpos=0x00130000;
        marine->ypos=0x00120000;
        x_pos=0x00128000;y_pos=0x00128000;
        marine->energy=marine->oldenergy=100;
        marine->timer=1;
        marine->flags=0;
        player_type=PT_PREDATOR;
        discflag=0;invisflag=0;
        avp_amp_set_beam_counter(2);
        chase_player(marine);
        assert(marine->animseq==AS_FIGHT1 || marine->animseq==AS_FIGHT2);
        assert(marine->mode!=chase_player);
        /* The first read remains at the firing phase (2), while the
         * independent second TOM sample can have either parity.  Both
         * retail weapon choices must be reachable. */
        avp_amp_set_fight_callback(capture_marine_fight);
        avp_amp_set_beam_weapon_sample(beam_after_los);
        sampled_beam=3;
        marine->mode=chase_player;
        marine->energy=marine->oldenergy=100;
        marine_fight_action=0;
        gofight(marine);
        assert(marine_fight_action==AVP_AMP_FIGHT_FLAME);
        sampled_beam=2;
        marine->mode=chase_player;
        marine->energy=marine->oldenergy=100;
        marine_fight_action=0;
        gofight(marine);
        assert(marine_fight_action==AVP_AMP_FIGHT_CROUCH);
        avp_amp_set_beam_weapon_sample(NULL);
        /* Production has no Jaguar raster callback. The second beam read
         * must still make both original Marine weapon branches reachable. */
        marine->mode=chase_player;
        marine_fight_action=0;
        gofight(marine);
        int first_weapon=marine_fight_action;
        assert(first_weapon==AVP_AMP_FIGHT_FLAME ||
               first_weapon==AVP_AMP_FIGHT_CROUCH);
        marine->mode=chase_player;
        marine_fight_action=0;
        gofight(marine);
        assert(marine_fight_action==AVP_AMP_FIGHT_FLAME ||
               marine_fight_action==AVP_AMP_FIGHT_CROUCH);
        assert(marine_fight_action!=first_weapon);
        avp_amp_set_fight_callback(NULL);
    }
    /* The collision grid stores one-based AMP slot numbers, not boolean
     * occupancy.  Template placement and random spawning must never map
     * later actors back to AMP 1: weapon_hit() dereferences that number. */
    {
        static const AvpAmpTemplate template[] = {
            {.mode=dummy,.creature=AC_HUMAN,.animseq=AS_STAND,
             .energy=150,.oldenergy=150,.flags=1u<<AMP_KILLABLE}
        };
        static const AvpAmpPlacement placements[] = {
            {0x0880,0x0880,0},{0x0980,0x0880,0},{0x0a80,0x0880,0},
            {-1,0,0}
        };
        static const AvpAmpRandomEntry randoms[]={{0,1},{-1,0}};
        AvpAmp shot={0};
        AvpAmp *victim;
        InitAMPs();
        player_type=PT_HUMAN;cur_level=1;
        avp_amp_bind_templates(template,1);
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,placements);
        build_level();
        assert(collmap[8u*AVP_AMP_GRID_W+8u]==1u);
        assert(collmap[8u*AVP_AMP_GRID_W+9u]==2u);
        assert(collmap[8u*AVP_AMP_GRID_W+10u]==3u);
        shot.creature=AC_BOLT;
        shot.xpos=level1amps[2].xpos;
        shot.ypos=level1amps[2].ypos;
        shot.xvel=-1;
        shot.flags=1u<<AMP_PHIT;
        victim=weapon_hit(&shot);
        assert(victim==&level1amps[2]);
        assert(level1amps[2].energy==50);
        assert(level1amps[0].energy==150);
        assert(level1amps[1].energy==150);
        avp_amp_bind_random_list(0,1,randoms);
        rand_set();
        assert(collmap[0]==4u);
        shot.xpos=level1amps[3].xpos;
        shot.ypos=level1amps[3].ypos;
        victim=weapon_hit(&shot);
        assert(victim==&level1amps[3]);
        assert(level1amps[3].energy==50);
        assert(level1amps[0].energy==150);
    }
    /* R50/R51 owner registration must survive the production LoadLevel path:
     * restore_level() builds the AMP population after initialise/place_grid.
     * Clearing collmap once more after that silently removed every owner,
     * making projectile hits fail even though direct build_level tests passed.
     * Verify both an initial visit and a saved-level revisit. */
    {
        static const AvpAmpPlacement guards[]={
            {0x0880,0x0880,2}, {0x0980,0x0880,2},
            {0x0a80,0x0880,11}, /* medkit must populate object grid */
            {-1,0,0}
        };
        static const AvpAmpPlacement empty[]={ {-1,0,0} };
        static const AvpAmpRandomEntry no_random[]={ {-1,0} };
        AvpAmp shot={0};
        InitAMPs();
        avp_amp_bind_retail_source_templates_and_random();
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,guards);
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,2,empty);
        for(unsigned k=0;k<3u;k++){
            avp_amp_bind_random_list(k,1,no_random);
            avp_amp_bind_random_list(k,2,no_random);
        }
        player_type=PT_ALIEN;cur_level=1;
        x_pos=0x000e8000u;y_pos=0x000e8000u;
        restore_level();
        assert(collmap[8u*AVP_AMP_GRID_W+8u]==1u);
        assert(collmap[8u*AVP_AMP_GRID_W+9u]==2u);
        /* Retail collectables occupy objmap only.  They must not be
         * mistaken for AMP collision owners or obstruct enemy navigation. */
        assert(collmap[8u*AVP_AMP_GRID_W+10u]==0u);
        UpdateAMPs(); /* static pickup writes its object-grid registration */
        assert(collmap[8u*AVP_AMP_GRID_W+10u]==0u);
        assert(objmap[8u*AVP_AMP_OBJ_ROW_BYTES+20u]==0u);
        assert(objmap[8u*AVP_AMP_OBJ_ROW_BYTES+21u]==3u);
        shot.creature=AC_BOLT;
        shot.xpos=level1amps[2].xpos;
        shot.ypos=level1amps[2].ypos;
        shot.xvel=-1;
        assert(weapon_hit(&shot)==NULL); /* medkit is not an enemy */
        shot.xpos=level1amps[1].xpos;
        shot.ypos=level1amps[1].ypos;
        shot.xvel=-1;shot.flags=1u<<AMP_PHIT;
        assert(weapon_hit(&shot)==&level1amps[1]);
        assert(level1amps[1].energy==-50);
        /* Save and leave, then return: no stale owner from the next level,
         * and all still-live registrations get rebuilt on reentry. */
        save_level();
        cur_level=2;restore_level();
        assert(collmap[8u*AVP_AMP_GRID_W+8u]==0u);
        assert(collmap[8u*AVP_AMP_GRID_W+9u]==0u);
        cur_level=1;restore_level();
        assert(collmap[8u*AVP_AMP_GRID_W+8u]==1u);
        assert(collmap[8u*AVP_AMP_GRID_W+9u]==2u);
        assert(collmap[8u*AVP_AMP_GRID_W+10u]==0u);
        UpdateAMPs();
        assert(objmap[8u*AVP_AMP_OBJ_ROW_BYTES+21u]==3u);
    }
    /* Retail AMP.S::amp_cleargrid clears the previous actor's cell even when
     * another AMP wrote its one-based number there afterward. This differs
     * from the former PC owner-preserving cleanup. */
    {
        AvpAmp *old_owner,*current_owner;
        AvpAmp shot={0};
        InitAMPs();
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,NULL);
        build_level();
        old_owner=amp_req();
        current_owner=amp_req();
        old_owner->xpos=current_owner->xpos=0x00088000;
        old_owner->ypos=current_owner->ypos=0x00098000;
        old_owner->creature=AC_HUMAN;
        old_owner->mode=chase_player;
        old_owner->energy=old_owner->oldenergy=0;
        old_owner->flags=1u<<AMP_KILLABLE;
        current_owner->creature=AC_HUMAN;
        current_owner->mode=dummy;
        current_owner->energy=current_owner->oldenergy=150;
        current_owner->flags=1u<<AMP_KILLABLE;
        amp_setgrid(old_owner);
        amp_setgrid(current_owner);
        assert(collmap[9u*AVP_AMP_GRID_W+8u]==2u);
        chase_player(old_owner); /* retail unconditional clear */
        assert(collmap[9u*AVP_AMP_GRID_W+8u]==0u);
        shot.creature=AC_BOLT;
        shot.xpos=current_owner->xpos;
        shot.ypos=current_owner->ypos;
        shot.xvel=-1;
        shot.flags=1u<<AMP_PHIT;
        assert(weapon_hit(&shot)==NULL);
        assert(current_owner->energy==150);
    }
    /* Freeze only hostile AMP CPU modes; retain player, pickups and effects. */
    {
        AvpAmp *enemy=&level1amps[0], *egg=&level1amps[1],
               *pickup=&level1amps[2], *projectile=&level1amps[3],
               *player_amp=&level1amps[4];
        InitAMPs();
        enemy->creature=AC_HUMAN;enemy->mode=counted_mode;enemy->flags=0;
        egg->creature=AC_EGG;egg->mode=counted_mode;egg->flags=0;
        pickup->creature=AC_AMMO1;pickup->mode=counted_mode;pickup->flags=0;
        projectile->creature=AC_BOLT;projectile->mode=counted_mode;projectile->flags=0;
        player_amp->creature=AC_ALIEN;player_amp->flags=1u<<AMP_PLAYER;player_amp->mode=counted_mode;
        enemy->host_static=egg->host_static=pickup->host_static=projectile->host_static=player_amp->host_static=0;
        host_mode_calls=0;
        avp_amp_set_enemies_frozen(1);
        assert(avp_amp_enemies_frozen());
        UpdateAMPs();
        assert(host_mode_calls==3);
        avp_amp_set_enemies_frozen(0);
        UpdateAMPs();
        assert(host_mode_calls==8);
    }
    /* AMP.S::generate starts FX_force once, modulates the alarm in-place,
     * then kills the alarm without ending the independent level ambience. */
    {
        AvpRuntimeOps ops={0};AvpAmp generator={0};
        ops.play_sfx_params=gen_mod;ops.kill_ambient=gen_amb;
        avp_runtime_bind(&ops);
        x_pos=y_pos=0x00120000u;
        generator.xpos=(s32)x_pos;generator.ypos=(s32)y_pos;
        ngens=4u;
        generate(&generator);
        assert(amp_test_alarm_starts==1u&&amp_test_alarm_sound==AVP_FX_FORCE);
        assert(gen_updates==0u&&gen_ambient_kills==0u);
        assert(generator.mode!=NULL);
        generator.mode(&generator);
        assert(gen_updates==1u&&gen_last_volume==0x1a00&&gen_last_pitch==0xe0);
        generator.mode(&generator);
        assert(gen_updates==2u&&amp_test_alarm_starts==1u);
        ngens=0u;
        generator.mode(&generator);
        assert(amp_test_alarm_kills==1u&&gen_ambient_kills==0u);
        assert(generator.mode==NULL&&gen_updates==2u);
        avp_runtime_reset_ops();
    }
    /* Only the ordinary Queen plays ASHRI when starting a melee sequence;
     * AMP.S::qfight calls it after choosing qfightab. */
    {
        AvpRuntimeOps ops={0};AvpAmp q={0};
        ops.play_sfx=queen_attack_sound;avp_runtime_bind(&ops);
        queen_attack_sound_count=0;
        x_pos=y_pos=0x00200000u;
        q.xpos=(s32)x_pos;q.ypos=(s32)y_pos;
        q.energy=q.oldenergy=1000;
        lockxxx(&q);q.timer=0;
        q.mode(&q);
        assert(queen_attack_sound_count==1u);
        assert(q.animseq==AS_FIGHT2);
        avp_runtime_reset_ops();
    }
    /* The retail explosion rate depends on distance, with one playback per
     * AMP tick. A far explosion still plays but inherits the prior rate. */
    {
        AvpRuntimeOps ops={0};AvpAmp gen={0};
        ops.play_sfx_params=explosion_mod;
        avp_runtime_bind(&ops);
        InitAMPs();build_level();
        x_pos=y_pos=0x00200000u;
        explosion_sounds=0;
        (void)explosion((s32)x_pos,(s32)y_pos,0,200,0x10000u);
        assert(explosion_sounds==1u&&explosion_rate==192);
        (void)explosion((s32)x_pos+0x10000,(s32)y_pos,0,200,0x10000u);
        assert(explosion_sounds==1u); /* once per UpdateAMPs tick */
        UpdateAMPs();
        (void)explosion((s32)x_pos+0x10000,(s32)y_pos,0,200,0x10000u);
        assert(explosion_sounds==2u&&explosion_rate==176);
        UpdateAMPs();
        (void)explosion((s32)x_pos+0x100000,(s32)y_pos,0,200,0x10000u);
        assert(explosion_sounds==3u&&explosion_rate==-1);
        gen.flags=(u16)(1u<<AMP_DEADGEN);
        side_shield(&gen,0,0);
        assert(explosion_sounds==4u&&explosion_rate==170);
        avp_runtime_reset_ops();
    }
    /* AMP.S::gofight/launchfire share global loop handles across all enemy
     * Marines.  Repeated attacks must not restart a looping sample, and
     * UpdateAMPs releases it after the original random tick interval. */
    {
        AvpAmp *marine;
        unsigned starts,stops;
        InitAMPs();build_level();
        amp_test_effect_starts=amp_test_effect_stops=0;
        amp_test_last_effect=0; amp_test_last_released=0;
        x_pos=y_pos=0x00120000u; player_type=PT_PREDATOR;
        discflag=0;invisflag=0;
        avp_amp_set_beam_counter(2);
        avp_amp_set_beam_weapon_sample(beam_after_los);
        marine=amp_req();
        marine->xpos=(s32)x_pos;marine->ypos=(s32)y_pos;
        marine->creature=AC_HUMAN;marine->flags=0;
        marine->mode=chase_player;marine->energy=marine->oldenergy=100;
        sampled_beam=2;
        gofight(marine);
        assert(amp_test_effect_starts==1u&&amp_test_last_effect==AVP_FX_PLSX1);
        gofight(marine);
        assert(amp_test_effect_starts==1u); /* second shot reuses pulse loop */
        marine->mode=dummy;
        UpdateAMPs();
        assert(amp_test_effect_stops==0u); /* random=0 => two ticks */
        UpdateAMPs();
        assert(amp_test_effect_stops==1u&&amp_test_last_released!=0u);
        marine->mode=chase_player;
        gofight(marine);
        assert(amp_test_effect_starts==2u); /* loop can start again */
        /* The pulse and flame loops do not share an identifier.  They can
         * both be active without either restarting the other. */
        sampled_beam=3;discflag=0;
        marine->mode=chase_player;marine->animframe=0;
        gofight(marine); /* frame one, no fire yet */
        marine->mode(marine); /* frame two launches four fireballs */
        assert(amp_test_effect_starts==3u&&amp_test_last_effect==AVP_FX_FLAMX);
        starts=amp_test_effect_starts;stops=amp_test_effect_stops;
        /* The original save releases both loops, independently. */
        save_level();
        assert(amp_test_effect_starts==starts);
        assert(amp_test_effect_stops==stops+2u);
        restore_level(); /* clear saved IDs, as AMP.S::restore_level does */
        for(unsigned n=0;n<16u;n++)UpdateAMPs();
        assert(amp_test_effect_stops==stops+2u);
        /* Verify the independent seven-tick minimum flame release without
         * leaving another pulse handle active. */
        InitAMPs();build_level();
        marine=amp_req();
        marine->xpos=(s32)x_pos;marine->ypos=(s32)y_pos;
        marine->creature=AC_HUMAN;marine->flags=0;
        marine->mode=chase_player;marine->energy=marine->oldenergy=100;
        sampled_beam=3;discflag=0;
        gofight(marine);marine->mode(marine);
        assert(amp_test_effect_starts==starts+1u);
        marine->mode=dummy;
        stops=amp_test_effect_stops;
        for(unsigned n=0;n<6u;n++)UpdateAMPs();
        assert(amp_test_effect_stops==stops);
        UpdateAMPs();
        assert(amp_test_effect_stops==stops+1u);
        avp_amp_set_beam_weapon_sample(NULL);
    }
    /* Revisiting a level must retain surviving stationary furniture and
     * uncollected static pickups, not pull props across from other levels.
     * Dead actors must remain corpses (never living enemies), while consumed
     * items must not respawn through the rendering-only host_static bit. */
    {
        static const AvpAmpPlacement props[]={
            {0x1100,0x1100,17}, /* authored static chain */
            {0x1200,0x1100,26}, /* furniture becomes stationary on update */
            {0x1300,0x1100,11}, /* medkit becomes stationary on update */
            {0x1400,0x1100,12}, /* consumed static pickup */
            {0x1500,0x1100,0},  /* dead enemy */
            {-1,0,0}
        };
        static const AvpAmpPlacement empty[]={ {-1,0,0} };
        static const AvpAmpRandomEntry no_random[]={ {-1,0} };
        const AvpAmpSavedPlacement *saved;
        unsigned n=0;
        InitAMPs();
        avp_amp_bind_retail_source_templates_and_random();
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,props);
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,2,empty);
        for(unsigned k=0;k<3;k++){
            avp_amp_bind_random_list(k,1,no_random);
            avp_amp_bind_random_list(k,2,no_random);
        }
        cur_level=1;
        restore_level();
        assert(level1amps[0].host_static); /* chain */
        UpdateAMPs(); /* furniture and pickups enter Jaguar static mode */
        for(unsigned k=0;k<4;k++)assert(level1amps[k].host_static);
        /* Simulate the collector's removal of an ordinary pickup. */
        level1amps[3].mode=NULL;level1amps[3].host_static=0;
        level1amps[4].mode=NULL;level1amps[4].host_static=1;
        level1amps[4].astype=52; /* dead Alien with persistent acid */
        level1amps[4].flags|=(1u<<AMP_DEAD);
        save_level();
        saved=avp_amp_saved_level(1);
        while(n<AVP_NUM_AMPS&&saved[n].x!=-1)n++;
        assert(n==4);
        assert(saved[0].astype==17);
        assert(saved[1].astype==26);
        assert(saved[2].astype==11);
        assert(saved[3].astype==52);
        assert(saved[3].flags&(1u<<AMP_DEAD));
        cur_level=2;restore_level();
        for(unsigned k=0;k<AVP_NUM_AMPS;k++)
            assert(!level1amps[k].host_static); /* no carryover */
        cur_level=1;restore_level();
        assert(level1amps[0].astype==17&&level1amps[0].host_static);
        assert(level1amps[1].astype==26);
        assert(level1amps[2].astype==11);
        assert(level1amps[3].astype==52);
        assert(level1amps[3].mode!=NULL); /* original acid behavior */
        assert(level1amps[3].flags&(1u<<AMP_DEAD));
        assert(collmap[0x11u*AVP_AMP_GRID_W+0x15u]==0u);
        for(unsigned k=4;k<AVP_NUM_AMPS;k++)
            assert(!level1amps[k].mode&&!level1amps[k].host_static);
    }
    /* Randomly spawned pickups must reserve their selected square without
     * masquerading as a combat actor.  Inactive AMP owners must never absorb
     * projectile damage even if a stale collision entry survives. */
    {
        static const AvpAmpPlacement empty[]={{-1,0,0}};
        static const AvpAmpRandomEntry aid[]={{11,2},{0,1},{-1,0}};
        AvpAmp shot={0};
        InitAMPs();
        avp_amp_bind_retail_source_templates_and_random();
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,empty);
        avp_amp_bind_random_list(0,1,aid);
        player_type=PT_HUMAN;cur_level=1;
        build_level();
        ampcount=0;
        spawn_random_calls=0;deterministic_spawn_random=1;
        rand_set();
        deterministic_spawn_random=0;
        assert(ampcount==3u);
        assert(level1amps[0].astype==11&&level1amps[1].astype==11);
        assert(level1amps[2].astype==0);
        assert(collmap[0]==0u);
        assert(collmap[AVP_AMP_GRID_W+1u]==0u);
        assert(collmap[2u*AVP_AMP_GRID_W+2u]==3u);
        x_pos=0x00200000u;y_pos=0x00200000u;
        UpdateAMPs();
        assert(objmap[1]==1u);
        assert(objmap[AVP_AMP_OBJ_ROW_BYTES+3u]==2u);
        shot.xpos=level1amps[0].xpos;
        shot.ypos=level1amps[0].ypos;
        shot.creature=AC_BOLT;
        shot.xvel=-1;
        assert(weapon_hit(&shot)==NULL);
        /* A stale grid owner must not refer to a released, invisible AMP. */
        collmap[0]=1u;
        level1amps[0].host_static=0;
        level1amps[0].mode=NULL;
        level1amps[0].energy=100;
        assert(weapon_hit(&shot)==NULL);
        assert(level1amps[0].energy==100);
    }
    /* Authored medkits occupy no collision cell and have not yet had an
     * UpdateAMPs tick to register in objmap.  Random placement must still
     * avoid them, including after the authored item becomes stationary. */
    {
        static const AvpAmpPlacement authored[]={
            {0x0080,0x0080,11},{-1,0,0}
        };
        static const AvpAmpPlacement empty[]={{-1,0,0}};
        static const AvpAmpRandomEntry random_medkit[]={
            {11,1},{-1,0}
        };
        InitAMPs();
        avp_amp_bind_retail_source_templates_and_random();
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,authored);
        avp_amp_bind_random_list(0,1,random_medkit);
        cur_level=1;player_type=PT_HUMAN;
        deterministic_spawn_random=1;spawn_random_calls=0;
        restore_level();
        deterministic_spawn_random=0;
        assert(level1amps[0].astype==11);
        assert(level1amps[1].astype==11);
        assert((u32)level1amps[0].xpos>>16==0u);
        assert((u32)level1amps[1].xpos>>16==1u);
        assert((u32)level1amps[1].ypos>>16==1u);
        assert(collmap[0]==0u);
        UpdateAMPs();
        assert(objmap[1]==1u);
        assert(objmap[AVP_AMP_OBJ_ROW_BYTES+3u]==2u);
        /* Removed/collected objects do not reserve their old location. */
        InitAMPs();build_level();
        level1amps[0].mode=NULL;
        level1amps[0].host_static=0;
        deterministic_spawn_random=1;spawn_random_calls=0;
        rand_set();
        deterministic_spawn_random=0;
        assert((u32)level1amps[1].xpos>>16==0u);
        assert((u32)level1amps[1].ypos>>16==0u);
        /* Restore the empty authored list for the following independent
         * AMP allocation and destruction regressions. */
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,empty);
    }
    /* Retail AMP.S .xgen2 and ?genmode clear only amp_mode.  Keep the
     * corresponding 1-based grid number, even after species shutdown. */
    {
        static const unsigned defs[]={43u,44u,4u};
        InitAMPs();build_level();
        for(unsigned i=0;i<3;i++){
            AvpAmp *a=amp_req();
            a->xpos=(s32)((8u+i)<<16|0x8000u);
            a->ypos=0x00098000;
            a->mode=avp_amp_builtin_mode_for_def(defs[i]);
            a->creature=(i==2u)?AC_FACEHUG:AC_GEN;
            amp_setgrid(a);
            assert(collmap[9u*AVP_AMP_GRID_W+8u+i]==i+1u);
        }
        ngens=0;
        generate(&level1amps[0]);
        level1amps[0].mode(&level1amps[0]);
        player_type=PT_HUMAN;
        level1amps[1].mode(&level1amps[1]);
        player_type=PT_ALIEN;
        level1amps[2].mode(&level1amps[2]);
        for(unsigned i=0;i<3;i++){
            /* The two generator entries are not released or cleared;
             * the facehugger still follows its own amp_release path. */
            assert(collmap[9u*AVP_AMP_GRID_W+8u+i]==(i<2u?i+1u:0u));
            assert(level1amps[i].mode==NULL&&!level1amps[i].host_static);
        }
    }
    /* An old grid number must not hit a newly allocated actor that now
     * lives just across the cell boundary.  Close pixel distance alone is
     * insufficient for a stale collision owner. */
    {
        AvpAmp shot={0};
        InitAMPs();build_level();
        AvpAmp *victim=amp_req();
        victim->mode=dummy;victim->host_static=0;
        victim->xpos=0x00090400;victim->ypos=0x00088000;
        victim->energy=150;
        collmap[8u*AVP_AMP_GRID_W+8u]=1u; /* obsolete owner */
        collmap[8u*AVP_AMP_GRID_W+9u]=1u; /* current owner */
        shot.creature=AC_BOLT;shot.xvel=-1;
        shot.xpos=0x0008f000;shot.ypos=victim->ypos;
        assert(weapon_hit(&shot)==NULL);
        assert(victim->energy==150);
        shot.xpos=victim->xpos;
        assert(weapon_hit(&shot)==victim);
        assert(victim->energy==50);
    }
    /* An expiring pooled effect must clear only its own stale collision
     * number before its AMP slot is returned to the free list. */
    {
        InitAMPs();build_level();
        make_spark_at(0x00088000,0x00098000,0);
        assert(level1amps[0].mode!=NULL);
        collmap[9u*AVP_AMP_GRID_W+8u]=1u;
        for(unsigned step=0;step<4;step++)UpdateAMPs();
        assert(level1amps[0].mode==NULL);
        assert(collmap[9u*AVP_AMP_GRID_W+8u]==0u);
        assert(amp_req()==&level1amps[0]);
    }
    /* Deathmatch player bullet contacts reuse the original AMP blood effect
     * creator, rather than drawing colored shapes outside Jaguar sprites. */
    {
        const u16 species[]={AC_ALIEN,AC_PREDATOR,AC_HUMAN};
        const u16 blood[]={AC_ABLOOD,AC_PBLOOD,AC_MBLOOD};
        for(unsigned b=0;b<3u;b++){
            AvpAmp victim={0};
            victim.creature=species[b];
            InitAMPs();build_level();
            make_spark_hit_at(0x00088000,0x00098000,&victim);
            assert(level1amps[0].creature==blood[b]);
            assert(level1amps[0].mode!=NULL);
            assert(level1amps[0].energy==2);
            UpdateAMPs();
            assert(level1amps[0].mode!=NULL);
            UpdateAMPs();
            assert(level1amps[0].mode==NULL);
        }
    }
    /* Which vanished level entities return their AMP slots (retail rules). */
    {
        const unsigned defs[]={43u,44u,4u};
        InitAMPs();build_level();
        for(unsigned i=0;i<3;i++){
            AvpAmp *a=amp_req();
            a->xpos=(s32)((8u+i)<<16|0x8000u);
            a->ypos=0x00098000;
            a->mode=avp_amp_builtin_mode_for_def(defs[i]);
            a->creature=(i==2u)?AC_FACEHUG:AC_GEN;
            amp_setgrid(a);
        }
        ngens=0;
        generate(&level1amps[0]);
        level1amps[0].mode(&level1amps[0]);
        player_type=PT_HUMAN;
        level1amps[1].mode(&level1amps[1]);
        player_type=PT_ALIEN;
        level1amps[2].mode(&level1amps[2]);
        /* R272: AMP.S .xgen2 and ?genmode only CLR.L amp_mode -- generator
         * slots stay allocated (not returned to l1_amp_free); only paths that
         * call amp_release (here the facehugger) return their slot. */
        assert(amp_req()==&level1amps[2]);
        {AvpAmp *next=amp_req();
         assert(next!=&level1amps[1]&&next!=&level1amps[0]);}
    }
    /* Spawn candidates use the same ownership rules as movement: an AMP
     * that has left its old cell cannot reserve that cell forever.  Genuine
     * occupants and the retail 0xffff placement sentinel still block. */
    {
        static const AvpAmpRandomEntry one_enemy[]={{0,1},{-1,0}};
        static const AvpAmpPlacement empty[]={{-1,0,0}};
        AvpAmp *old;
        InitAMPs();
        avp_amp_bind_retail_source_templates_and_random();
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,empty);
        avp_amp_bind_random_list(0,1,one_enemy);
        player_type=PT_HUMAN;cur_level=1;
        build_level();
        old=amp_req();
        old->mode=dummy;old->creature=AC_HUMAN;
        old->xpos=0x000a8000;old->ypos=0x000a8000;
        collmap[0]=1u; /* stale owner at (0,0) */
        deterministic_spawn_random=1;spawn_random_calls=0;
        rand_set();
        deterministic_spawn_random=0;
        assert(level1amps[1].astype==0);
        assert((u32)level1amps[1].xpos>>16==0u);
        assert((u32)level1amps[1].ypos>>16==0u);
        assert(collmap[0]==2u);
        /* Explicitly forbidden squares must still be refused. */
        InitAMPs();build_level();
        /* A walkable floor with the retail $ffff placement exclusion must
         * still reject the random spawn even though chasers may enter it. */
        {extern void avp_test_bind_grid_fixture(int);
         avp_test_bind_grid_fixture(1);}
        collmap[0]=0xffffu;
        deterministic_spawn_random=1;spawn_random_calls=0;
        rand_set();
        deterministic_spawn_random=0;
        assert((u32)level1amps[0].xpos>>16==1u);
        assert((u32)level1amps[0].ypos>>16==1u);
        {extern void avp_test_bind_grid_fixture(int);
         avp_test_bind_grid_fixture(0);}
        /* Keep the existing tests' global random-list configuration. */
        {static const AvpAmpRandomEntry none[]={{-1,0}};
         avp_amp_bind_random_list(0,1,none);}
    }
    /* Navigation must not treat a stale collmap slot as an occupied square.
     * Only a live/static actor currently standing in that grid cell blocks
     * an enemy's near-edge movement.  Previously weapon hits validated the
     * owner but chase movement did not, causing phantom obstacles. */
    {
        AvpRuntimeOps ops={0};AvpAmp *enemy,*other;
        ops.safe_pos=amp_always_safe;
        avp_runtime_bind(&ops);
        InitAMPs();build_level();
        enemy=amp_req();other=amp_req();
        enemy->mode=chase_player;enemy->creature=AC_ALIEN;
        enemy->xpos=0x0008f000;enemy->ypos=0x00088000;
        enemy->xvel=enemy->yvel=0x1200;
        enemy->energy=enemy->oldenergy=150;
        enemy->animseq=AS_RUN;
        other->mode=dummy;other->host_static=0;
        other->xpos=0x00118000;other->ypos=0x00128000;
        x_pos=0x000a8000u;y_pos=0x00088000u;
        player_type=PT_HUMAN;invisflag=0;discflag=0;
        collmap[8u*AVP_AMP_GRID_W+9u]=2u; /* old slot at an empty square */
        chase_player(enemy);
        assert((u32)enemy->xpos==0x00090200u);
        /* Inactive slots cannot block either, even at matching coords. */
        enemy->xpos=0x0008f000;enemy->ypos=0x00088000;
        other->xpos=0x00098000;other->ypos=0x00088000;
        other->mode=NULL;other->host_static=0;
        collmap[8u*AVP_AMP_GRID_W+9u]=2u;
        chase_player(enemy);
        assert((u32)enemy->xpos==0x00090200u);
        /* Level placement marks forbidden squares with 0xffff.  They
         * remain blocked regardless of whether the ID is a valid AMP. */
        enemy->xpos=0x0008f000;enemy->ypos=0x00088000;
        collmap[8u*AVP_AMP_GRID_W+9u]=0xffffu;
        chase_player(enemy);
        assert((u32)enemy->xpos==0x0008f000u);
        /* A real occupying actor remains solid. */
        enemy->xpos=0x0008f000;enemy->ypos=0x00088000;
        other->mode=dummy;other->host_static=0;
        collmap[8u*AVP_AMP_GRID_W+9u]=2u;
        chase_player(enemy);
        assert((u32)enemy->xpos==0x0008f000u);
        avp_runtime_reset_ops();
    }
    /* R272: AMP.S has no "align to open door" fallback (the R058 PC-only
     * amp_align_open_door was "not original Jaguar navigation").  An actor
     * whose every chase step is blocked at an open, narrow door takes the
     * ordinary blocked path: amp_random_move draws from the RNG. */
    {
        AvpRuntimeOps ops={0};AvpAmp *enemy;
        ops.safe_pos=amp_door_safe_pos;
        avp_runtime_bind(&ops);
        InitAMPs();build_level();
        enemy=amp_req();
        enemy->creature=AC_ALIEN;enemy->mode=chase_player;
        enemy->xpos=0x0001d000;enemy->ypos=0x00011900;
        enemy->xvel=enemy->yvel=0x1800;
        enemy->energy=enemy->oldenergy=150;
        enemy->animseq=AS_RUN;
        x_pos=0x00048000u;y_pos=0x00018000u;
        player_type=PT_HUMAN;invisflag=0;discflag=0;
        avp_test_open_door=1;
        enemy->yvector=0;
        {int blocked=0;
         for(int i=0;i<40&&!blocked;i++){
             s32 oldx=enemy->xpos,oldy=enemy->ypos;
             chase_player(enemy);
             /* First tick on which the chase made no progress at all. */
             if(enemy->mode!=chase_player||(enemy->xpos==oldx&&enemy->ypos==oldy))blocked=1;
             /* The removed PC realignment slid the perpendicular axis
              * toward the door centre while the along-axis was blocked. */
             assert(!(enemy->xpos==oldx&&enemy->ypos!=oldy&&enemy->mode==chase_player));
         }
         assert(blocked);}
        avp_test_open_door=0;
        avp_runtime_reset_ops();
    }
    /* The retail level placement grid marks the player's spawn cell and
     * other valid floor cells as $ffff to prohibit random spawns.  That is
     * not an enemy/prop owner.  Enemies must cross a physically open archway
     * and enter a player-occupied elevator cell without treating the spawn
     * exclusion bit pattern as an invisible wall.  Actual wall/door checks
     * are still handled by SafePos, and floorless squares remain excluded. */
    {
        extern void avp_test_bind_grid_fixture(int);
        extern void avp_test_floorless_cell(unsigned,unsigned);
        AvpRuntimeOps ops={0};AvpAmp *enemy;
        ops.safe_pos=amp_always_safe;
        avp_runtime_bind(&ops);
        InitAMPs();build_level();
        avp_test_bind_grid_fixture(1);
        enemy=amp_req();enemy->mode=chase_player;
        enemy->creature=AC_ALIEN;
        enemy->xpos=0x0001d000;enemy->ypos=0x00018000;
        enemy->xvel=enemy->yvel=0x1800;
        enemy->energy=enemy->oldenergy=150;
        enemy->animseq=AS_RUN;
        x_pos=0x00048000u;y_pos=0x00018000u;
        player_type=PT_HUMAN;invisflag=0;discflag=0;
        /* $ffff is the player's reserved destination; there is no AMP. */
        collmap[1u*AVP_AMP_GRID_W+2u]=0xffffu;
        for(int k=0;k<12 && (u32)enemy->xpos<0x00020000u;++k)
            chase_player(enemy);
        assert((u32)enemy->xpos>=0x00020000u);
        /* An inaccessible floorless tile remains excluded. */
        InitAMPs();build_level();
        avp_test_bind_grid_fixture(1);
        avp_test_floorless_cell(2u,1u);
        enemy=amp_req();enemy->mode=chase_player;
        enemy->creature=AC_ALIEN;
        enemy->xpos=0x0001d000;enemy->ypos=0x00018000;
        enemy->xvel=enemy->yvel=0x1800;
        enemy->energy=enemy->oldenergy=150;
        enemy->animseq=AS_RUN;
        collmap[1u*AVP_AMP_GRID_W+2u]=0xffffu;
        for(int k=0;k<12;++k)chase_player(enemy);
        assert((u32)enemy->xpos<0x00020000u);
        avp_test_bind_grid_fixture(0);
        avp_runtime_reset_ops();
    }
    /* Retail AMP.S (68000 recoil entry 0x1742e) never overwrites the
     * original eight-phase oldenergy value. All four ordinary recoil-capable
     * creature types must retain that source value on entering knockback.
     * This is a fidelity check, not a new random pain-chance rule. */
    {
        static const u16 species[]={AC_ALIEN,AC_PREDATOR,AC_HUMAN,AC_ACRAWL};
        for(unsigned k=0;k<sizeof(species)/sizeof(species[0]);++k){
            AvpAmp *enemy;
            InitAMPs();build_level();
            enemy=amp_req();
            enemy->creature=species[k];enemy->mode=chase_player;
            enemy->xpos=0x000a8000;enemy->ypos=0x000a8000;
            enemy->energy=120;enemy->oldenergy=150;
            enemy->animseq=AS_RUN;enemy->timer=0;
            x_pos=0x000b8000u;y_pos=0x000a8000u;
            player_type=PT_HUMAN;discflag=0;invisflag=0;
            chase_player(enemy);
            assert(enemy->animseq==AS_KNOCKBACK);
            assert(enemy->oldenergy==150);
            assert(enemy->energy==120);
            assert(enemy->mode!=chase_player);
        }
    }
    /* Retail interruption is state-dependent, not a species percentage:
     * Alien melee checks damage while attacking, Predator melee finishes
     * its authored attack frames instead of calling the recoil handler. */
    {
        AvpAmp *alien,*pred;
        InitAMPs();build_level();
        alien=amp_req();
        alien->creature=AC_ALIEN;alien->mode=chase_player;
        alien->xpos=0x000a8000;alien->ypos=0x000a8000;
        alien->energy=alien->oldenergy=150;
        alien->animseq=AS_RUN;alien->timer=0;
        x_pos=0x000a9000u;y_pos=0x000a8000u;
        player_type=PT_HUMAN;discflag=0;invisflag=0;
        chase_player(alien);
        assert(alien->mode!=chase_player);
        alien->energy=140;
        alien->mode(alien);
        assert(alien->animseq==AS_KNOCKBACK);
        InitAMPs();build_level();
        pred=amp_req();
        pred->creature=AC_PREDATOR;pred->mode=chase_player;
        pred->xpos=0x000a8000;pred->ypos=0x000a8000;
        pred->energy=pred->oldenergy=150;
        pred->animseq=AS_RUN;pred->timer=0;
        chase_player(pred);
        for(unsigned i=0;i<4;i++)pred->mode(pred);
        assert(pred->mode!=chase_player);
        pred->energy=140;
        pred->mode(pred);
        assert(pred->animseq!=AS_KNOCKBACK);
    }
    /* The original chase entry at $156f4 tests 0x480 (1152) sprite
     * pixels before seeking the player. Seeing an actor beyond the
     * threshold does not imply the opening's collision is blocked. */
    {
        AvpRuntimeOps ops={0};AvpAmp *enemy;
        ops.safe_pos=amp_always_safe;
        avp_runtime_bind(&ops);
        InitAMPs();build_level();
        enemy=amp_req();
        enemy->creature=AC_ALIEN;enemy->mode=chase_player;
        enemy->xpos=0x00118000;enemy->ypos=0x00118000;
        enemy->xvel=enemy->yvel=0x1000;
        enemy->energy=enemy->oldenergy=150;
        enemy->animseq=AS_RUN;
        x_pos=(u32)enemy->xpos+((1153u)<<9);
        y_pos=(u32)enemy->ypos;
        chase_player(enemy);
        assert((u32)enemy->xpos==0x00118000u);
        x_pos=(u32)enemy->xpos+((1100u)<<9);
        chase_player(enemy);
        assert((u32)enemy->xpos>0x00118000u);
        avp_runtime_reset_ops();
    }
    /* A dead egg keeps the dead-egg visual template when revisiting its
     * level, but AMP.S::make_dead_egg removed its solid grid owner. Rebuild
     * must not recreate it. Non-solid cocoons likewise have no grid owner. */
    {
        static const AvpAmpPlacement props[]={
            {0x0880,0x0880,3}, /* hatchable egg */
            {0x0980,0x0880,18}, /* cocoon */
            {-1,0,0}
        };
        static const AvpAmpPlacement empty[]={ {-1,0,0} };
        static const AvpAmpRandomEntry no_random[]={ {-1,0} };
        const unsigned egg_cell=8u*AVP_AMP_GRID_W+8u;
        const unsigned cocoon_cell=8u*AVP_AMP_GRID_W+9u;
        InitAMPs();
        avp_amp_bind_retail_source_templates_and_random();
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,props);
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,2,empty);
        for(unsigned k=0;k<3;k++){
            avp_amp_bind_random_list(k,1,no_random);
            avp_amp_bind_random_list(k,2,no_random);
        }
        player_type=PT_HUMAN;cur_level=1;
        restore_level();
        assert(collmap[egg_cell]!=0u);
        assert(collmap[cocoon_cell]==0u);
        level1amps[0].energy=0;
        level1amps[0].ldir=0;
        eggwait(&level1amps[0]);
        assert(level1amps[0].astype==56);
        assert(collmap[egg_cell]==0u);
        save_level();
        cur_level=2;restore_level();
        cur_level=1;restore_level();
        assert(level1amps[0].astype==56);
        assert(level1amps[0].host_static);
        assert(collmap[egg_cell]==0u);
        assert(collmap[cocoon_cell]==0u);
    }
    /* Finished Marine/Predator/Facehugger/Queen corpses and an Alien acid
     * body must persist across repeated level exits, retaining their exact
     * death templates without becoming solid or reviving.  Neither runtime
     * save code nor the renderer is allowed to drop static corpse slots. */
    {
        static const AvpAmpPlacement empty[]={ {-1,0,0} };
        static const AvpAmpRandomEntry no_random[]={ {-1,0} };
        static const s16 corpse_types[]={52,53,54,55,57};
        static const u16 corpse_frames[]={3,2,4,3,2};
        const AvpAmpSavedPlacement *saved;
        InitAMPs();
        avp_amp_bind_retail_source_templates_and_random();
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,empty);
        avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,2,empty);
        for(unsigned k=0;k<3;k++){
            avp_amp_bind_random_list(k,1,no_random);
            avp_amp_bind_random_list(k,2,no_random);
        }
        cur_level=1;restore_level();
        for(unsigned k=0;k<5;k++){
            AvpAmp *a=amp_req();
            a->astype=corpse_types[k];
            a->xpos=(s32)((10u+k)<<16|0x8000u);
            a->ypos=(s32)(8u<<16|0x8000u);
            a->flags=(u16)(1u<<AMP_DEAD);
            a->animseq=AS_DEATH;
            a->animframe=corpse_frames[k];
            a->mode=k==0?dummy:NULL;
            a->host_static=(u8)(k!=0);
        }
        save_level();
        saved=avp_amp_saved_level(1);
        for(unsigned k=0;k<5;k++){
            assert(saved[k].astype==corpse_types[k]);
            assert(saved[k].flags&(1u<<AMP_DEAD));
        }
        assert(saved[5].x==-1);
        for(unsigned pass=0;pass<3;pass++){
            cur_level=2;restore_level();
            save_level(); /* each visited level is saved on exit */
            cur_level=1;restore_level();
            for(unsigned k=0;k<5;k++){
                const AvpAmp *a=&level1amps[k];
                assert(a->astype==corpse_types[k]);
                assert(a->flags&(1u<<AMP_DEAD));
                assert(a->animseq==AS_DEATH);
                assert(a->animframe==corpse_frames[k]);
                assert(a->host_static==(u8)(k!=0));
                assert((a->mode!=NULL)==(k==0)); /* Alien acid only */
                assert(collmap[8u*AVP_AMP_GRID_W+10u+k]==0u);
            }
            save_level();
        }
    }
    /* R188 optional PC cheats: selected-class kills use retail death paths,
     * skip pickups/projectiles/corpses, and never touch other classes. */
    {
        AvpAmp *alien,*marine,*prop,*hug;
        InitAMPs();build_level();
        alien=amp_req();marine=amp_req();prop=amp_req();hug=amp_req();
        *alien=(AvpAmp){0};alien->mode=dummy;alien->creature=AC_ALIEN;alien->energy=80;alien->astype=0;alien->flags=1u<<AMP_KILLABLE;
        *marine=(AvpAmp){0};marine->mode=dummy;marine->creature=AC_HUMAN;marine->energy=80;marine->astype=2;marine->flags=1u<<AMP_KILLABLE;
        *prop=(AvpAmp){0};prop->mode=dummy;prop->creature=AC_AID;prop->energy=80;prop->astype=11;
        *hug=(AvpAmp){0};hug->mode=dummy;hug->creature=AC_FACEHUG;hug->energy=80;hug->astype=4;hug->flags=1u<<AMP_KILLABLE;
        player_energy=500;x_pos=0;y_pos=0;
        assert(avp_amp_dev_kill_enemies(AC_ALIEN)==1u);
        assert(alien->flags&(1u<<AMP_DEAD));assert(marine->energy==80);
        assert(player_energy==500); /* no cheat-induced alien death splash */
        assert(avp_amp_dev_kill_enemies(AC_ALIEN)==0u);
        assert(avp_amp_dev_kill_enemies(-1)==2u);
        assert(marine->flags&(1u<<AMP_DEAD));assert(hug->flags&(1u<<AMP_DEAD));
        assert(prop->energy==80 && prop->mode==dummy);
    }
    /* R189: Facehugger Eggs are their own selector target, distinct from
     * Facehuggers. Use the original dead-egg template and clear the solid
     * collision owner; a cheat may not award score for prior hit flags. */
    {
        AvpAmp *egg,*hug,*other_egg;
        s32 before_score;
        const unsigned cell=8u*AVP_AMP_GRID_W+8u;
        InitAMPs();build_level();
        egg=amp_req();hug=amp_req();other_egg=amp_req();
        *egg=(AvpAmp){0};egg->mode=eggwait;egg->creature=AC_EGG;
        egg->energy=20;egg->astype=3;egg->animframe=0;
        egg->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_PHIT);
        egg->xpos=0x00088000;egg->ypos=0x00088000;
        amp_setgrid(egg);
        assert(collmap[cell]!=0);
        *hug=(AvpAmp){0};hug->mode=dummy;hug->creature=AC_FACEHUG;
        hug->energy=20;hug->astype=4;hug->flags=(1u<<AMP_KILLABLE);
        *other_egg=(AvpAmp){0};other_egg->mode=eggwait;
        other_egg->creature=AC_EGG;other_egg->energy=20;
        other_egg->astype=3;other_egg->flags=(1u<<AMP_KILLABLE);
        before_score=score;
        assert(avp_amp_dev_kill_enemies(AC_FACEHUG)==1u);
        assert(egg->energy==20 && other_egg->energy==20);
        assert(avp_amp_dev_kill_enemies(AC_EGG)==2u);
        assert(egg->energy==0 && egg->astype==56 && egg->animframe==5);
        assert(egg->mode==NULL && egg->host_static==1);
        assert(!(egg->flags&(1u<<AMP_KILLABLE)));
        assert(collmap[cell]==0 && score==before_score);
        assert(other_egg->astype==56 && other_egg->host_static);
        assert(avp_amp_dev_kill_enemies(AC_EGG)==0u);
    }
    /* Kill All also includes a living egg; nonhostile scenery stays put. */
    {
        AvpAmp *egg,*prop;
        InitAMPs();build_level();egg=amp_req();prop=amp_req();
        *egg=(AvpAmp){0};egg->creature=AC_EGG;egg->mode=eggwait;
        egg->energy=20;egg->astype=3;egg->flags=(1u<<AMP_KILLABLE);
        *prop=(AvpAmp){0};prop->creature=AC_AID;prop->mode=dummy;
        prop->energy=100;prop->astype=11;
        assert(avp_amp_dev_kill_enemies(-1)==1u);
        assert(egg->astype==56 && egg->host_static);
        assert(prop->energy==100 && prop->mode==dummy);
    }
    /* Disabling latch or hostile attacks leaves facehuggers chasing, while
     * the original enabled path can still latch after its attack frames. */
    {
        AvpAmp *h;
        InitAMPs();build_level();h=amp_req();
        *h=(AvpAmp){0};h->mode=chase_player;h->creature=AC_FACEHUG;
        h->energy=80;h->oldenergy=80;h->xpos=0x00028000;h->ypos=0x00028000;
        x_pos=(u32)h->xpos;y_pos=(u32)h->ypos;hugkill=0;
        avp_amp_set_facehug_attach_disabled(1);
        for(unsigned k=0;k<8;k++)chase_player(h);
        assert(hugkill==0);
        avp_amp_set_facehug_attach_disabled(0);
        avp_amp_set_attacks_disabled(1);
        for(unsigned k=0;k<8;k++)chase_player(h);
        assert(hugkill==0);
        avp_amp_set_attacks_disabled(0);
        h->xpos=(s32)x_pos;h->ypos=(s32)y_pos;h->mode=chase_player;
        chase_player(h);
        assert(h->mode!=chase_player); /* attack began normally */
        avp_amp_set_facehug_attach_disabled(1); /* while airborne/attacking */
        for(unsigned k=0;k<5;k++)if(h->mode)h->mode(h);
        assert(hugkill==0 && h->mode==chase_player);
        avp_amp_set_facehug_attach_disabled(0);
        avp_amp_set_attacks_disabled(0);
    }
    /* If an Alien has already entered a melee animation when the no-attack
     * cheat is enabled, completed attack frames must not inflict damage. */
    {
        AvpAmp *a;
        InitAMPs();build_level();a=amp_req();
        *a=(AvpAmp){0};a->mode=chase_player;a->creature=AC_ALIEN;
        a->energy=100;a->oldenergy=100;a->xpos=0x00028000;a->ypos=0x00028000;
        x_pos=(u32)a->xpos;y_pos=(u32)a->ypos;
        avp_amp_set_attacks_disabled(0);
        chase_player(a);assert(a->mode!=chase_player);
        avp_amp_set_attacks_disabled(1);player_energy=500;
        for(unsigned k=0;k<15;k++)if(a->mode)a->mode(a);
        assert(player_energy==500);
        avp_amp_set_attacks_disabled(0);
    }
    /* RE33 R59: opt-in circular enemy contact reach. The Jaguar default
     * remains a world-aligned square; character turning does not resize it.
     * Exact axial boundary and a 3-4-5 corner case are covered. */
    {
        AvpAmp a={0};
        const s32 bx=0x00028000,by=0x00028000;
        a.xpos=bx;a.ypos=by;
        player_type=PT_HUMAN;
        avp_amp_set_normalize_diagonal_contact_range(0);
        assert(!avp_amp_normalize_diagonal_contact_range());
        x_pos=(u32)(bx+(25<<9));y_pos=(u32)(by+(25<<9));
        player_energy=100;
        assert(avp_amp_acid_contact_player(&a)==1 && player_energy==97);
        avp_amp_set_normalize_diagonal_contact_range(1);
        assert(avp_amp_normalize_diagonal_contact_range());
        player_energy=100;
        assert(avp_amp_acid_contact_player(&a)==0 && player_energy==100);
        /* Exact radius 30: axis and 18/24/30 triangle stay eligible. */
        x_pos=(u32)(bx+(30<<9));y_pos=(u32)by;
        assert(avp_amp_acid_contact_player(&a)==1 && player_energy==97);
        player_energy=100;
        x_pos=(u32)(bx+(18<<9));y_pos=(u32)(by+(24<<9));
        assert(avp_amp_acid_contact_player(&a)==1 && player_energy==97);
        player_energy=100;
        x_pos=(u32)(bx+(19<<9));
        assert(avp_amp_acid_contact_player(&a)==0 && player_energy==100);
        avp_amp_set_normalize_diagonal_contact_range(0);
        assert(avp_amp_acid_contact_player(&a)==1 && player_energy==97);
        x_pos=(u32)(bx+(31<<9));y_pos=(u32)by;
        assert(avp_amp_acid_contact_player(&a)==0);
        avp_amp_set_normalize_diagonal_contact_range(0);
    }
    /* Enemy Facehuggers only begin their latch animation within the chosen
     * reach. With default square corners enabled, the same diagonal starts
     * the original latch at 50/50; with normalization it continues pursuit. */
    {
        AvpAmp *h;
        InitAMPs();build_level();h=amp_req();
        *h=(AvpAmp){0};h->mode=chase_player;h->creature=AC_FACEHUG;
        h->energy=80;h->oldenergy=80;
        h->xpos=0x00028000;h->ypos=0x00028000;
        x_pos=(u32)(h->xpos+(50<<9));y_pos=(u32)(h->ypos+(50<<9));
        player_type=PT_HUMAN;
        avp_amp_set_attacks_disabled(0);
        avp_amp_set_facehug_attach_disabled(0);
        avp_amp_set_normalize_diagonal_contact_range(1);
        chase_player(h);
        assert(h->animseq!=AS_FIGHT1); /* no latch at square-only corner */
        h->xpos=0x00028000;h->ypos=0x00028000;h->mode=chase_player;
        avp_amp_set_normalize_diagonal_contact_range(0);
        chase_player(h);
        assert(h->mode!=chase_player);
        avp_amp_set_normalize_diagonal_contact_range(0);
    }
    return 0;}
