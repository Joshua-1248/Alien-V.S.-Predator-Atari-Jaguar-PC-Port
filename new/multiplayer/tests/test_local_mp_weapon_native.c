/* Actual original MAZESCRN.S weapon command-stream state and Predator unlock
 * isolation. Only unrelated audio/projectile/math engine dependencies mocked.
 */
#include "local_mp_weapon_native.h"
#include "local_mp_input.h"
#include "local_mp_sprite_pose.h"
#include "player.h"
#include "joypad.h"
#include "maze.h"
#include "music.h"
#include "avp_runtime.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while(0)

/* Only external state used by selected original script entrypoints. */
s16 player_type,player_energy,cheat;
s32 score;
u16 player_vel;
s16 alien_bite;
u8 wep_fire;
s16 medpak;
u16 invisflag;
AvpAmmoInfo ammo_info[6];
volatile u32 joy_edge;
s32 sin_ang,cos_ang;
uintptr_t savegame;

/* Stand-ins for audio/render/projectile services only, not game logic. */
s32 cos_d0(u16 angle) { (void)angle; return 0; }
AvpAmp *player_weapon(s32 a,s32 b,u16 c,u16 d,s32 e,s32 f)
{ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return NULL; }
u32 avp_music_placed_voice(unsigned descriptor,unsigned voice)
{ (void)descriptor;(void)voice; return 0xAABB; }
u32 TFX(unsigned id) { (void)id; return 0; }
static u32 last_killed_weapon_sound;
void StopLoop(u32 id) { (void)id; }
void KillSfx(u32 id) { last_killed_weapon_sound=id; }
AvpAmp *TestSpark(void) { return NULL; }
static AvpAmp *shot_stub(s16 damage,u32 distance,u16 width)
{ (void)damage;(void)distance;(void)width; return NULL; }

static int equal(const AvpMpWeaponEngineState *a,
                 const AvpMpWeaponEngineState *b)
{
    return memcmp(a,b,sizeof(*a))==0;
}

