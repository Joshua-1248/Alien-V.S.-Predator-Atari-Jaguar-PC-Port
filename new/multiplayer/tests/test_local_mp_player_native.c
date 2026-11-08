/* Original PLAYER.S routine isolation: one process, no extra windows. */
#include "local_mp_player_native.h"
#include "avp_runtime.h"
#include "collision.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

/* Real player.c owns the player machine; these are its independent foreign
 * globals in the PC executable. Stubs here only replace the unlinked owners.
 * No movement, random-number or player snapshot algorithm is mocked. */
u32 x_pos,y_pos,centre_angle;
s32 sin_ang,cos_ang;
u16 maze_width;
u8 cur_weps,old_weps;
s16 cur_wepno,new_wepno;
volatile u32 joy_cur,joy_edge;
void (*x_read)(void);
/* MovePlayer is retained by the original do_move function pointer, but it is
 * deliberately NOT invoked by this partial-state test: collision+AI are not
 * yet isolated. Link-only stand-ins for dependencies of that unused routine. */
s16 hugkill;
u8 lift_key;
const AvpRuntimeOps *avp_runtime_ops(void) { return NULL; }
u8 SafePos(u8 height,s32 xpos,s32 ypos,u16 width)
{ (void)height;(void)xpos;(void)ypos;(void)width;return 0; }
int AMPCollisions(s32 xpos,s32 ypos)
{ (void)xpos;(void)ypos;return 0; }


