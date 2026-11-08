/* Original PLAYER.S player update split; all locomotion/weapon-key logic is
 * the unmodified original player.c. Only external hardware/other modules are
 * substituted for this focused test, never in the real PC executable. */
#include "local_mp_player_native.h"
#include "avp_runtime.h"
#include "collision.h"
#include "doors.h"
#include "maze.h"
#include "amp.h"
#include "music.h"
#include "joypad.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)

extern void MovePlayer(void);
u32 x_pos,y_pos,centre_angle;
s32 sin_ang,cos_ang;
u16 maze_width;
u8 cur_weps,old_weps,acs_level,map_on,show_coords,lift_key;
s16 cur_wepno,new_wepno,cur_level,new_level,hugkill,use_cocoon,show_mt;
AvpAmp *amps_at;
u32 fire_angle,fire_damage,fire_distance,fire_width,swing_x;
volatile u32 joy_cur=UINT32_MAX,joy_edge=UINT32_MAX,frame_counter;
void (*x_read)(void);
uintptr_t savegame;
static unsigned doors,readpads;
void readpad(void) { ++readpads; }
void InitAccess(void){} void ResetAccess(void){} void InitDoors(void){}
void ResetDoors(void){} void InitHUD(void){} void ResetHUD(void){}
void ResetClct(void){} void Collectables(void){} void DoorKeys(void){}
void MoveDoors(void){++doors;}
void make_spark_at(s32 x,s32 y,int blood){(void)x;(void)y;(void)blood;}
void make_spark_hit_at(s32 x,s32 y,const AvpAmp *victim){(void)x;(void)y;(void)victim;}
u8 SafePos(u8 h,s32 x,s32 y,u16 w){(void)h;(void)x;(void)y;(void)w;return AVP_COLL_SAFE;}
int AMPCollisions(s32 x,s32 y){(void)x;(void)y;return 0;}
u32 FireDistance(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 32767;}
void StopLoop(u32 h){(void)h;}
u32 TFX(unsigned id){(void)id;return 0;}
const AvpRuntimeOps *avp_runtime_ops(void){static const AvpRuntimeOps ops={0};return &ops;}
u16 avp_host_mmio_read16(u32 addr){(void)addr;return 0;}