int main(void)
{
    AvpMpWeaponEngineState host,after,players[4],bad;
    AvpMpWeaponScope scope={0},nested={0};
    AvpFirstPersonOverlayState over;
    static const s16 species[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR,PT_HUMAN};
    static const s16 weapons[]={1,2,4,3};

    /* R20 Enhancement: Marine firearm rays reach the authored map wall;
     * original range must be restored when toggled off. Projectiles and
     * both species' melee remain unchanged. */
    player_type=PT_HUMAN;cur_wepno=1;new_weapon();
    avp_mazescrn_set_unlimited_shooting_range(0);fire_wep();
    CHECK(fire_distance==0x88000u);
    avp_mazescrn_set_unlimited_shooting_range(1);fire_wep();
    CHECK(fire_distance==0x7fff00u);
    cur_wepno=2;new_weapon();fire_wep();
    CHECK(fire_distance==0x7fff00u);
    cur_wepno=4;new_weapon();fire_wep();
    CHECK(fire_distance==0x7fff00u);
    cur_wepno=3;new_weapon();fire_wep();
    CHECK(fire_distance==0u);
    player_type=PT_ALIEN;cur_wepno=3;new_weapon();fire_wep();
    CHECK(fire_distance==0x9000u);
    player_type=PT_PREDATOR;cur_wepno=4;new_weapon();fire_wep();
    CHECK(fire_distance==0x9000u);
    avp_mazescrn_set_unlimited_shooting_range(0);
    player_type=PT_HUMAN;cur_wepno=1;new_weapon();fire_wep();
    CHECK(fire_distance==0x88000u);

    player_type=PT_HUMAN;
    cur_wepno=1;
    cur_weps=0x1eu;
    num_weps=4;
    new_weapon();
    ammo_info[0].cur=22;
    CHECK(avp_mp_weapon_native_capture(&host));
    CHECK(host.cur_def==avp_weapon_def(PT_HUMAN,1));

    for (unsigned i=0;i<4u;i++) {
        player_type=species[i];
        cur_wepno=weapons[i];
        cur_weps=0x7eu;
        num_weps=species[i]==PT_HUMAN?4:species[i]==PT_ALIEN?3:6;
        new_weapon();
        ammo_info[0].cur=(u16)(30+i);
        medpak=(s16)(100+i);
        CHECK(avp_mp_weapons_native_set_index(i%4u));
        CHECK(avp_mp_weapon_native_capture(&players[i]));
    }
    CHECK(avp_mp_weapon_native_restore(&host));
    player_type=PT_HUMAN;

    /* Tick original weapon scripts in four independent slots without changing
     * any other slot. This is *not* a full player/world gameplay tick. */
    for (unsigned i=0;i<4u;i++) {
        player_type=species[i];
        CHECK(avp_mp_weapon_scope_enter(&scope,&players[i]));
        CHECK(!avp_mp_weapon_scope_enter(&nested,&players[(i+1u)%4u]));
        CHECK(cur_wepno==weapons[i]);
        CHECK(ammo_info[0].cur==(u16)(30+i));
        CHECK(medpak==(s16)(100+i));
        CHECK(avp_mp_weapons_native_get_index()==i%4u);
        avp_mazescrn_get_overlay_state(&over);
        CHECK(over.overlay_id>=0);
        update_wep();
        ++ammo_info[0].cur;
        CHECK(avp_mp_weapon_scope_leave(&scope,&players[i]));
        CHECK(!avp_mp_weapon_scope_leave(&scope,&players[i]));
        player_type=PT_HUMAN;
        CHECK(avp_mp_weapon_native_capture(&after));
        CHECK(equal(&host,&after));
    }
    for (unsigned i=0;i<4u;i++) {
        CHECK(players[i].ammo_info[0].cur==(u16)(31+i));
        player_type=species[i];
        CHECK(avp_mp_weapon_scope_enter(&scope,&players[i]));
        CHECK(cur_wepno==weapons[i]);
        CHECK(ammo_info[0].cur==(u16)(31+i));
        CHECK(avp_mp_weapon_scope_leave(&scope,&players[i]));
    }
    player_type=PT_HUMAN;
    CHECK(avp_mp_weapon_native_capture(&after));
    CHECK(equal(&host,&after));

    /* RE31 R17: the F2 runtime must apply the live F12 Unlimited Shooting
     * Range toggle before EACH scoped actor's weapon-script tick. The single-
     * player pc_main settings loop is bypassed by the F2 dispatch. Exercise
     * the real original fire_wep() in separately restored actor weapon states.
     * Only Marine hitscan slots 1/2/4 extend; others retain source ranges. */
    for(unsigned i=0;i<4u;i++){
        const AvpWeaponDef *def=avp_weapon_def(species[i],weapons[i]);
        u32 retail;
        CHECK(def!=NULL);
        retail=(u32)def->distance<<8;
        player_type=species[i];
        CHECK(avp_mp_weapon_scope_enter(&scope,&players[i]));
        avp_mazescrn_set_unlimited_shooting_range(1);
        fire_wep();
        CHECK(fire_distance==((species[i]==PT_HUMAN &&
                              (weapons[i]==1||weapons[i]==2||weapons[i]==4))?
                              0x7fff00u:retail));
        avp_mazescrn_set_unlimited_shooting_range(0);
        fire_wep();
        CHECK(fire_distance==retail);
        CHECK(avp_mp_weapon_scope_leave(&scope,&players[i]));
    }
    /* Two different Marine actors may use the same global toggle and must
     * not inherit the prior actor's saved fire_distance or weapon selection. */
    for(unsigned i=0;i<2u;i++){
        unsigned slot=i?3u:0u; /* P4 and P1 */
        player_type=PT_HUMAN;
        CHECK(avp_mp_weapon_scope_enter(&scope,&players[slot]));
        for(s16 w=1;w<=4;w++){
            const AvpWeaponDef *def=avp_weapon_def(PT_HUMAN,w);
            cur_wepno=w;
            new_weapon();
            avp_mazescrn_set_unlimited_shooting_range(1);
            fire_wep();
            CHECK(fire_distance==((w==1||w==2||w==4)?
                                  0x7fff00u:((u32)def->distance<<8)));
            avp_mazescrn_set_unlimited_shooting_range(0);
            fire_wep();
            CHECK(fire_distance==((u32)def->distance<<8));
        }
        CHECK(avp_mp_weapon_scope_leave(&scope,&players[slot]));
    }
    avp_mazescrn_set_unlimited_shooting_range(0);
    player_type=PT_HUMAN;
    CHECK(avp_mp_weapon_native_capture(&after));
    CHECK(equal(&host,&after));

    /* S74: the original PC next/previous weapon request must execute while
     * THAT player's MAZESCRN state is bound. Verify real original cycle
     * selection, not a replacement host-side weapon switch implementation. */
    {
        s16 other=players[1].new_wepno;
        CHECK(avp_mp_weapon_scope_enter(&scope,&players[0]));
        new_wepno=1;
        avp_weapons_cycle(1,4);
        CHECK(new_wepno==2);
        CHECK(avp_mp_weapon_scope_leave(&scope,&players[0]));
        CHECK(players[0].new_wepno==2);
        CHECK(players[1].new_wepno==other);
        CHECK(avp_mp_weapon_scope_enter(&scope,&players[3]));
        new_wepno=3;
        avp_weapons_cycle(-1,4);
        CHECK(new_wepno==2);
        CHECK(avp_mp_weapon_scope_leave(&scope,&players[3]));
        CHECK(players[3].new_wepno==2);
        CHECK(players[1].new_wepno==other);
        CHECK(avp_mp_weapon_native_capture(&after));
        CHECK(equal(&host,&after));
    }
    /* R15: Fast-Equip must retain the exact native move-out/move-in frames
     * and execute them at 2x presentation speed, not skip to frame0.
     * Both runs start from an IDENTICAL settled weapon snapshot. */
    {
        AvpMpWeaponEngineState ready,step_state;
        unsigned elapsed[2]={0,0};
        player_type=PT_HUMAN;
        CHECK(avp_mp_weapon_native_restore(&host));
        cur_wepno=1;new_wepno=0;wep_fire=0;wep_desel=0;
        new_weapon();
        for(unsigned t=0;t<35u;t++){
            CHECK(avp_mp_weapon_native_capture(&step_state));
            if(!step_state.wep_animptr)break;
            update_wep();
        }
        CHECK(avp_mp_weapon_native_capture(&ready));
        CHECK(!ready.wep_animptr && ready.wep_y==0);
        for(unsigned mode=0;mode<2u;mode++){
            CHECK(avp_mp_weapon_native_restore(&ready));
            new_wepno=2;
            avp_mazescrn_mp_fast_equip((int)mode);
            for(unsigned tick=1;tick<=70u;tick++){
                update_wep();
                CHECK(avp_mp_weapon_native_capture(&step_state));
                if(tick==1u){
                    CHECK(step_state.cur_wepno==1);
                    CHECK(step_state.wep_animptr!=NULL);
                }
                if(cur_wepno==2 && !step_state.wep_animptr && !wep_desel){
                    elapsed[mode]=tick;
                    CHECK(step_state.wep_y==0);
                    break;
                }
            }
            CHECK(elapsed[mode]>0);
        }
        CHECK(elapsed[1]>1u);
        CHECK(elapsed[1]==(elapsed[0]+1u)/2u);
        avp_mazescrn_mp_fast_equip(0);
        /* A firing weapon's owned sound must stop immediately at death. */
        CHECK(avp_mp_weapon_native_restore(&ready));
        wep_fire=1;
        update_wep();
        CHECK(avp_mp_weapon_native_capture(&step_state));
        CHECK(step_state.wepfxID==0xAABBu);
        avp_mazescrn_mp_stop_weapon_audio();
        CHECK(last_killed_weapon_sound==0xAABBu);
        CHECK(wep_fire==0 && wait_state==0);
        CHECK(avp_mp_weapon_native_capture(&step_state));
        CHECK(step_state.wepfxID==0);
        CHECK(avp_mp_weapon_native_restore(&host));
    }
    /* R18: all Predator combat weapons remain usable in Deathmatch even
     * when original honour has fallen below its single-player thresholds.
     * With the host modifier OFF, the original revocation stays intact. */
    {
        player_type=PT_PREDATOR;
        cur_weps=0x3eu;
        CHECK(avp_mp_weapons_native_set_index(3u));
        score=0;
        avp_weapons_mp_predator_deathmatch(1);
        PredAvail();
        CHECK((cur_weps & 0x1eu)==0x1eu);
        CHECK(avp_mp_weapons_native_get_index()==3u);
        avp_weapons_mp_predator_deathmatch(0);
        PredAvail();
        CHECK((cur_weps & 0x0eu)==0u); /* retail honour revocation restored */
        CHECK(avp_mp_weapons_native_get_index()==0u);
        player_type=PT_HUMAN;
        score=0;
        CHECK(avp_mp_weapon_native_restore(&host));
    }
    /* R23: the actual original weapon-cycle routine must receive a source
     * NEXT_WEAPON edge from each of the 16 independent actor lanes. The
     * R22 AI-only test passed even though the host dropped P5-P16 actions.
     * A separate original MAZESCRN weapon state is scoped for every bot
     * and the original avp_weapons_cycle() selects its next weapon. */
    {
        uint32_t prior[16]={0};
        AvpMpWeaponEngineState predator[16];
        s16 other=players[2].new_wepno;
        for(unsigned i=0;i<16u;i++)predator[i]=players[2];
        for(unsigned i=0;i<16u;i++){
            int direction=avp_mp_input_weapon_cycle_edge(
                1u<<AVP_MP_NEXT_WEAPON,&prior[i]);
            CHECK(direction==1);
            player_type=PT_PREDATOR;
            CHECK(avp_mp_weapon_scope_enter(&scope,&predator[i]));
            cur_weps=0x1eu;
            new_wepno=4;
            avp_weapons_cycle(direction,4);
            CHECK(new_wepno==1);
            CHECK(avp_mp_weapon_scope_leave(&scope,&predator[i]));
            CHECK(predator[i].new_wepno==1);
        }
        CHECK(players[2].new_wepno==other);
        player_type=PT_HUMAN;
        CHECK(avp_mp_weapon_native_capture(&after));
        CHECK(equal(&host,&after));
    }
    /* Invalid live machine state must NOT contaminate the original host. */
    CHECK(avp_mp_weapon_scope_enter(&scope,&players[0]));
    num_weps=7;
    CHECK(!avp_mp_weapon_scope_leave(&scope,&players[0]));
    CHECK(avp_mp_weapon_native_capture(&after));
    CHECK(equal(&host,&after));
    bad=players[0];bad.valid=0;
    CHECK(!avp_mp_weapon_scope_enter(&scope,&bad));
    bad=players[0];bad.cur_def=(const AvpWeaponDef *)(uintptr_t)1;
    CHECK(!avp_mp_weapon_native_restore(&bad));
    bad=players[0];bad.wep_animptr=(const AvpWepCmd *)(uintptr_t)1;
    CHECK(!avp_mp_weapon_native_restore(&bad));
    bad=players[0];bad.pred_unlock_index=9;
    CHECK(!avp_mp_weapon_native_restore(&bad));
    CHECK(!avp_mp_weapon_native_restore(NULL));
    CHECK(!avp_mp_weapon_native_capture(NULL));
    CHECK(avp_mp_weapon_native_capture(&after));
    CHECK(equal(&host,&after));
    /* R64: real MAZESCRN shotgun script, not a synthesized button press.
     * Pre/post UpdtScrOverlays snapshots must detect ammo spent exactly on
     * the FIRE tick, and no extra shot while the weapon animates/reloads. */
    {
        AvpMpWeaponEngineState pre,post;
        unsigned blasts=0;
        player_type=PT_HUMAN;
        cur_wepno=1;
        new_wepno=0;
        cur_weps=0x1eu;
        num_weps=4;
        wep_fire=0;
        new_weapon();
        for(unsigned tick=0;tick<40u;tick++){
            CHECK(avp_mp_weapon_native_capture(&post));
            if(!post.wep_animptr)break;
            update_wep();
        }
        CHECK(avp_mp_weapon_native_capture(&post));
        CHECK(!post.wep_animptr && !post.mp_equip_phase);
        ammo_info[0].cur=7;
        avp_mazescrn_set_shot_callback(shot_stub);
        wep_fire=1;
        for(unsigned tick=0;tick<25u;tick++){
            CHECK(avp_mp_weapon_native_capture(&pre));
            UpdtScrOverlays();
            CHECK(avp_mp_weapon_native_capture(&post));
            {
                unsigned fired=avp_mp_sprite_marine_shot_event(1,1,
                    avp_mp_sprite_marine_weapon_ready(1,post.new_wepno,
                        post.wep_desel,post.mp_equip_phase),
                    pre.cur_wepno==1 && post.cur_wepno==1,
                    pre.ammo_info[0].cur,post.ammo_info[0].cur);
                if(fired){
                    CHECK(pre.ammo_info[0].cur==post.ammo_info[0].cur+1u);
                    ++blasts;
                }else CHECK(pre.ammo_info[0].cur==post.ammo_info[0].cur);
            }
        }
        CHECK(blasts>=1u);
        CHECK(blasts==7u-ammo_info[0].cur);
        CHECK(!avp_mp_sprite_marine_shot_event(1,1,1,1,
                ammo_info[0].cur,ammo_info[0].cur));
        avp_mazescrn_set_shot_callback(NULL);
        wep_fire=0;
        CHECK(avp_mp_weapon_native_restore(&host));
    }
    puts("PASS: real Jaguar shotgun FIRE/ammo event, native weapon script contexts, host restoration, pointer rejection");
    return 0;
}