int main(void)
{
    AvpMpPlayerEngineState host_before,host_after,a,b,bad,inside,slots[4];
    AvpMpPlayerScope scope={0}, nested={0};
    u16 rng_a,rng_b;

    maze_width=8;
    player_type=PT_HUMAN;
    player_energy=713;
    x_pos=1u<<16; y_pos=2u<<16; centre_angle=0x20000000u;
    seed0=0x1032u;seed1=0x4567u;
    joy_cur=0xFFFFFFFEu;joy_edge=0xEEEEEEEEu;
    cur_wepno=2;cur_weps=0x3Fu;
    InitMove();ResetMove();
    avp_player_host_analog_motion(1,0.25f,0.5f);
    CHECK(avp_mp_player_native_capture(&host_before));
    CHECK(host_before.player_type==PT_HUMAN && host_before.acc_shift==4);

    a=host_before;
    a.player_type=PT_ALIEN;
    a.player_energy=777;
    a.x_pos=3u<<16;a.y_pos=2u<<16;
    a.x_vel=40;a.y_vel=20;a.ang_vel=0;
    a.seed0=0x0111u;a.seed1=0x2222u;
    a.host_analog_forward=0.75f;
    a.cur_wepno=1;a.joy_cur=0xAAAAAA00u;

    b=host_before;
    b.player_type=PT_PREDATOR;
    b.player_energy=2500;
    b.x_pos=5u<<16;b.y_pos=4u<<16;
    b.x_vel=400;b.y_vel=300;b.ang_vel=0;
    b.seed0=0x3333u;b.seed1=0x4444u;
    b.host_analog_forward=-0.5f;
    b.cur_wepno=4;b.joy_cur=0xBBBBBB00u;

    CHECK(avp_mp_player_scope_enter(&scope,&a));
    CHECK(!avp_mp_player_scope_enter(&nested,&b));
    CHECK(player_type==PT_ALIEN && player_energy==777 && x_pos==(3u<<16));
    CHECK(joy_cur==0xAAAAAA00u && cur_wepno==1);
    InitMove();
    CHECK(acc_shift==3 && max_speed==17000u);
    TidyMove();
    CHECK(new_pos==((2u*8u+3u)*8u) && player_vel==45u);
    rng_a=avp_random();
    CHECK(rng_a==0x2222u);
    player_energy=700;
    avp_player_host_analog_motion(1,0.8f,-0.25f);
    CHECK(avp_mp_player_scope_leave(&scope,&a));
    CHECK(!avp_mp_player_scope_leave(&scope,&a));
    CHECK(avp_mp_player_native_capture(&host_after));
    CHECK(memcmp(&host_before,&host_after,sizeof(host_before))==0);
    CHECK(a.player_energy==700 && a.host_analog_forward==0.8f);
    CHECK(a.new_pos==152u && a.seed0==0x2222u);

    CHECK(avp_mp_player_scope_enter(&scope,&b));
    CHECK(player_type==PT_PREDATOR && cur_wepno==4);
    InitMove();
    CHECK(acc_shift==4 && max_speed==11900u);
    TidyMove();
    CHECK(new_pos==((4u*8u+5u)*8u) && player_vel==475u);
    rng_b=avp_random();
    CHECK(rng_b==0x4444u);
    player_energy=2400;
    CHECK(avp_mp_player_scope_leave(&scope,&b));
    CHECK(b.player_energy==2400 && b.new_pos==296u);
    CHECK(avp_mp_player_native_capture(&host_after));
    CHECK(memcmp(&host_before,&host_after,sizeof(host_before))==0);

    CHECK(avp_mp_player_scope_enter(&scope,&a));
    CHECK(player_energy==700 && new_pos==152u && cur_wepno==1);
    CHECK(seed0==0x2222u);
    CHECK(avp_mp_player_native_capture(&inside));
    CHECK(inside.host_analog_forward==0.8f);
    ResetMove();
    CHECK(x_vel==0 && y_vel==0 && ang_vel==0);
    CHECK(avp_mp_player_scope_leave(&scope,&a));
    CHECK(b.x_vel==400 && b.y_vel==300);

    /* Four simultaneous player slots, all serially borrowing the ORIGINAL
     * machine. Two species can be the same while remaining independent. */
    slots[0]=a;slots[1]=b;slots[2]=host_before;slots[3]=host_before;
    slots[2].player_type=PT_HUMAN;slots[3].player_type=PT_ALIEN;
    for(unsigned i=0;i<4u;i++) {
        slots[i].player_energy=(s16)(100+i*100);
        slots[i].x_pos=(u32)(i+1u)<<16;
        slots[i].y_pos=(u32)i<<16;
        slots[i].x_vel=(s32)(i+1u)*16;
        slots[i].y_vel=0;
        slots[i].seed0=(u16)(0x1500u+i);
        slots[i].seed1=(u16)(0x2600u+i);
    }
    for(unsigned i=0;i<4u;i++) {
        CHECK(avp_mp_player_scope_enter(&scope,&slots[i]));
        CHECK(player_energy==(s16)(100+i*100));
        InitMove();TidyMove();
        CHECK(new_pos==((i*8u+i+1u)*8u));
        CHECK(avp_random()==(u16)(0x2600u+i));
        player_energy++;
        CHECK(avp_mp_player_scope_leave(&scope,&slots[i]));
        CHECK(avp_mp_player_native_capture(&host_after));
        CHECK(memcmp(&host_before,&host_after,sizeof(host_before))==0);
    }
    for(unsigned i=0;i<4u;i++) {
        CHECK(slots[i].player_energy==(s16)(101+i*100));
        CHECK(slots[i].seed0==(u16)(0x2600u+i));
        CHECK(slots[i].new_pos==((i*8u+i+1u)*8u));
    }

    bad=b;
    bad.valid=0;
    CHECK(!avp_mp_player_scope_enter(&scope,&bad));
    CHECK(avp_mp_player_native_capture(&host_after));
    CHECK(memcmp(&host_before,&host_after,sizeof(host_before))==0);
    bad=b;
    bad.player_type=23;
    CHECK(!avp_mp_player_native_restore(&bad));
    CHECK(!avp_mp_player_native_restore(NULL));
    CHECK(!avp_mp_player_native_capture(NULL));
    CHECK(avp_mp_player_native_capture(&host_after));
    CHECK(memcmp(&host_before,&host_after,sizeof(host_before))==0);

    puts("PASS: original InitMove/TidyMove/ResetMove/RNG, four independent player snapshots, original host restored; one process, no windows");
    return 0;
}