int main(void)
{
    AvpMpPlayerEngineState baseline,original,split,actors[2],host,post;
    AvpMpPlayerScope scope={0};
    unsigned i;
    maze_width=8;player_type=PT_HUMAN;player_energy=900;
    key_lock=1;hugkill=1; /* exclude unlinked combat/interaction dependencies */
    cur_weps=0x0fu;cur_wepno=1;new_wepno=0;
    x_pos=3u<<16;y_pos=3u<<16;centre_angle=0;
    joy_cur=UINT32_MAX & ~(1u<<JOY_UP);joy_edge=joy_cur;
    seed0=0x1245u;seed1=0x4567u;
    InitMove();ResetMove();cos_ang=32767;sin_ang=0;
    CHECK(avp_mp_player_native_capture(&baseline));
    UpdatePlayer();
    CHECK(readpads==1 && doors==1);
    CHECK(avp_mp_player_native_capture(&original));
    CHECK(avp_mp_player_native_restore(&baseline));
    readpads=0;doors=0;
    avp_mp_player_native_step_actor();
    CHECK(readpads==0 && doors==0); /* No readpad; no extra door advance */
    avp_mp_player_native_step_world_doors();
    CHECK(doors==1);
    CHECK(avp_mp_player_native_capture(&split));
    CHECK(memcmp(&original,&split,sizeof(original))==0);

    /* Ordinary unlocked gameplay must match too, including PlayerWeapons,
     * original map/level keys and the authored Pain/BGSounds RNG sequence. */
    for(i=0;i<3u;i++) {
        CHECK(avp_mp_player_native_restore(&baseline));
        player_type=i==0?PT_HUMAN:(i==1?PT_ALIEN:PT_PREDATOR);
        key_lock=0;hugkill=1;
        joy_cur=UINT32_MAX & ~(1u<<JOY_UP);
        if(i==1u)joy_cur &= ~(1u<<FIRE_C);
        if(i==2u)joy_cur &= ~(1u<<KEY_7);
        joy_edge=joy_cur;
        InitMove();ResetMove();
        CHECK(avp_mp_player_native_capture(&baseline));
        UpdatePlayer();
        CHECK(avp_mp_player_native_capture(&original));
        CHECK(avp_mp_player_native_restore(&baseline));
        avp_mp_player_native_step_actor();
        avp_mp_player_native_step_world_doors();
        CHECK(avp_mp_player_native_capture(&split));
        CHECK(memcmp(&original,&split,sizeof(original))==0);
    }
    /* Restore the initial Marine actor for the shared-level test below. */
    CHECK(avp_mp_player_native_restore(&baseline));

    /* Two independently sampled players on same level. MoveDoors runs once
     * after both original movement updates, while host state is restored. */
    CHECK(avp_mp_player_native_restore(&baseline));
    CHECK(avp_mp_player_native_capture(&host));
    actors[0]=baseline;actors[1]=baseline;
    actors[0].joy_cur=UINT32_MAX & ~(1u<<JOY_UP);
    actors[1].joy_cur=UINT32_MAX & ~(1u<<JOY_DOWN);
    actors[0].joy_edge=actors[0].joy_cur;
    actors[1].joy_edge=actors[1].joy_cur;
    actors[0].seed1=0x1111;actors[1].seed1=0x2222;
    doors=0;readpads=0;
    for(i=0;i<2u;i++) {
        CHECK(avp_mp_player_scope_enter(&scope,&actors[i]));
        avp_mp_player_native_step_actor();
        CHECK(avp_mp_player_scope_leave(&scope,&actors[i]));
    }
    avp_mp_player_native_step_world_doors();
    CHECK(doors==1 && readpads==0);
    CHECK(actors[0].x_vel!=actors[1].x_vel);
    CHECK(actors[0].seed0==0x1111 && actors[1].seed0==0x2222);
    CHECK(avp_mp_player_native_capture(&post));
    CHECK(memcmp(&host,&post,sizeof(host))==0);
    /* S84 regression: digital controller sensitivity changes acceleration,
     * and speed changes the maximum, while 1x retains Jaguar bit parity.
     * All three actor states borrow the very same original MovePlayer(). */
    {
        AvpMpPlayerEngineState stock, quick, slow, original_host;
        CHECK(avp_mp_player_native_restore(&host));
        player_type=PT_HUMAN;hugkill=0;key_lock=0;use_cocoon=0;
        joy_cur=UINT32_MAX & ~(1u<<FIRE_C) & ~(1u<<JOY_RIGHT);
        joy_edge=joy_cur;
        InitMove();ResetMove();
        cos_ang=32767;sin_ang=0;
        CHECK(avp_mp_player_native_capture(&stock));
        original_host=stock;
        quick=stock;slow=stock;
        quick.host_digital_turn_sensitivity=3.0f;
        quick.host_digital_turn_speed=3.0f;
        slow.host_digital_turn_sensitivity=0.5f;
        slow.host_digital_turn_speed=0.5f;
        for(i=0;i<12u;i++){
            CHECK(avp_mp_player_scope_enter(&scope,&stock));
            MovePlayer();
            CHECK(avp_mp_player_scope_leave(&scope,&stock));
            CHECK(avp_mp_player_scope_enter(&scope,&quick));
            MovePlayer();
            CHECK(avp_mp_player_scope_leave(&scope,&quick));
            CHECK(avp_mp_player_scope_enter(&scope,&slow));
            MovePlayer();
            CHECK(avp_mp_player_scope_leave(&scope,&slow));
        }
        CHECK(quick.ang_vel<stock.ang_vel);
        CHECK(stock.ang_vel<slow.ang_vel);
        /* Heading wraps modulo 65536 after twelve 10-Hz ticks. */
        CHECK(quick.centre_angle!=stock.centre_angle);
        CHECK(slow.centre_angle!=stock.centre_angle);
        CHECK(avp_mp_player_native_capture(&post));
        CHECK(memcmp(&original_host,&post,sizeof(original_host))==0);
    }
    puts("PASS: original UpdatePlayer parity, prepared per-player joy input preserved, two original actor movements, one shared-world door tick, host restored");
    return 0;
}
