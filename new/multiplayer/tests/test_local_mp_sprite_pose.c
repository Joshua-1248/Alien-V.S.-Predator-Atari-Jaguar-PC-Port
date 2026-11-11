#include "local_mp_sprite_pose.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s at %d\n",#x,__LINE__);return 1;}}while(0)
int main(void)
{
    static const unsigned cloak_out[]={0,1,1,1,1,1,1,1,1,1,1,1,2,3,4,5,6};
    static const unsigned cloak_in[]={6,5,4,3,2,0};
    AvpMpSpritePose p;
    p=avp_mp_sprite_stand_pose(2,1);CHECK(p.active && p.sequence==AS_FIGHT5 && p.frame==0);
    p=avp_mp_sprite_stand_pose(2,2);CHECK(p.active && p.sequence==AS_FIGHT3 && p.frame==0);
    CHECK(!avp_mp_sprite_stand_pose(2,3).active);
    CHECK(!avp_mp_sprite_stand_pose(2,4).active);

    /* MAZESCRN.S human_defs order and AMP.S enemy Marine sequences. */
    CHECK(avp_mp_sprite_attack_pose(0,1,0).sequence==AS_FIGHT2); /* shotgun standing */
    CHECK(avp_mp_sprite_attack_pose(0,2,0).sequence==AS_FIGHT1); /* pulse crouched */
    CHECK(avp_mp_sprite_attack_pose(0,3,0).sequence==AS_FIGHT2); /* flame standing */
    CHECK(avp_mp_sprite_attack_pose(0,4,0).sequence==AS_FIGHT1); /* smartgun crouched */
    CHECK(avp_mp_sprite_attack_pose(0,1,2).frame==2);
    CHECK(avp_mp_sprite_attack_pose(0,1,3).frame==1);
    CHECK(!avp_mp_sprite_attack_pose(0,1,4).active);
    CHECK(avp_mp_sprite_attack_pose(0,2,3).frame==2);
    CHECK(avp_mp_sprite_attack_pose(0,4,7).frame==0);
    CHECK(!avp_mp_sprite_attack_pose(0,4,8).active);
    CHECK(!avp_mp_sprite_attack_pose(0,0,0).active);
    CHECK(!avp_mp_sprite_attack_pose(0,5,0).active);

    /* R64: only the three on-shot frontal frames, not the return pose. */
    { const unsigned shotgun[]={0,1,2};
      for(unsigned age=0;age<3u;age++){
          p=avp_mp_sprite_attack_pose_view(0,1,age,1,0);
          CHECK(p.active && p.sequence==AS_FIGHT2 && p.frame==shotgun[age]);
          p=avp_mp_sprite_attack_pose_view(0,1,age,1,1);
          CHECK(p.active && p.sequence==AS_FIGHT2 && p.frame==shotgun[age]);
      }
      CHECK(!avp_mp_sprite_attack_pose_view(0,1,3,1,0).active);
      CHECK(!avp_mp_sprite_attack_pose_view(0,1,3,1,1).active);
      CHECK(!avp_mp_sprite_attack_pose_view(0,1,40,1,1).active);
    }
    p=avp_mp_sprite_attack_pose_view(0,1,2,0,0);
    CHECK(p.active && p.sequence==AS_FIGHT2 && p.frame==2); /* side unchanged */
    p=avp_mp_sprite_attack_pose_view(0,1,3,0,0);
    CHECK(p.active && p.frame==1); /* side unchanged */

    /* Pulse Rifle/Smart Gun: first three FIGHT1 AAF frames loop under
     * held fire, omitting the rising/standing final pose. */
    for(unsigned weapon=2u;weapon<=4u;weapon+=2u){
        for(unsigned age=0;age<48u;age++){
            p=avp_mp_sprite_attack_pose_view(0,weapon,age,1,1);
            CHECK(p.active && p.sequence==AS_FIGHT1 && p.frame==age%3u);
        }
        CHECK(avp_mp_sprite_attack_pose_view(0,weapon,2,1,0).active);
        CHECK(!avp_mp_sprite_attack_pose_view(0,weapon,3,1,0).active);
        /* The rear/side still uses the original crouch playlist. */
        p=avp_mp_sprite_attack_pose_view(0,weapon,3,0,1);
        CHECK(p.active && p.frame==2);
        p=avp_mp_sprite_attack_pose_view(0,weapon,8,0,1);
        CHECK(p.active && p.frame==0);
        CHECK(!avp_mp_sprite_attack_pose_view(0,weapon,8,0,0).active);
    }
    /* Flamethrower loops [0,1,2] without the final return [1] pose. */
    { const unsigned flame[]={0,1,2};
      for(unsigned age=0;age<48u;age++){
          p=avp_mp_sprite_attack_pose_view(0,3,age,1,1);
          CHECK(p.active && p.sequence==AS_FIGHT2 && p.frame==flame[age%3u]);
      }
      CHECK(avp_mp_sprite_attack_pose_view(0,3,2,1,0).active);
      CHECK(!avp_mp_sprite_attack_pose_view(0,3,3,1,0).active);
      CHECK(avp_mp_sprite_attack_pose_view(0,3,4,0,1).frame==0);
      CHECK(!avp_mp_sprite_attack_pose_view(0,3,4,0,0).active);
    }
    /* R62: the retail AAF 18 frontal crouch sheet is staged once per hold:
     * lower [0,1,1], sustained [2,1] without frame 3, rise [1,0]. Do not
     * loop the standing pose while holding, or skip the release animation. */
    for(unsigned weapon=2u;weapon<=4u;weapon+=2u){
        AvpMpMarineCrouchAnim crouch={0,0};
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,100);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_LOWER);
        p=avp_mp_sprite_marine_crouch_pose_present(&crouch,100,0.0,weapon);
        CHECK(p.active && p.sequence==AS_FIGHT1 && p.frame==0);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,101);
        CHECK(avp_mp_sprite_marine_crouch_pose_present(&crouch,101,0.0,weapon).frame==1);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,102);
        /* Entry must visibly reach and HOLD the crouch before looping. */
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_LOWER);
        CHECK(avp_mp_sprite_marine_crouch_pose_present(&crouch,102,0.0,weapon).frame==1);
        CHECK(avp_mp_sprite_marine_crouch_pose_present(&crouch,102,0.95,weapon).frame==1);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,103);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_FIRE);
        for(unsigned tick=103;tick<130;tick++){
            avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,tick);
            p=avp_mp_sprite_marine_crouch_pose_present(&crouch,tick,0.0,weapon);
            CHECK(p.active && p.sequence==AS_FIGHT1);
            CHECK(p.frame==2u); /* 2x / 4x: sampled at whole ticks, both begin at 2. */
            CHECK(p.frame!=0u && p.frame!=3u); /* no mid-burst stand-up */
        }
        /* Exact 20/40 Hz presentation phase samples within a single
         * unchanged 10 Hz simulation tick. Crouch entry/exit stays 10 Hz. */
        { static const double fractions[]={0.05,0.30,0.55,0.80,0.999};
          static const unsigned pulse[]={2,2,1,1,1};
          static const unsigned smart[]={2,1,2,1,1};
          const unsigned *expect=weapon==2u?pulse:smart;
          for(unsigned k=0;k<5;k++){
              p=avp_mp_sprite_marine_crouch_pose_present(&crouch,129,
                  fractions[k],weapon);
              CHECK(p.active && p.sequence==AS_FIGHT1 && p.frame==expect[k]);
          }
          p=avp_mp_sprite_marine_crouch_pose_present(&crouch,129,-1.0,weapon);
          CHECK(p.active && p.frame==2);
          p=avp_mp_sprite_marine_crouch_pose_present(&crouch,129,1.0,weapon);
          CHECK(p.active && p.frame==1);
        }
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,0,0,130);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_RAISE);
        CHECK(avp_mp_sprite_marine_crouch_pose_present(&crouch,130,0.0,weapon).frame==1);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,0,0,131);
        CHECK(avp_mp_sprite_marine_crouch_pose_present(&crouch,131,0.0,weapon).frame==0);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,0,0,132);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_IDLE);
        CHECK(!avp_mp_sprite_marine_crouch_pose_present(&crouch,132,0.0,weapon).active);
        /* Re-press after the release started immediately enters lower. */
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,140);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,142);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,0,0,143);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_RAISE);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,144);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_LOWER);
        CHECK(avp_mp_sprite_marine_crouch_pose_present(&crouch,144,0.0,weapon).frame==0);
        /* Pain, death and weapon changes cancel visual transition. */
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,1,145);
        CHECK(!avp_mp_sprite_marine_crouch_pose_present(&crouch,145,0.0,weapon).active);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,146);
        avp_mp_sprite_marine_crouch_update(&crouch,1u,1,0,147);
        CHECK(!avp_mp_sprite_marine_crouch_pose_present(&crouch,147,0.0,weapon).active);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,148);
        avp_mp_sprite_marine_crouch_update(&crouch,3u,1,0,149);
        CHECK(!avp_mp_sprite_marine_crouch_pose_present(&crouch,149,0.0,weapon).active);
        /* A tap shorter than one native frame must not crouch on release. */
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,1,0,150);
        avp_mp_sprite_marine_crouch_update(&crouch,weapon,0,0,150);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_IDLE);
    }
    CHECK(avp_mp_sprite_marine_crouch_weapon(2));
    CHECK(avp_mp_sprite_marine_crouch_weapon(4));
    CHECK(!avp_mp_sprite_marine_crouch_weapon(1));
    CHECK(!avp_mp_sprite_marine_crouch_weapon(3));
    CHECK(avp_mp_sprite_marine_sustained_weapon(2));
    CHECK(avp_mp_sprite_marine_sustained_weapon(3));
    CHECK(avp_mp_sprite_marine_sustained_weapon(4));
    CHECK(!avp_mp_sprite_marine_sustained_weapon(1));
    CHECK(!avp_mp_sprite_marine_fire_interrupted(100,0,0));
    CHECK(avp_mp_sprite_marine_fire_interrupted(100,0,1)); /* damaged */
    CHECK(avp_mp_sprite_marine_fire_interrupted(0,0,0)); /* fatal hit */
    CHECK(avp_mp_sprite_marine_fire_interrupted(100,1,0)); /* dying */

    /* A weapon is NOT ready during the original move-out or move-in
     * scripts, nor while a different selection is pending. These gates
     * reject held-Fire cosmetic attacks without dropping gameplay input. */
    for(unsigned weapon=1;weapon<=4;weapon++){
        CHECK(avp_mp_sprite_marine_weapon_ready(weapon,0,0,0));
        CHECK(!avp_mp_sprite_marine_weapon_ready(weapon,2,0,0));
        CHECK(!avp_mp_sprite_marine_weapon_ready(weapon,0,1,0));
        CHECK(!avp_mp_sprite_marine_weapon_ready(weapon,0,0,1));
    }
    CHECK(!avp_mp_sprite_marine_weapon_ready(0,0,0,0));
    CHECK(!avp_mp_sprite_marine_weapon_ready(5,0,0,0));
    {
        AvpMpMarineCrouchAnim crouch={0,0};
        /* Held Fire while equipping must NOT begin crouching; as soon as
         * the ready script completes, the original held input is honored. */
        avp_mp_sprite_marine_crouch_update(&crouch,2,1,1,10);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_IDLE);
        avp_mp_sprite_marine_crouch_update(&crouch,2,1,1,11);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_IDLE);
        avp_mp_sprite_marine_crouch_update(&crouch,2,1,0,12);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_LOWER);
        CHECK(avp_mp_sprite_marine_crouch_pose_present(&crouch,12,0.5,2).frame==0);
        avp_mp_sprite_marine_crouch_update(&crouch,2,1,0,14);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_LOWER);
        avp_mp_sprite_marine_crouch_update(&crouch,2,1,0,15);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_FIRE);
        avp_mp_sprite_marine_crouch_update(&crouch,2,1,1,16);
        CHECK(crouch.phase==AVP_MP_MARINE_CROUCH_IDLE);
    }

    /* R64: only completed ammunition transactions count as real shots.
     * Earlier active_wep transitions cannot produce a cosmetic false shot;
     * the final Banana Stops round may refill to a larger number. */
    CHECK(!avp_mp_sprite_marine_shot_event(1,1,1,1,40,40));
    CHECK(avp_mp_sprite_marine_shot_event(1,1,1,1,40,39));
    CHECK(avp_mp_sprite_marine_shot_event(1,1,1,1,1,40));
    CHECK(!avp_mp_sprite_marine_shot_event(1,0,1,1,40,39));
    CHECK(!avp_mp_sprite_marine_shot_event(1,1,0,1,40,39));
    CHECK(!avp_mp_sprite_marine_shot_event(1,1,1,0,40,39));
    CHECK(!avp_mp_sprite_marine_shot_event(2,1,1,1,40,39));
    /* Non-Marine attack timelines remain byte-for-byte source-equivalent. */
    for(unsigned species=1;species<3;species++)
        for(unsigned weapon=1;weapon<5;weapon++)
            for(unsigned age=0;age<20;age++){
                AvpMpSpritePose old=avp_mp_sprite_attack_pose(species,weapon,age);
                p=avp_mp_sprite_attack_pose_view(species,weapon,age,1,1);
                CHECK(p.sequence==old.sequence && p.frame==old.frame &&
                      p.active==old.active);
            }

    /* MAZESCRN.S alien_defs: 1 tail / 2 swipe / 3 bite, never reversed. */
    CHECK(avp_mp_sprite_attack_pose(1,1,0).sequence==AS_FIGHT3);
    CHECK(avp_mp_sprite_attack_pose(1,2,0).sequence==AS_FIGHT2);
    CHECK(avp_mp_sprite_attack_pose(1,3,0).sequence==AS_FIGHT1);
    CHECK(avp_mp_sprite_attack_pose(1,2,1).frame==2);
    CHECK(avp_mp_sprite_attack_pose(1,2,2).frame==3);
    CHECK(avp_mp_sprite_attack_pose(1,2,3).frame==3);
    CHECK(!avp_mp_sprite_attack_pose(1,2,6).active);
    CHECK(avp_mp_sprite_attack_pose(1,1,4).frame==3);
    CHECK(avp_mp_sprite_attack_pose(1,3,3).frame==2);

    /* The AMP.S playlists consume exactly one native 10 Hz tick per frame;
     * held visual repeats obey amp_handalien's five-tick between-attacks
     * recovery without delaying a new fire press or any actual game logic. */
    CHECK(!avp_mp_sprite_attack_rearm(1,0,0,0,100,105));
    CHECK(avp_mp_sprite_attack_rearm(1,1,0,0,100,105));
    CHECK(!avp_mp_sprite_attack_rearm(1,1,1,1,100,105));
    for(unsigned tick=100;tick<105;tick++)
        CHECK(!avp_mp_sprite_attack_rearm(1,1,1,0,tick,105));
    CHECK(avp_mp_sprite_attack_rearm(1,1,1,0,105,105));
    CHECK(avp_mp_sprite_attack_rearm(0,1,1,0,101,105));
    CHECK(avp_mp_sprite_attack_rearm(2,1,1,0,101,105));

    /* Predator source playlist frames consume one source AMP update each. */
    CHECK(avp_mp_sprite_attack_pose(2,1,8).frame==4);
    CHECK(!avp_mp_sprite_attack_pose(2,1,10).active);
    CHECK(avp_mp_sprite_attack_pose(2,2,5).frame==2);
    CHECK(avp_mp_sprite_attack_pose(2,2,8).frame==1);
    CHECK(!avp_mp_sprite_attack_pose(2,2,10).active);
    CHECK(avp_mp_sprite_attack_pose(2,3,5).frame==5);
    CHECK(!avp_mp_sprite_attack_pose(2,3,6).active);
    CHECK(avp_mp_sprite_attack_pose(2,4,5).frame==5);
    CHECK(!avp_mp_sprite_attack_pose(2,4,6).active);

    /* Enemy aliens and predators advance RUN every native 10 Hz tick;
     * enemy Marines chase on alternate ticks. Idlers always hold frame 0. */
    for(unsigned species=0;species<3;species++){
        p=avp_mp_sprite_motion_pose(species,0,99);
        CHECK(p.active && p.frame==0);
        CHECK(p.sequence==(species==0?AS_WALK:AS_STAND));
        p=avp_mp_sprite_motion_pose(species,1,0);
        CHECK(p.sequence==AS_RUN && p.frame==0);
        p=avp_mp_sprite_motion_pose(species,1,1);
        CHECK(p.frame==(species==0?0:1));
        p=avp_mp_sprite_motion_pose(species,1,2);
        CHECK(p.frame==(species==0?1:2));
    }

    /* Exact AMP.S invis_frames, skipping the -2 SFX marker without
     * skipping a tick; appear_frames uses one native frame per tick. */
    for(unsigned i=0;i<sizeof(cloak_out)/sizeof(cloak_out[0]);i++){
        p=avp_mp_sprite_predator_cloak_pose(1,i);
        CHECK(p.active && p.sequence==AS_FIGHT1 && p.frame==cloak_out[i]);
    }
    p=avp_mp_sprite_predator_cloak_pose(1,17);
    CHECK(p.active && p.sequence==AS_HIDDEN);
    for(unsigned i=0;i<sizeof(cloak_in)/sizeof(cloak_in[0]);i++){
        p=avp_mp_sprite_predator_cloak_pose(0,i);
        CHECK(p.active && p.sequence==AS_FIGHT1 && p.frame==cloak_in[i]);
    }
    CHECK(!avp_mp_sprite_predator_cloak_pose(0,6).active);
    CHECK(avp_mp_sprite_predator_pain_pose(0).active);
    CHECK(avp_mp_sprite_predator_pain_pose(1).active);
    CHECK(avp_mp_sprite_predator_pain_pose(1).sequence==AS_KNOCKBACK);
    CHECK(!avp_mp_sprite_predator_pain_pose(2).active);
    /* Marine, Alien, and Predator third-person recoil all use the same two
     * original AMP.S 10 Hz frames rather than long PLAYER.S pain timers. */
    for(unsigned species=0;species<3;species++)
        for(unsigned age=0;age<6;age++){
            AvpMpSpritePose recoil=avp_mp_sprite_enemy_recoil_pose(age);
            CHECK(recoil.active==(age<2u));
            if(age<2u)CHECK(recoil.sequence==AS_KNOCKBACK && recoil.frame==0);
        }
    /* Alien enemy recoil is the same two 10 Hz frames as Predator AMP.S.
     * It must end even if PLAYER.S's first-person pain fade persists. */
    for(unsigned age=0;age<4;age++){
        AvpMpSpritePose recoil=avp_mp_sprite_enemy_recoil_pose(age);
        CHECK(recoil.active==(age<2u));
        if(age<2u)CHECK(recoil.sequence==AS_KNOCKBACK && recoil.frame==0);
    }
    {
        uint8_t valid=0;
        int16_t prev=0;
        uint64_t start=0;
        avp_mp_sprite_enemy_recoil_observe(&valid,&prev,&start,100,0);
        CHECK(valid && prev==100);
        CHECK(!avp_mp_sprite_enemy_recoil_pose((unsigned)(0-start)).active);
        avp_mp_sprite_enemy_recoil_observe(&valid,&prev,&start,90,10);
        CHECK(start==10 && avp_mp_sprite_enemy_recoil_pose(0).active);
        avp_mp_sprite_enemy_recoil_observe(&valid,&prev,&start,90,11);
        CHECK(avp_mp_sprite_enemy_recoil_pose((unsigned)(11-start)).active);
        avp_mp_sprite_enemy_recoil_observe(&valid,&prev,&start,90,12);
        CHECK(!avp_mp_sprite_enemy_recoil_pose((unsigned)(12-start)).active);
        /* A later hit restarts the two-tick timeline; healing does not. */
        avp_mp_sprite_enemy_recoil_observe(&valid,&prev,&start,75,14);
        CHECK(start==14);
        avp_mp_sprite_enemy_recoil_observe(&valid,&prev,&start,80,15);
        CHECK(start==14);
        avp_mp_sprite_enemy_recoil_observe(&valid,&prev,&start,0,16);
        CHECK(start==14);
    }
    CHECK(avp_mp_sprite_roar_pose(5).frame==4);
    CHECK(!avp_mp_sprite_roar_pose(12).active);
    puts("PASS: Marine post-weapon shotgun event, crouch-before-fire, 20/40Hz crouched loops, Alien/Predator parity, cloak, recoil");
    return 0;
}
