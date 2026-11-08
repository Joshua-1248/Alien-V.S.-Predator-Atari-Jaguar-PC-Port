/* Original-engine, one-process multiworld/actor binding regression. A live
 * game loop, audio and sprites are intentionally NOT emulated by this test. */
#include "local_mp_native_context.h"
#include "local_mp_native_input.h"
#include "collision.h"
#include "doors.h"
#include "maze.h"
#include "player.h"
#include "weapons.h"
#include "joypad.h"
#include "amp.h"
#include "avp_runtime.h"
#include "hud.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
/* Globals in unlinked original hardware, levels, and MAZE.S. */
s16 cur_level,new_level;
u8 panel_list[AVP_MAX_PANELS];
u32 levfx_ID;
u16 maze_width,maze_height;
u32 x_pos,y_pos,centre_angle;
s32 sin_ang,cos_ang;
volatile u32 joy_cur,joy_edge,frame_counter;
void (*x_read)(void);
s16 alien_bite, medpak, show_mt;
uintptr_t savegame;
AvpAmmoInfo ammo_info[6];
/* Link-only functions: not used by the tested original allocator or weapon
 * animation transitions. No fake physics or artificial AI is executed. */
const AvpRuntimeOps *avp_runtime_ops(void){return NULL;}
s32 cos_d0(u16 a){(void)a;return 0;}
u32 avp_music_placed_voice(unsigned a,unsigned b){(void)a;(void)b;return 0;}
u32 TFX(unsigned a){(void)a;return 0;}
void StopLoop(u32 a){(void)a;}

typedef struct TestCtx {
    AvpMpNativeWorld *world;
    unsigned actor_calls[4], world_calls;
    int fail_actor;
} TestCtx;

static int actor_step(void *u,unsigned p)
{
    TestCtx *t=(TestCtx *)u;
    u8 collision;
    t->actor_calls[p]++;
    if(cur_level!=(s16)t->world->map->level || maze_width!=5u || maze_height!=5u ||
       avp_collision_maze_data()!=t->world->map->bytes+8u ||
       avp_collision_maze_width()!=5u || avp_collision_maze_height()!=5u)
        return 0;
    /* The original Jaguar collision function runs against the selected
     * level: no generated geometry or alternative movement rules. */
    collision=SafePos(64,(2<<16)|0x8000,(2<<16)|0x8000,40);
    if(collision!=AVP_COLL_SAFE) return 0;
    player_energy=(s16)(player_energy+5);
    ammo_info[0].cur=(u16)(ammo_info[0].cur+1u);
    update_wep(); /* original MAZESCRN.S, non-firing weapon frame */
    return t->fail_actor!=(int)p;
}

static int world_step(void *u)
{
    TestCtx *t=(TestCtx *)u;
    AvpAmp *a=amp_req(); /* ORIGINAL AMP.S allocator, once per world */
    if(a!=&level1amps[0])return 0;
    a->creature=(u16)t->world->map->level;
    a->host_static=1;
    t->world_calls++;
    ++counttime; /* world clock / countdown changed only in world callback */
    inventory_table[2]=(u8)(inventory_table[2]+1u);
    SaveDoors(); /* ORIGINAL DOORS.S, once per shared level */
    return 1;
}

