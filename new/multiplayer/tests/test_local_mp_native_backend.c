/* Genuine native scope -> generic session -> four-frame compositor integration.
 * The callbacks deliberately avoid unbound original renderer/sound/AI; output
 * pixels are TEST ONLY, not a claim of finished multiplayer or original HUD. */
#include "local_mp_native_backend.h"
#include "local_mp_amp_targets.h"
#include "local_mp_commands.h"
#include "collision.h"
#include "doors.h"
#include "maze.h"
#include "player.h"
#include "weapons.h"
#include "joypad.h"
#include "amp.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include "hud.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
s16 cur_level,new_level;
u8 panel_list[AVP_MAX_PANELS];
u32 levfx_ID;
u16 maze_width,maze_height;
u32 x_pos,y_pos,centre_angle;
s32 sin_ang,cos_ang;
volatile u32 joy_cur,joy_edge,frame_counter;
void (*x_read)(void);
s16 alien_bite,medpak,show_mt;
uintptr_t savegame;
AvpAmmoInfo ammo_info[6];
static unsigned acid_sound_events;
static void acid_sound_observer(void *user,unsigned id)
{
    (void)user;
    if(id==AVP_FX_ACIDX)++acid_sound_events;
}
const AvpRuntimeOps *avp_runtime_ops(void)
{
    static const AvpRuntimeOps runtime={.play_sfx=acid_sound_observer};
    return &runtime;
}
s32 cos_d0(u16 a){(void)a;return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
void StartAla(unsigned id){(void)id;}
void KillAlar(void){}
u32 DoEffect(unsigned id){(void)id;return 0;}

u32 avp_music_placed_voice(unsigned a,unsigned b){(void)a;(void)b;return 0;}
u32 TFX(unsigned a){(void)a;return 0;}
void StopLoop(u32 a){(void)a;}

typedef struct Fixture {
    AvpMpNativeWorld worlds[2];
    AvpMpActorNativeState actors[4];
    unsigned tick_actors[4],tick_levels[2],renders[4];
    unsigned post_actors[4], post_observed_world[4];
    unsigned drawn_world;
    unsigned dying_mask;
    unsigned death_view_snapshots[4];
    unsigned snapshot_pre_update_ok;
} Fixture;
static uint32_t frames[4][320u*228u];
static unsigned targeted_mode_visits;
/* Test ONLY: observable original AMP mode-callback ABI, not replacement AI. */
static void targeted_mode(AvpAmp *a)
{
    ++targeted_mode_visits;
    ++a->timer;
    player_energy=(s16)(player_energy-13);
}
static int choose_second(void *user,const AvpAmp *a,unsigned index,
                         unsigned mask,const AvpMpActorNativeState *const actors[AVP_LOCAL_MP_MAX_ACTORS])
{
    (void)user;(void)a;(void)index;
    return (mask&2u)&&actors[1] ? 1 : -1;
}

static int choose_invalid(void *user,const AvpAmp *a,unsigned index,
                          unsigned mask,const AvpMpActorNativeState *const actors[AVP_LOCAL_MP_MAX_ACTORS])
{(void)user;(void)a;(void)index;(void)mask;(void)actors;return -2;}
static int choose_living(void *user,const AvpAmp *a,unsigned index,
                         unsigned mask,const AvpMpActorNativeState *const actors[AVP_LOCAL_MP_MAX_ACTORS])
{
    (void)user;(void)a;(void)index;
    for(unsigned p=0;p<4;p++)if((mask&(1u<<p))&&actors[p]&&
        actors[p]->player.player_energy>0 && !actors[p]->player.game_over)
        return (int)p;
    return -1;
}
static int spawn_actor(void *u,unsigned p,unsigned species,AvpMpActorNativeState *out)
{
    Fixture *f=(Fixture *)u;
    if(p>=4 || species!= (unsigned)(f->actors[p].player.player_type==PT_HUMAN?0:
                 f->actors[p].player.player_type==PT_ALIEN?1:2))return 0;
    *out=f->actors[p];return 1;
}
static int spawn_world(void *u,unsigned campaign,unsigned level,AvpMpNativeWorld *out)
{
    Fixture *f=(Fixture *)u;
    if(campaign!=0 || level<3 || level>4)return 0;
    *out=f->worlds[level-3];return 1;
}
static void destroy_world(void *u,AvpMpNativeWorld *w){(void)u;(void)w;}
static int join_world(void *u,AvpMpNativeWorld *w,unsigned p,AvpMpActorNativeState *a)
{(void)u;(void)p;return avp_mp_native_world_valid(w)&&avp_mp_player_native_valid(&a->player);}
static void leave_world(void *u,AvpMpNativeWorld *w,unsigned p,AvpMpActorNativeState *a)
{(void)u;(void)w;(void)p;(void)a;}
static int prepare_p1(void *u,AvpMpActorNativeState *a,const AvpMpPlayerCommand *c)
{
    (void)u;(void)c;
    /* The actual host will copy its live original Jaguar-pad/PC aim state
     * here. This test deliberately leaves the original P1 bindings intact. */
    return avp_mp_player_native_valid(&a->player);
}
static int step_actor(void *u,unsigned p)
{
    Fixture *f=(Fixture *)u;
    f->tick_actors[p]++;
    player_energy=(s16)(player_energy+2);
    update_wep(); /* original nonfiring MAZESCRN.S command */
    return 1;
}
/* Test the original world's presentation prepass even when the player
 * has no movement step. No replacement AMP simulation is used. */
static int snapshot_dying_views(void *u,unsigned mask)
{
    Fixture *f=(Fixture *)u;
    unsigned w=(unsigned)(cur_level-3);
    if(w>=2 || avp_mp_actor_scope_active())return 0;
    for(unsigned p=0;p<4;p++)if((mask&(1u<<p)) &&
                                  (f->dying_mask&(1u<<p))){
        /* The callback must precede, not follow, the original world tick. */
        f->death_view_snapshots[p]++;
    }
    f->snapshot_pre_update_ok++;
    return 1;
}
static int step_world(void *u)
{
    Fixture *f=(Fixture *)u;
    unsigned w=(unsigned)(cur_level-3);
    AvpAmp *a=amp_req();
    if(w>=2||!a)return 0;
    f->tick_levels[w]++;
    ++counttime;
    a->creature=(u16)cur_level;
    inventory_table[2]++;
    SaveDoors();
    return 1;
}
static int step_post_actor(void *u,unsigned p)
{
    Fixture *f=(Fixture *)u;
    unsigned w=(unsigned)(cur_level-3);
    if(p>=4u||w>=2u)return 0;
    f->post_actors[p]++;
    f->post_observed_world[p]=f->tick_levels[w];
    /* Confirm the REAL player/weapon source globals belong to this actor,
     * with the post-world phase running after its world tick. */
    player_energy=(s16)(player_energy+3);
    ammo_info[0].cur=(u16)(ammo_info[0].cur+1u);
    return 1;
}
static int draw_actor(void *u,AvpMpNativeWorld *w,unsigned p,
                      AvpMpActorNativeState *actor,AvpLocalMpSourceFrame *out)
{
    Fixture *f=(Fixture *)u;
    unsigned i;
    if(p>=4 || cur_level!=(s16)w->map->level ||
       player_energy!=actor->player.player_energy)return 0;
    f->renders[p]++;
    f->drawn_world++;
    for(i=0;i<320u*228u;i++)frames[p][i]=(i>=320u*180u)?
        0x0000a000u+p:0x00001000u+(p<<8)+(unsigned)cur_level;
    out->pixels=frames[p];out->width=320u;out->height=228u;out->pitch=320u;
    return 1;
}
int main(void)
{
    static const s16 types[4]={PT_HUMAN,PT_ALIEN,PT_PREDATOR,PT_HUMAN};
    static uint8_t levelbytes[2][8+5*5*8],hostdoor[5*5*8],hostcollision[5*5*8];
    AvpMpRetailMap maps[2]={{0}};
    Fixture f={0};
    AvpMpNativeBackendHost host={0};
    AvpMpWorldBackend backend;
    AvpMpSession session={0};
    AvpMpInputConfig profiles;
    AvpMpCommandInput inputs={0};
    AvpMpPlayerCommand commands[AVP_LOCAL_MP_MAX_ACTORS]={{0}};
    AvpLocalMpSetup setup;
    AvpLocalMpOutputFrame output;
    AvpMpActorNativeState host_actor,after_actor;
    AvpMpDoorEngineState host_doors,after_doors;
    AvpMpAmpNativeState host_amp,after_amp;
    static uint32_t pixels[640u*456u];
    unsigned p,i,n;
    cur_level=9;maze_width=5;maze_height=5;frame_counter=789u;counttime=42;
    avp_doors_bind_maze(hostdoor,5,5);InitDoors();initialise();
    CHECK(avp_mp_doors_native_capture(&host_doors));
    CHECK(avp_mp_amp_native_capture(&host_amp));
    avp_collision_bind_maze(hostcollision,5,5);
    player_type=PT_HUMAN;player_energy=900;num_weps=4;
    cur_weps=0x1eu;cur_wepno=1;new_wepno=0;new_weapon();
    CHECK(avp_mp_actor_native_capture(&host_actor));
    for(p=0;p<4;p++){
        player_type=types[p];player_energy=(s16)(100+p*100);
        num_weps=types[p]==PT_HUMAN?4:types[p]==PT_ALIEN?3:6;
        cur_weps=0x7eu;cur_wepno=types[p]==PT_PREDATOR?4:1;
        new_wepno=0;new_weapon();
        CHECK(avp_mp_actor_native_capture(&f.actors[p]));
    }
    CHECK(avp_mp_actor_native_restore(&host_actor));
    for(i=0;i<2;i++){
        levelbytes[i][1]=5;levelbytes[i][3]=5;
        maps[i].bytes=levelbytes[i];maps[i].size=sizeof(levelbytes[i]);
        maps[i].width=5;maps[i].height=5;maps[i].level=3+i;maps[i].leases=1;
        f.worlds[i].map=&maps[i];
        f.worlds[i].pad_frame_counter=100u+i*100u;
        CHECK(avp_mp_hudworld_native_capture(&f.worlds[i].hud_world));
        f.worlds[i].hud_world.counttime=(s16)(150u+i*200u);
        CHECK(avp_mp_doors_world_init(&f.worlds[i].doors,
              levelbytes[i]+8,5,5,3+i));
        f.worlds[i].amp=host_amp;
        f.worlds[i].amp.free_count=AVP_NUM_AMPS;
        for(n=0;n<AVP_NUM_AMPS;n++)
            f.worlds[i].amp.free_list[n]=(u16)(AVP_NUM_AMPS-1u-n);
    }
    avp_mp_input_defaults(&profiles);
    {
        AvpMpActorNativeState test_actor=f.actors[2];
        AvpMpPlayerCommand tuned={0};
        tuned.analog_enabled=1; /* same original PC gate as P1 */
        tuned.analog_axis_mask=4u;
        tuned.analog_turn=0.5f;
        tuned.analog_turn_sensitivity=3.0f;
        tuned.analog_turn_speed=2.5f;
        CHECK(avp_mp_native_input_prepare(&test_actor,&tuned,&profiles.player[2]));
        CHECK(test_actor.player.host_analog_aim);
        CHECK(test_actor.player.host_aim_turn==0.5f);
        CHECK(test_actor.player.host_aim_sensitivity==3.0f);
        CHECK(test_actor.player.host_aim_speed==2.5f);
        /* Disabling analog movement must disable analog stick yaw, even if
         * an axis remains assigned (P1 single-player parity). */
        tuned.analog_enabled=0;
        CHECK(avp_mp_native_input_prepare(&test_actor,&tuned,&profiles.player[2]));
        CHECK(!test_actor.player.host_analog_aim);
        CHECK(test_actor.player.host_aim_turn==0.0f);
        tuned.analog_enabled=1;
        /* Unlike S83, D-pad/button turning must receive the same tuning. */
        profiles.player[2].gamepad_turn_sensitivity=3.0f;
        profiles.player[2].gamepad_turn_speed=2.5f;
        tuned.analog_axis_mask=0u;
        tuned.analog_turn=0.0f;
        tuned.actions=(1u<<AVP_MP_TURN_RIGHT);
        tuned.digital_actions=tuned.actions;
        CHECK(avp_mp_native_input_prepare(&test_actor,&tuned,&profiles.player[2]));
        CHECK(test_actor.player.host_digital_turn_sensitivity==3.0f);
        CHECK(test_actor.player.host_digital_turn_speed==2.5f);
        profiles.player[2].gamepad_turn_speed=0.0f;
        CHECK(!avp_mp_native_input_prepare(&test_actor,&tuned,&profiles.player[2]));
        profiles.player[2].gamepad_turn_speed=2.5f;
        CHECK(test_actor.player.host_aim_turn==0.0f);
        CHECK(!test_actor.player.host_analog_motion);
    }
    profiles.player[1].key[AVP_MP_FORWARD]='j';
    inputs.keysyms[5]='j';
    CHECK(avp_mp_commands_take(&profiles,&inputs,4,commands));
    host.user=&f;host.profiles=&profiles;
    host.create_actor=spawn_actor;host.create_level=spawn_world;
    host.destroy_level=destroy_world;host.join_level=join_world;
    host.leave_level=leave_world;host.prepare_player1=prepare_p1;
    host.advance_actor=step_actor;
    host.snapshot_world_presentation=snapshot_dying_views;
    host.advance_level=step_world;host.draw_actor=draw_actor;
    CHECK(avp_mp_native_backend_valid(&host));
    backend=avp_mp_native_backend_ops();
    avp_local_mp_defaults(&setup);setup.enabled=1;setup.players=4;
    setup.species[0]=0;setup.species[1]=1;setup.species[2]=2;setup.species[3]=0;
    setup.level[0]=setup.level[1]=3;setup.level[2]=setup.level[3]=4;
    CHECK(avp_mp_session_start(&session,&setup,&backend,&host));
    CHECK(avp_mp_session_world_count(&session)==2);
    CHECK(avp_mp_session_tick(&session,commands));
    CHECK(f.tick_actors[0]==1&&f.tick_actors[1]==1&&
          f.tick_actors[2]==1&&f.tick_actors[3]==1);
    CHECK(f.tick_levels[0]==1&&f.tick_levels[1]==1);
    CHECK(((AvpMpNativeWorld *)session.worlds[0].context)->pad_frame_counter==101u);
    CHECK(((AvpMpNativeWorld *)session.worlds[1].context)->pad_frame_counter==201u);
    CHECK(frame_counter==789u && counttime==42);
    CHECK(((AvpMpNativeWorld *)session.worlds[0].context)->hud_world.counttime==151);
    CHECK(((AvpMpNativeWorld *)session.worlds[1].context)->hud_world.counttime==351);
    for(p=0;p<4;p++){
        AvpMpActorNativeState *a=(AvpMpActorNativeState *)session.player[p];
        CHECK(a->player.player_energy==(s16)(102+p*100));
    }
    CHECK(((AvpMpNativeWorld *)session.worlds[0].context)->amp.inventory_table[2]==1);
    CHECK(((AvpMpNativeWorld *)session.worlds[1].context)->amp.inventory_table[2]==1);
    output.pixels=pixels;output.width=640;output.height=456;output.pitch=640;
    CHECK(avp_mp_session_compose(&session,&output,0)==4);
    CHECK(f.drawn_world==4);
    CHECK(pixels[0]==0x00001003u);
    CHECK(pixels[640u/2u]==0x00001103u);
    CHECK(pixels[640u*228u+0]==0x00001204u);
    CHECK(pixels[640u*456u-1]==0x0000a003u);
    CHECK(avp_mp_session_move(&session,1,4));
    CHECK(avp_mp_session_world_count(&session)==2);
    CHECK(avp_mp_session_player_world(&session,1)->level==4);
    CHECK(avp_mp_session_tick(&session,commands));
    CHECK(f.tick_levels[0]==2 && f.tick_levels[1]==2);
    CHECK(((AvpMpNativeWorld *)session.worlds[0].context)->pad_frame_counter==102u);
    CHECK(((AvpMpNativeWorld *)session.worlds[1].context)->pad_frame_counter==202u);
    CHECK(frame_counter==789u);
    CHECK(avp_mp_actor_native_capture(&after_actor));
    CHECK(memcmp(&host_actor,&after_actor,sizeof(host_actor))==0);
    CHECK(avp_mp_amp_native_capture(&after_amp));
    CHECK(memcmp(&host_amp,&after_amp,sizeof(host_amp))==0);
    CHECK(avp_mp_doors_native_capture(&after_doors));
    CHECK(memcmp(&host_doors,&after_doors,sizeof(host_doors))==0);
    CHECK(avp_collision_maze_data()==hostcollision && cur_level==9);
    /* S64: one original AMP update tick bound to the explicitly selected
     * ORIGINAL player context; enemy is ticked once, P1 is untouched. */
    CHECK(avp_mp_session_move(&session,1,3));
    {
        AvpMpNativeWorld *w=(AvpMpNativeWorld *)
            avp_mp_session_player_world(&session,1)->context;
        AvpMpNativeScope scope={0};
        AvpMpActorNativeState *roster[AVP_LOCAL_MP_MAX_ACTORS]={
            (AvpMpActorNativeState *)session.player[0],
            (AvpMpActorNativeState *)session.player[1],NULL,NULL};
        AvpMpActorNativeState *first=roster[0],*second=roster[1];
        s16 before0=first->player.player_energy,before1=second->player.player_energy;
        AvpMpAmpNativeState amp_before,amp_targeted;
        AvpMpActorNativeState actor_before,actor_targeted;
        CHECK(avp_mp_native_world_enter(&scope,w));
        AvpAmp *enemy=amp_req();
        CHECK(enemy!=NULL);
        enemy->mode=targeted_mode;enemy->host_static=0;
        enemy->timer=0;enemy->creature=AC_HUMAN;
        CHECK(avp_mp_native_world_leave(&scope,w));
        amp_before=w->amp;
        actor_before=*second;
        /* Bad roster rejected before any original timer/actor modification. */
        CHECK(!avp_mp_amp_step_targets(roster,8u,choose_second,NULL));
        CHECK(!avp_mp_amp_step_targets(roster,3u,choose_second,NULL));
        CHECK(memcmp(&amp_before,&w->amp,sizeof(amp_before))==0);
        /* A policy returning -1 means NO living target: an isolated dead
         * player may not fault the other worlds or redirect attacks to P1.
         * Truly invalid callback returns (-2) must still fail loudly. */
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_amp_step_targets(roster,1u,choose_second,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK(targeted_mode_visits==0u);
        w->amp=amp_before; /* restore snapshot for a separate error case */
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(!avp_mp_amp_step_targets(roster,1u,choose_invalid,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK(targeted_mode_visits==0u);
        w->amp=amp_before;
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_amp_step_targets(roster,3u,choose_second,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK(targeted_mode_visits==1u);
        CHECK(first->player.player_energy==before0);
        CHECK(second->player.player_energy==(s16)(before1-13));
        CHECK(w->amp.actors[2].timer==1u);
        amp_targeted=w->amp;
        actor_targeted=*second;
        /* Multiplayer world's original AMP snapshot must contain an actual
         * progressing PC video-beam sample, not the never-firing zero gate.
         * The selected victim and the original enemy update are unchanged. */
        CHECK(w->amp.beam_counter==avp_mp_amp_host_beam_value(w->pad_frame_counter));
        {
            unsigned even=0,odd=0;
            for(unsigned n=0;n<128;n++){
                if((avp_mp_amp_host_beam_value(n)&7u)==2u){
                    if(n&1u)odd++;else even++;
                }
            }
            /* Original Marines only evaluate combat every second AI tick.
             * The drifting phase must eventually reach both parities. */
            CHECK(even>0u && odd>0u);
        }
        /* Control experiment: reset the SAME world+actor, execute the
         * untouched original single-player UpdateAMPs once, compare every
         * original AMP/actor field. No replacement enemy physics or AI. */
        w->amp=amp_before;
        *second=actor_before;
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_native_actor_enter(&scope,second));
        /* Match the single-player PC's pre-UpdateAMPs beam sample to the
         * world-local sample installed by the multiplayer dispatcher. */
        avp_amp_set_beam_counter(avp_mp_amp_host_beam_value(frame_counter));
        UpdateAMPs();
        CHECK(avp_mp_native_actor_leave(&scope,second));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK(targeted_mode_visits==2u);
        CHECK(memcmp(&amp_targeted,&w->amp,sizeof(amp_targeted))==0);
        CHECK(memcmp(&actor_targeted,second,sizeof(actor_targeted))==0);
        CHECK(avp_mp_actor_native_capture(&after_actor));
        CHECK(memcmp(&host_actor,&after_actor,sizeof(host_actor))==0);
        /* Opt-in path through the REAL native session tick scheduler. */
        host.choose_amp_target=choose_second;
        CHECK(avp_mp_session_tick(&session,commands));
        CHECK(targeted_mode_visits==3u);
        CHECK(first->player.player_energy==(s16)(before0+2));
        CHECK(second->player.player_energy==(s16)(before1-24));
        CHECK(w->amp.actors[2].timer==2u);
        /* Original-frame order: actor updates -> one original AMP/world
         * update -> one isolated weapon/HUD post pass for each participant. */
        host.advance_actor_post_world=step_post_actor;
        CHECK(avp_mp_session_tick(&session,commands));
        for(unsigned k=0;k<4u;k++){
            const AvpMpActiveWorld *actor_world=avp_mp_session_player_world(&session,k);
            CHECK(f.post_actors[k]==1u);
            CHECK(actor_world!=NULL);
            CHECK(f.post_observed_world[k]==f.tick_levels[actor_world->level-3u]);
            CHECK(((AvpMpActorNativeState *)session.player[k])->weapon.ammo_info[0].cur==
                  (u16)(f.actors[k].weapon.ammo_info[0].cur+1u));
        }
        CHECK(avp_mp_actor_native_capture(&after_actor));
        CHECK(memcmp(&host_actor,&after_actor,sizeof(host_actor))==0);
    }
    /* RE27/S78: the player's world can be empty of LIVING players during
     * their independent death fade. Other worlds must continue ticking.
     * This is a real native multiworld AMP session, not a stub callback. */
    CHECK(avp_mp_session_move(&session,1,4));
    CHECK(avp_mp_session_player_world(&session,0)->level==3);
    CHECK(avp_mp_session_player_world(&session,1)->level==4);
    {unsigned before_visits=targeted_mode_visits;
     unsigned before_w0=f.tick_levels[0],before_w1=f.tick_levels[1];
     AvpMpActorNativeState *victim=(AvpMpActorNativeState *)session.player[0];
     AvpMpActorNativeState *survivor=(AvpMpActorNativeState *)session.player[1];
     s16 before_survivor=survivor->player.player_energy;
     victim->player.player_energy=-100;  /* still dead after test actor tick */
     f.dying_mask=1u; /* P1 is visually dying; skip original input in production */
     host.choose_amp_target=choose_living;
     CHECK(avp_mp_session_tick(&session,commands));
     CHECK(!session.faulted);
     CHECK(f.tick_levels[0]==before_w0+1u && f.tick_levels[1]==before_w1+1u);
     CHECK(targeted_mode_visits==before_visits); /* no attack on dead P1 */
     CHECK(survivor->player.player_energy==(s16)(before_survivor+5));
     CHECK(avp_mp_session_player_world(&session,0)->level==3);
     CHECK(avp_mp_session_player_world(&session,1)->level==4);
     CHECK(f.death_view_snapshots[0]==1u);
     CHECK(f.death_view_snapshots[1]==0u);
     CHECK(f.snapshot_pre_update_ok>=2u);
     /* A second death-fade tick must refresh the view AGAIN, even though
      * the dead player's actor state has not advanced at all. */
     CHECK(avp_mp_session_tick(&session,commands));
     CHECK(f.death_view_snapshots[0]==2u);
     CHECK(!session.faulted);
    }
    /* S88: one Alien corpse acid hazard must touch EVERY living player
     * within the original rectangular range, not just the player selected
     * by the enemy's target-lock/LOS policy. Exercise the exact original
     * AMP mode and original PLAYER.S actor scopes in one native world. */
    CHECK(avp_mp_session_move(&session,1,3));
    CHECK(avp_mp_session_move(&session,2,3));
    CHECK(avp_mp_session_move(&session,3,3));
    {
        AvpMpNativeWorld *w=(AvpMpNativeWorld *)
            avp_mp_session_player_world(&session,0)->context;
        AvpMpActorNativeState *roster[AVP_LOCAL_MP_MAX_ACTORS]={0};
        AvpMpNativeScope scope={0};
        s16 previous[4];
        unsigned sfx_before;
        for(unsigned n=0;n<4;n++){
            roster[n]=(AvpMpActorNativeState *)session.player[n];
            CHECK(roster[n]!=NULL);
            roster[n]->player.player_energy=(s16)(200+n*100);
            roster[n]->player.game_over=0;
            roster[n]->player.x_pos=0x000a8000u;
            roster[n]->player.y_pos=0x000b8000u;
        }
        roster[2]->player.x_pos=0x000c8000u;
        roster[3]->player.y_pos=0x000d8000u;
        for(unsigned n=0;n<4;n++)previous[n]=roster[n]->player.player_energy;
        CHECK(avp_mp_native_world_enter(&scope,w));
        {
            AvpAmp *acid=&amp_data[2];
            acid->mode=avp_amp_builtin_mode_for_def(52u);
            acid->astype=52;
            acid->host_static=0;
            acid->creature=AC_ALIEN;
            acid->xpos=0x000a8000;
            acid->ypos=0x000b8000;
            acid->flags=0;
            CHECK(acid->mode!=NULL);
            CHECK(avp_amp_is_acid_puddle(acid));
            acid->host_static=1u;
            CHECK(!avp_amp_is_acid_puddle(acid));
            acid->host_static=0u;
        }
        CHECK(avp_mp_native_world_leave(&scope,w));
        /* Single-player original AMP acid mode, scoped to the Alien
           player, must also leave their health and sound state intact. */
        CHECK(avp_mp_native_world_enter(&scope,w));
        {
            AvpMpActorNativeScope alien_scope={0};
            CHECK(avp_mp_actor_scope_enter(&alien_scope,roster[1]));
            {s16 before=player_energy;
             AvpAmp *acid=&amp_data[2];
             CHECK(acid->mode!=NULL);
             acid->mode(acid);
             CHECK(player_energy==before);
             CHECK((acid->flags&(1u<<AMP_ALTFRAME))==0u);
            }
            CHECK(avp_mp_actor_scope_leave(&alien_scope,roster[1]));
        }
        CHECK(avp_mp_native_world_leave(&scope,w));
        sfx_before=acid_sound_events;
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_amp_step_targets(roster,15u,choose_second,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK(roster[0]->player.player_energy==previous[0]-3);
        /* An Alien player standing directly in the puddle is immune. */
        CHECK(roster[1]->player.player_energy==previous[1]);
        CHECK(roster[2]->player.player_energy==previous[2]);
        CHECK(roster[3]->player.player_energy==previous[3]);
        CHECK(acid_sound_events==sfx_before+1u);
        CHECK((w->amp.actors[2].flags&(1u<<AMP_ALTFRAME))!=0u);
        /* P0 moves off; P1 remains on. Audio doesn't restart and P1
         * continues taking exactly one contact tick, not two. */
        roster[0]->player.x_pos=0x000c8000u;
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_amp_step_targets(roster,15u,choose_living,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK(roster[0]->player.player_energy==previous[0]-3);
        CHECK(roster[1]->player.player_energy==previous[1]);
        CHECK(acid_sound_events==sfx_before+1u);
        CHECK((w->amp.actors[2].flags&(1u<<AMP_ALTFRAME))==0u);
        /* Nobody in contact: stop flag, then a completely different
         * player may enter and take damage without enemy retargeting. */
        roster[1]->player.x_pos=0x000c8000u;
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_amp_step_targets(roster,15u,choose_second,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK((w->amp.actors[2].flags&(1u<<AMP_ALTFRAME))==0u);
        roster[2]->player.x_pos=0x000a8000u;
        roster[3]->player.y_pos=0x000b8000u;
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_amp_step_targets(roster,15u,choose_second,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK(roster[2]->player.player_energy==previous[2]-3);
        CHECK(roster[3]->player.player_energy==previous[3]-3);
        CHECK(acid_sound_events==sfx_before+2u);
        /* A dead player never receives acid while their co-op death fade
         * runs; the other living person sharing the puddle still does. */
        roster[3]->player.player_energy=0;
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_amp_step_targets(roster,15u,choose_living,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK(roster[2]->player.player_energy==previous[2]-6);
        CHECK(roster[3]->player.player_energy==0);
        /* Different-level player may not be damaged by this puddle. */
        CHECK(avp_mp_session_move(&session,2,4));
        {
            s16 away=roster[2]->player.player_energy;
            CHECK(avp_mp_native_world_enter(&scope,w));
            CHECK(avp_mp_amp_step_targets(roster,11u,choose_living,NULL));
            CHECK(avp_mp_native_world_leave(&scope,w));
            CHECK(roster[2]->player.player_energy==away);
        }
        /* All remaining same-world participants are dead: an abandoned
         * puddle still updates its own contact state without a target. */
        roster[0]->player.player_energy=0;
        roster[1]->player.player_energy=0;
        sfx_before=acid_sound_events;
        CHECK(avp_mp_native_world_enter(&scope,w));
        CHECK(avp_mp_amp_step_targets(roster,11u,choose_living,NULL));
        CHECK(avp_mp_native_world_leave(&scope,w));
        CHECK((w->amp.actors[2].flags&(1u<<AMP_ALTFRAME))==0u);
        CHECK(acid_sound_events==sfx_before);
        CHECK(avp_mp_actor_native_capture(&after_actor));
        CHECK(memcmp(&host_actor,&after_actor,sizeof(host_actor))==0);
    }
    avp_mp_session_stop(&session);
    puts("PASS: native multiworld session, Alien player acid immunity, other species acid contact, death isolation, source AMP parity and host restoration");
    return 0;
}
