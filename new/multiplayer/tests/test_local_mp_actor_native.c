/* Full native PLAYER.S + original MAZESCRN.S script snapshot transaction.
 * This is NOT whole-world movement/combat or multiplayer gameplay.
 */
#include "local_mp_actor_native.h"
#include "player.h"
#include "joypad.h"
#include "collision.h"
#include "maze.h"
#include "avp_runtime.h"
#include "hud_message.h"
#include "hud.h"
#include "collectables.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)

/* Non-player/non-weapon globals which belong to other original units. */
u32 x_pos,y_pos,centre_angle;
s32 sin_ang,cos_ang;
u16 maze_width;
volatile u32 joy_cur,joy_edge;
void (*x_read)(void);
u8 lift_key,acs_level;
s16 alien_bite,show_mt;
uintptr_t savegame;
AvpAmmoInfo ammo_info[6];
s16 medpak;

/* Link-only dependencies of original gameplay code; not used for physics. */
const AvpRuntimeOps *avp_runtime_ops(void) { static const AvpRuntimeOps no_events={0}; return &no_events; }
u8 SafePos(u8 height,s32 xpos,s32 ypos,u16 width)
{ (void)height;(void)xpos;(void)ypos;(void)width;return 0; }
int AMPCollisions(s32 xpos,s32 ypos)
{ (void)xpos;(void)ypos;return 0; }
s32 cos_d0(u16 angle){(void)angle;return 0;}
AvpAmp *player_weapon(s32 a,s32 b,u16 c,u16 d,s32 e,s32 f)
{(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return NULL;}
u32 avp_music_placed_voice(unsigned id,unsigned channel)
{(void)id;(void)channel;return 0;}
u32 TFX(unsigned id){(void)id;return 0;}
void StopLoop(u32 id){(void)id;}

int main(void)
{
    AvpMpActorNativeState host,again,actors[4],bad;
    AvpMpActorNativeScope scope={0},other={0};
    static const s16 species[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR,PT_ALIEN};
    static const s16 weapon[]={1,2,4,1};
    player_type=PT_HUMAN;player_energy=900;acs_level=0;
    num_weps=4;cur_weps=0x1eu;cur_wepno=1;new_wepno=0;
    new_weapon();
    ammo_info[0].cur=33;
    InitHUDMsg();
    avp_hudmsg_set_access_code(42);
    avp_hud_set_tracker_distance(600);
    map_on=1;map_info.x=7;show_mt=0;
    num_cocoons=1;cocoon_data[0].frame=10;
    CHECK(avp_mp_actor_native_capture(&host));
    for(unsigned i=0;i<4u;i++) {
        player_type=species[i];player_energy=(s16)(100+i*100);acs_level=(u8)(2+i);
        cur_weps=0x7eu;cur_wepno=weapon[i];new_wepno=0;
        num_weps=species[i]==PT_HUMAN?4:species[i]==PT_ALIEN?3:6;
        new_weapon();
        x_pos=(i+1u)*65536u;y_pos=(i+2u)*65536u;
        ammo_info[0].cur=(u16)(45u+i);
        avp_hud_set_tracker_distance(100u+i);
        map_on=(u8)(i&1u);map_info.x=(s16)(30+i);show_mt=i==0u?-1:0;
        num_cocoons=(s16)(i+1u);cocoon_data[0].frame=(s16)(40+i);
        avp_hudmsg_pickup(AVP_CNOTICE_CARD,i+1u);
        UpdtHUDMsg();
        ShowHUDMsg();
        avp_hudmsg_set_access_code(i+10u);
        avp_hudmsg_set_destination_level(i+1u);
        avp_countdown_text[5]=(char)('1'+i);
        CHECK(avp_mp_actor_native_capture(&actors[i]));
    }
    CHECK(avp_mp_actor_native_restore(&host));
    for(unsigned i=0;i<4u;i++) {
        CHECK(avp_mp_actor_scope_enter(&scope,&actors[i]));
        CHECK(!avp_mp_actor_scope_enter(&other,&actors[(i+1u)%4u]));
        CHECK(player_type==species[i] && player_energy==(s16)(100+i*100));
        CHECK(acs_level==(u8)(2+i));
        CHECK(show_mt==(i==0u?-1:0));
        CHECK(ammo_info[0].cur==(u16)(45u+i));
        CHECK(cur_wepno==weapon[i]);
        CHECK(avp_hudmsg_current()!=NULL);
        CHECK(actors[i].hudmsg.access_text[10]==(char)('0'+(i+10u)/10u));
        CHECK(strstr(actors[i].hudmsg.pickup_card,"CARD #")!=NULL);
        CHECK(actors[i].hudmsg.newlevel_text[9]==(char)('1'+i));
        CHECK(actors[i].hudmsg.countdown_text[5]==(char)('1'+i));
        CHECK(actors[i].hudmsg.pickup_card[7]==(char)('0'+(i+1u)));
        CHECK(avp_hud_tracker_nearest()==100u+i);
        CHECK(map_info.x==(s16)(30+i));
        CHECK(num_cocoons==(s16)(i+1u));
        CHECK(cocoon_data[0].frame==(s16)(40+i));
        num_cocoons=(s16)(7+i);cocoon_data[0].frame=(s16)(80+i);
        avp_hud_set_tracker_distance(300u+i);
        map_info.x=(s16)(60+i);
        avp_hudmsg_set_access_code(i+70u);
        avp_hudmsg_set_destination_level(i+1u);
        avp_countdown_text[5]=(char)('5'+i);
        avp_hudmsg_pickup(AVP_CNOTICE_CARD,i+5u);
        UpdtHUDMsg();
        update_wep();
        player_energy=(s16)(player_energy+5);
        acs_level=(u8)(6+i);
        cur_weps=(u8)(cur_weps-1u); /* shared selector lives in BOTH native snapshots */
        ammo_info[0].cur=(u16)(ammo_info[0].cur+2u);
        CHECK(avp_mp_actor_scope_leave(&scope,&actors[i]));
        CHECK(avp_mp_actor_native_capture(&again));
        CHECK(memcmp(&host,&again,sizeof(host))==0);
    }
    for(unsigned i=0;i<4u;i++) {
        CHECK(actors[i].player.player_energy==(s16)(105+i*100));
        CHECK(actors[i].access_level==(u8)(6+i));
        CHECK(actors[i].weapon.ammo_info[0].cur==(u16)(47u+i));
        CHECK(actors[i].hudmsg.access_text[10]==(char)('0'+(i+70u)/10u));
        CHECK(actors[i].hudmsg.pickup_card[7]==(char)('0'+(i+5u)));
        CHECK(actors[i].hudmsg.newlevel_text[9]==(char)('1'+i));
        CHECK(actors[i].hudmsg.countdown_text[5]==(char)('5'+i));
        CHECK(actors[i].hudview.tracker_nearest==300u+i);
        CHECK(actors[i].hudview.show_mt==(i==0u?-1:0));
        CHECK(actors[i].hudview.map_info.x==(s16)(60+i));
        CHECK(actors[i].hudview.num_cocoons==(s16)(7+i));
        CHECK(actors[i].hudview.cocoon_data[0].frame==(s16)(80+i));
        CHECK(actors[i].player.cur_weps==(u8)(0x7eu-1u));
        CHECK(actors[i].weapon.cur_weps==actors[i].player.cur_weps);
        CHECK(avp_mp_actor_scope_enter(&scope,&actors[i]));
        CHECK(cur_weps==(u8)(0x7eu-1u));
        CHECK(avp_mp_actor_scope_leave(&scope,&actors[i]));
    }
    CHECK(avp_mp_actor_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);

    /* S74 regression: map input and the original health-bar machine must
     * update the actor currently bound to the engine. A P1 map press must
     * never toggle P2's independent HUD, and their health pixels cannot
     * share one process-global cached value. */
    {
        const u8 p0_map=actors[0].hudview.map_on;
        const u8 p1_map=actors[1].hudview.map_on;
        unsigned bar[2];
        for(unsigned p=0;p<2u;p++){
            CHECK(avp_mp_actor_scope_enter(&scope,&actors[p]));
            /* Physical KEY_8 edge is active-low in the original Jaguar
             * input convention, exactly as PLAYER.S::MapKeys expects. */
            joy_edge=UINT32_MAX & ~(1u<<KEY_8);
            MapKeys();
            max_energy=1000;
            player_energy=(s16)(p==0u?200:900);
            InitNrg();
            UpdtNrg();
            bar[p]=avp_hud_energy_pixels();
            CHECK(avp_mp_actor_scope_leave(&scope,&actors[p]));
        }
        CHECK(actors[0].hudview.map_on==(u8)(p0_map^0xffu));
        CHECK(actors[1].hudview.map_on==(u8)(p1_map^0xffu));
        CHECK(bar[0]>0u && bar[1]>bar[0] && bar[1]<=44u);
        CHECK(actors[0].hudview.nrg_draw_pixels==bar[0]);
        CHECK(actors[1].hudview.nrg_draw_pixels==bar[1]);
        CHECK(avp_mp_actor_scope_enter(&scope,&actors[0]));
        joy_edge=UINT32_MAX;
        MapKeys(); /* Released key must not toggle map again. */
        CHECK(avp_mp_actor_scope_leave(&scope,&actors[0]));
        CHECK(actors[0].hudview.map_on==(u8)(p0_map^0xffu));
        CHECK(avp_mp_actor_native_capture(&again));
        CHECK(memcmp(&host,&again,sizeof(host))==0);
    }
    /* A failed live script capture still restores the original engine. */
    CHECK(avp_mp_actor_scope_enter(&scope,&actors[0]));
    num_weps=7;
    CHECK(!avp_mp_actor_scope_leave(&scope,&actors[0]));
    CHECK(avp_mp_actor_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);
    /* Deathmatch Security 99 is valid for an original Marine actor,
       while out-of-range/corrupt access credentials still fail. */
    bad=actors[0];bad.access_level=99u;
    CHECK(avp_mp_actor_native_valid(&bad));
    bad.access_level=100u;
    CHECK(!avp_mp_actor_native_valid(&bad));
    bad=actors[0];bad.weapon.cur_weps^=1;
    CHECK(!avp_mp_actor_scope_enter(&scope,&bad));
    CHECK(!avp_mp_actor_native_restore(&bad));
    CHECK(avp_mp_actor_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);
    bad=actors[0];bad.hudmsg.valid=0;
    CHECK(!avp_mp_actor_scope_enter(&scope,&bad));
    CHECK(!avp_mp_actor_native_restore(&bad));
    CHECK(avp_mp_actor_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);
    bad=actors[0];bad.hudview.valid=0;
    CHECK(!avp_mp_actor_scope_enter(&scope,&bad));
    CHECK(!avp_mp_actor_native_restore(&bad));
    CHECK(avp_mp_actor_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);
    bad=actors[0];bad.weapon.cur_def=(const AvpWeaponDef *)(uintptr_t)1;
    CHECK(!avp_mp_actor_scope_enter(&scope,&bad));
    CHECK(avp_mp_actor_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);
    puts("PASS: four player/weapon/HUD contexts, original map keys and health bars isolated, host rollback and pointer rejection; one process");
    return 0;
}