int main(void)
{
    static const s16 types[4]={PT_HUMAN,PT_ALIEN,PT_PREDATOR,PT_ALIEN};
    uint8_t host_door_maze[5*5*8]={0},host_collision_maze[5*5*8]={0};
    uint8_t map_bytes[2][8+5*5*8]={{0}};
    AvpMpRetailMap maps[2]={{0}};
    AvpMpNativeWorld worlds[2]={0},bad={0};
    AvpMpNativeScope scope={0},nested={0};
    AvpMpActorNativeState host_actor,post_actor,actors[AVP_LOCAL_MP_MAX_ACTORS],*selected[AVP_LOCAL_MP_MAX_ACTORS]={0};
    AvpMpDoorEngineState host_doors,post_doors;
    AvpMpAmpNativeState host_amp,post_amp;
    TestCtx ctx={0};
    AvpMpInputConfig profiles;
    AvpMpPlayerCommand cmd={0};
    unsigned i,p;
    cur_level=9;
    frame_counter=900u;
    counttime=33;counting=1;ticktime=870;
    maze_width=5;maze_height=5;
    avp_doors_bind_maze(host_door_maze,5,5);
    InitDoors();
    initialise();
    CHECK(avp_mp_doors_native_capture(&host_doors));
    CHECK(avp_mp_amp_native_capture(&host_amp));
    avp_collision_bind_maze(host_collision_maze,5,5);
    player_type=PT_HUMAN;player_energy=900;
    num_weps=4;cur_weps=0x1eu;cur_wepno=1;new_wepno=0;
    new_weapon();
    ammo_info[0].cur=33;
    CHECK(avp_mp_actor_native_capture(&host_actor));
    for(p=0;p<4u;p++) {
        player_type=types[p];player_energy=(s16)(100+p*100);
        cur_weps=0x7eu;cur_wepno=types[p]==PT_PREDATOR?4:1;new_wepno=0;
        num_weps=types[p]==PT_HUMAN?4:types[p]==PT_ALIEN?3:6;
        new_weapon();
        ammo_info[0].cur=(u16)(20u+p);
        x_pos=(p+1u)*65536u;y_pos=(p+2u)*65536u;
        CHECK(avp_mp_actor_native_capture(&actors[p]));
        selected[p]=&actors[p];
    }
    CHECK(avp_mp_actor_native_restore(&host_actor));
    avp_mp_input_defaults(&profiles);
    /* P2 Alien: Option+left is source-defined turning, with independent
     * right sidestep through the EXISTING host movement bridge. */
    cmd.actions=(1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)|
                (1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_FORWARD);
    cmd.mouse_dx=120;
    actors[1].player.joy_cur=UINT32_MAX;
    CHECK(avp_mp_native_input_prepare(&actors[1],&cmd,&profiles.player[1]));
    CHECK(!(actors[1].player.joy_cur&(1u<<OPTION)) &&
          !(actors[1].player.joy_cur&(1u<<JOY_LEFT)) &&
          !(actors[1].player.joy_cur&(1u<<JOY_UP)) &&
          !(actors[1].player.joy_cur&(1u<<FIRE_C)));
    CHECK(actors[1].player.host_keyboard_strafe_during_turn==1);
    CHECK(actors[1].player.host_mouse_aim && actors[1].player.host_aim_turn>0);
    CHECK(!(actors[1].player.joy_edge&(1u<<JOY_UP)));
    CHECK(avp_mp_native_input_prepare(&actors[1],&cmd,&profiles.player[1]));
    CHECK((actors[1].player.joy_edge&(1u<<JOY_UP))!=0); /* held, not edge */
    cmd.actions=0;cmd.mouse_dx=0;
    CHECK(avp_mp_native_input_prepare(&actors[1],&cmd,&profiles.player[1]));
    CHECK((actors[1].player.joy_cur&(1u<<JOY_UP))!=0);
    /* Nonalien turning uses FIRE_C instead of OPTION; no Predator cloak. */
    actors[2].player.joy_cur=UINT32_MAX;
    cmd.actions=(1u<<AVP_MP_TURN_RIGHT)|(1u<<AVP_MP_PRED_VISION)|
                (1u<<AVP_MP_PRED_MEDKIT)|(1u<<AVP_MP_FIRE);
    CHECK(avp_mp_native_input_prepare(&actors[2],&cmd,&profiles.player[2]));
    CHECK(!(actors[2].player.joy_cur&(1u<<FIRE_C)) &&
          !(actors[2].player.joy_cur&(1u<<JOY_RIGHT)) &&
          !(actors[2].player.joy_cur&(1u<<KEY_6)) &&
          !(actors[2].player.joy_cur&(1u<<KEY_7)) &&
          !(actors[2].player.joy_cur&(1u<<FIRE_A)) &&
          (actors[2].player.joy_cur&(1u<<OPTION)));
    cmd.actions=UINT32_MAX;
    CHECK(!avp_mp_native_input_prepare(&actors[2],&cmd,&profiles.player[2]));
    for(i=0;i<2u;i++) {
        map_bytes[i][1]=5;map_bytes[i][3]=5;
        maps[i].bytes=map_bytes[i];maps[i].size=sizeof(map_bytes[i]);
        maps[i].width=5;maps[i].height=5;
        maps[i].level=3u+i;maps[i].leases=1;
        worlds[i].map=&maps[i];
        worlds[i].pad_frame_counter=75u+i*150u;
        CHECK(avp_mp_hudworld_native_capture(&worlds[i].hud_world));
        worlds[i].hud_world.counttime=(s16)(100u+i*200u);
        CHECK(avp_mp_doors_world_init(&worlds[i].doors,map_bytes[i]+8,5,5,3u+i));
        worlds[i].amp=host_amp;
        worlds[i].amp.free_count=AVP_NUM_AMPS;
        for(unsigned n=0;n<AVP_NUM_AMPS;n++)
            worlds[i].amp.free_list[n]=(u16)(AVP_NUM_AMPS-1u-n);
        CHECK(avp_mp_native_world_valid(&worlds[i]));
    }
    CHECK(!avp_mp_native_world_enter(&scope,NULL));
    bad=worlds[0];bad.map=NULL;
    CHECK(!avp_mp_native_world_enter(&scope,&bad));
    bad=worlds[0];bad.doors.width=4;
    CHECK(!avp_mp_native_world_enter(&scope,&bad));
    /* Shared level: two player actor ticks, one original world-allocator tick. */
    ctx.world=&worlds[0];ctx.fail_actor=-1;
    CHECK(avp_mp_native_world_enter(&scope,&worlds[0]));
    CHECK(!avp_mp_native_world_enter(&nested,&worlds[1]));
    CHECK(!avp_mp_native_world_leave(&nested,&worlds[1]));
    CHECK(avp_mp_native_actor_enter(&scope,selected[0]));
    CHECK(!avp_mp_native_actor_enter(&scope,selected[1]));
    CHECK(!avp_mp_native_world_leave(&scope,&worlds[0]));
    CHECK(avp_mp_native_actor_leave(&scope,selected[0]));
    CHECK(!avp_mp_native_actor_leave(&scope,selected[0]));
    CHECK(avp_mp_native_world_leave(&scope,&worlds[0]));
    CHECK(avp_mp_native_run_world(&worlds[0],selected,0x3u,actor_step,world_step,&ctx));
    CHECK(ctx.actor_calls[0]==1u && ctx.actor_calls[1]==1u && ctx.world_calls==1u);
    CHECK(worlds[0].pad_frame_counter==76u && frame_counter==900u);
    CHECK(worlds[0].hud_world.counttime==101 && counttime==33);
    CHECK(worlds[0].amp.actors[0].creature==3 && worlds[0].amp.inventory_table[2]==1);
    /* Independent level, same serialized engine; other participants tick. */
    ctx.world=&worlds[1];ctx.world_calls=0;
    CHECK(avp_mp_native_run_world(&worlds[1],selected,0xcu,actor_step,world_step,&ctx));
    CHECK(ctx.actor_calls[2]==1u && ctx.actor_calls[3]==1u && ctx.world_calls==1u);
    CHECK(worlds[1].pad_frame_counter==226u && frame_counter==900u);
    CHECK(worlds[1].hud_world.counttime==301 && counttime==33);
    CHECK(worlds[1].amp.actors[0].creature==4 && worlds[1].amp.inventory_table[2]==1);
    CHECK(worlds[0].amp.actors[0].creature==3 && worlds[0].amp.inventory_table[2]==1);
    for(p=0;p<4u;p++) CHECK(actors[p].player.player_energy==(s16)(105+p*100+(p==0?0:0)));
    /* Revisit world 3: its original allocator still returns a NEW slot. */
    ctx.world=&worlds[0];ctx.world_calls=0;
    CHECK(avp_mp_native_run_world(&worlds[0],selected,0x3u,actor_step,world_step,&ctx)==0);
    /* Original amp_req returns index 1, unlike the first tick's index 0.
       The failed callback is not replayed; native states remain owned. */
    CHECK(ctx.world_calls==0u);
    CHECK(avp_mp_amp_native_capture(&post_amp));
    CHECK(memcmp(&post_amp,&host_amp,sizeof(host_amp))==0);
    CHECK(avp_mp_doors_native_capture(&post_doors));
    CHECK(memcmp(&post_doors,&host_doors,sizeof(host_doors))==0);
    CHECK(avp_mp_actor_native_capture(&post_actor));
    CHECK(memcmp(&post_actor,&host_actor,sizeof(host_actor))==0);
    CHECK(avp_collision_maze_data()==host_collision_maze &&
          avp_collision_maze_width()==5u && avp_collision_maze_height()==5u);
    CHECK(cur_level==9 && maze_width==5 && maze_height==5);
    CHECK(!avp_mp_native_run_world(&worlds[0],selected,0,actor_step,world_step,&ctx));
    CHECK(!avp_mp_native_run_world(&worlds[0],selected,0x10u,actor_step,world_step,&ctx));
    CHECK(avp_mp_native_world_enter(&scope,&worlds[0]));
    CHECK(!avp_mp_native_world_leave(&scope,&worlds[1]));
    CHECK(avp_mp_native_world_leave(&scope,&worlds[0]));
    /* Sweep 65: factory for a real, already loaded Jaguar level, rather than
     * a hand-assembled doors/AMP context. The private map copy retains the
     * actual current door cells; snapshots keep the original access metadata. */
    {
        uint8_t private_bytes[8+5*5*8]={0};
        AvpMpRetailMap live_map={0};
        AvpMpNativeWorld captured={0},unchanged={0};
        AvpMpDoorEngineState active_doors,after_doors;
        uint8_t saved_cell;
        live_map.bytes=private_bytes;
        live_map.size=sizeof(private_bytes);
        live_map.width=live_map.height=5u;
        live_map.level=9u;
        live_map.leases=1u;
        private_bytes[1]=5u;private_bytes[3]=5u;
        avp_doors_bind_maze(host_collision_maze,5,5);
        InitDoors();
        CHECK(avp_mp_doors_native_capture(&active_doors));
        host_collision_maze[32]=0x76u;
        CHECK(avp_mp_native_capture_active_world(&captured,&live_map));
        CHECK(avp_mp_native_world_valid(&captured));
        CHECK(private_bytes[8+32]==0x76u);
        CHECK(captured.doors.maze==private_bytes+8 &&
              captured.doors.level_meta==active_doors.level_meta &&
              captured.doors.current_meta==active_doors.current_meta &&
              captured.doors.access_level==active_doors.access_level);
        CHECK(avp_mp_doors_native_capture(&after_doors));
        CHECK(memcmp(&active_doors,&after_doors,sizeof(active_doors))==0);
        CHECK(avp_mp_native_world_enter(&scope,&captured));
        CHECK(avp_collision_maze_data()==private_bytes+8);
        CHECK(avp_mp_native_world_leave(&scope,&captured));
        CHECK(avp_collision_maze_data()==host_collision_maze);
        CHECK(avp_mp_doors_native_capture(&after_doors));
        CHECK(memcmp(&active_doors,&after_doors,sizeof(active_doors))==0);
        unchanged=captured;saved_cell=private_bytes[8+32];
        live_map.level=8u;
        CHECK(!avp_mp_native_capture_active_world(&captured,&live_map));
        CHECK(memcmp(&captured,&unchanged,sizeof(captured))==0 &&
              private_bytes[8+32]==saved_cell);
        live_map.level=9u;live_map.leases=0u;
        CHECK(!avp_mp_native_capture_active_world(&captured,&live_map));
        CHECK(memcmp(&captured,&unchanged,sizeof(captured))==0);
    }
    puts("PASS: original native worlds, four players, shared-world tick, scoped host restoration, and active retail-level factory");
    return 0;
}
