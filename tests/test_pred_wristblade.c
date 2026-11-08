/* End-to-end Predator wrist-blade commands, animation, melee damage, and
 * medpak charge state without a retail ROM or host audio/video dependency. */
#include "mazescrn.h"
#include "collectables.h"
#include "player.h"
#include "weapons.h"
#include "avp_runtime.h"
#include "maze.h"
#include "joypad.h"
#include "avp_sfx_ids.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

s16 player_type=PT_PREDATOR;
u16 player_vel=0;
u8 wep_fire=0;
s16 cheat=0, medpak=0;
s16 player_energy=3000,max_energy=3000;
u16 invisflag=0;
u8 invis_act=0,invis_stat=0,medpak_act=0;
AvpAmmoInfo ammo_info[6];
s32 score=0;
u8 cur_weps=0,old_weps=0;
s16 cur_wepno=0,new_wepno=0;
volatile u32 joy_edge=0xffffffffu;
s32 sin_ang=0,cos_ang=0;
s16 alien_bite=0;
uintptr_t savegame=0;
s32 cos_d0(u16 angle){(void)angle;return 0x7fff;}
static unsigned blade_sfx,facehug_shake_voices,facehug_suck_voices;
u32 TFX(unsigned id){if(id==AVP_FX_WRBLA)++blade_sfx;
    if(id==AVP_FX_UHH3A||id==AVP_FX_PHIT3)++facehug_shake_voices;
    if(id==AVP_FX_SUCK1)++facehug_suck_voices;
    return 0;}
u32 avp_music_placed_voice(unsigned id,unsigned v){(void)v;return TFX(id);}
void StopLoop(u32 id){(void)id;}
AvpAmp *TestSpark(void){return NULL;}
AvpAmp *player_weapon(s32 a,s32 b,u16 c,u16 d,s32 e,s32 f){
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return NULL;
}
static unsigned melee_hits;
static s16 melee_damage;
static AvpAmp target;
static AvpAmp *wrist_hit(s16 damage,u32 dist,u16 width){
    assert(dist==(u32)0x90u<<8);assert(width==20);
    ++melee_hits;melee_damage=damage;return &target;
}
static void tick(void){update_wep();process_shot();recharge();}
int main(void){
    AvpFirstPersonOverlayState st;
    const AvpWeaponDef *w=avp_weapon_def(PT_PREDATOR,4);
    u8 saved[20]={0};
    unsigned n;
    assert(w && !strcmp(w->name,"wrist blade") && w->damage==30 && w->distance==0x90);
    assert(w->frame0 && w->actions[0] && w->action_count==1);

    /* A fresh Predator never inherits a previous campaign's medpak charge. */
    medpak=250;InitWeps();assert(num_weps==6 && medpak==0 && ammo_info[5].cur==0);
    saved[8]=0x02;saved[9]=0x58; /* 600 charge in retail save LONG 2 */
    savegame=(uintptr_t)saved;InitWeps();savegame=0;
    assert(medpak==600 && ammo_info[5].cur==600);
    fill_weps();assert(medpak==1000 && ammo_info[5].cur==1000);
    medpak=600;ammo_info[5].cur=600;
    medpak_act=0xff;player_energy=2500;PXFades();
    assert(medpak==590 && ammo_info[5].cur==590 && player_energy==2560);
    medpak_act=0;

    /* The permanently owned fourth slot is a genuine melee weapon: no hit
     * while idle, then retail PUNCH1/PUNCH2 animation, wrist sound, and a
     * close-range damage callback during the actual attack window. */
    cur_weps=(1u<<4)|(1u<<5);
    cur_wepno=4;new_wepno=0;wep_fire=0;
    avp_mazescrn_set_shot_callback(wrist_hit);new_weapon();
    /* The original frame0 pose is exposed directly; wrist blades do not
     * invent a move-in offset. */
    avp_mazescrn_get_overlay_state(&st);
    assert(st.x==0 && st.y==0);
    assert(!st.intro_preview);
    for(n=0;n<12;n++)tick();
    avp_mazescrn_get_overlay_state(&st);
    assert(st.overlay_id==22 && !st.overlay_visible && melee_hits==0);
    wep_fire=0xff;
    for(n=0;n<16;n++){
        tick();avp_mazescrn_get_overlay_state(&st);
        if(n==2)assert(st.overlay_id==23 && st.overlay_visible);
    }
    assert(melee_hits>=1 && melee_damage>0 && melee_damage<=30);
    assert(blade_sfx>=1);

    /* Preserve the previously user-confirmed Marine R98 correction.
     * The read-only source pose must never alter simulation state. */
    avp_mazescrn_set_shot_callback(NULL);
    wep_fire=0;
    player_type=PT_HUMAN;
    for(s16 slot=1;slot<=4;slot++){
        cur_weps=(u8)(1u<<slot);cur_wepno=slot;new_wepno=0;
        new_weapon();
        avp_mazescrn_get_overlay_state(&st);
        assert(st.x==0 && st.y==0);
        assert(st.intro_preview && st.intro_x==0 && st.intro_y==(90L<<16));
        /* Reading the source-authored presentation pose never advances gameplay. */
        avp_mazescrn_get_overlay_state(&st);
        assert(st.intro_preview && st.x==wep_x && st.y==wep_y);
        /* Original std_in command: offset (0,100), velocity (0,-10),
         * applied once at the end of the first command frame. */
        tick(); avp_mazescrn_get_overlay_state(&st);
        assert(st.x==0 && st.y==(90L<<16));
        assert(!st.intro_preview);
        for(n=0;n<12;n++)tick();
    }
    /* Exercise a REAL Marine change request (not only an isolated
     * new_weapon call).  The source-owned move_out and move_in remain
     * unchanged, and only the first rendered introduction needs the
     * previously working R98 offset. */
    player_type=PT_HUMAN;
    cur_weps=(u8)((1u<<1)|(1u<<2));cur_wepno=1;new_wepno=0;wep_fire=0;
    new_weapon();for(n=0;n<20;n++)tick();
    new_wepno=2;
    {
        int switched=0;
        for(n=0;n<36;n++){
            tick();avp_mazescrn_get_overlay_state(&st);
            if(cur_wepno==2){
                assert(st.intro_preview && st.intro_y==(90L<<16));
                assert(st.x==wep_x && st.y==wep_y);
                switched=1;break;
            }
        }
        assert(switched);
        tick();avp_mazescrn_get_overlay_state(&st);
        assert(!st.intro_preview);
        /* Let the original Marine move_in script finish before the next
         * independent species test; its SET_TIME timer is source-owned. */
        for(n=0;n<18;n++)tick();
    }
    /* Alien move_in uses three distinct source-authored initial positions.
     * The settled frame0 pose must not flash on-screen before the first
     * source move_in frame: the same Marine first-pose issue occurs on
     * rapid bite/tail taps returning to claw. */
    player_type=PT_ALIEN;
    for(s16 slot=1;slot<=3;slot++){
        static const s32 initial_x[]={20,24,0};
        static const s32 initial_y[]={-30,72,0};
        static const s32 first_x[]={50,24,0};
        static const s32 first_y[]={-60,132,60};
        cur_weps=0x0eu;cur_wepno=slot;new_wepno=0;
        new_weapon();
        avp_mazescrn_get_overlay_state(&st);
        assert(st.x==initial_x[slot-1]*65536 && st.y==initial_y[slot-1]*65536);
        assert(st.intro_preview);
        assert(st.intro_x==first_x[slot-1]*65536);
        assert(st.intro_y==first_y[slot-1]*65536);
        /* Repeated reads cannot advance simulation or alter the input state. */
        avp_mazescrn_get_overlay_state(&st);
        assert(st.intro_preview && st.x==wep_x && st.y==wep_y);
        tick();avp_mazescrn_get_overlay_state(&st);
        assert(st.x==first_x[slot-1]*65536 && st.y==first_y[slot-1]*65536);
        assert(!st.intro_preview);
        for(n=0;n<12;n++)tick();
    }
    /* Exercise actual original switch processing: held bite, then release to
     * default claw, then held tail, then release to default claw. The newly
     * equipped species sprite must be presented at its *own* first move-in
     * pose; no shared 0,90 'Marine' origin is allowed. Attack and weapon
     * request selection is deliberately not rewritten by this test/fix. */
    {
        const s16 destinations[]={3,2,1,2};
        const s32 px[]={0,24,50,24};
        const s32 py[]={60,132,-60,132};
        int found;
        player_type=PT_ALIEN;cur_weps=0x0e;cur_wepno=2;new_wepno=0;
        new_weapon();for(n=0;n<15;n++)tick();
        for(unsigned j=0;j<4u;j++){
            new_wepno=destinations[j];found=0;
            for(n=0;n<25;n++){
                tick();avp_mazescrn_get_overlay_state(&st);
                if(cur_wepno==destinations[j]&&st.intro_preview){
                    assert(st.intro_x==px[j]*65536 && st.intro_y==py[j]*65536);
                    assert(st.x==wep_x && st.y==wep_y);
                    found=1;break;
                }
            }
            assert(found);
            /* First actual 10-Hz command-frame matches read-only presentation. */
            new_wepno=destinations[j];tick();
            avp_mazescrn_get_overlay_state(&st);
            assert(!st.intro_preview);
            assert(st.x==px[j]*65536 && st.y==py[j]*65536);
            new_wepno=0;
            for(n=0;n<10;n++)tick();
        }
    }
    /* R188 independent facehug shake-voice preference: movement and
     * energy drain continue, the suction sound is not mistaken for pain. */
    player_type=PT_HUMAN;
    hugkill=1;hug_recov=0;hug_throw=9;unstick_dir=0;
    avp_mazescrn_set_facehug_shake_pain_sounds(0);
    {unsigned old=facehug_shake_voices; s16 hp=player_energy;
     joy_edge=~(1u<<JOY_LEFT);update_wep();
     assert(hug_throw==8 && player_energy==hp-10);
     assert(facehug_shake_voices==old);}
    hug_throw=9;unstick_dir=0;
    avp_mazescrn_set_facehug_shake_pain_sounds(1);
    {unsigned old=facehug_shake_voices;s16 hp=player_energy;
     joy_edge=~(1u<<JOY_LEFT);update_wep();
     assert(hug_throw==8 && player_energy==hp-10);
     assert(facehug_shake_voices==old+1u);}
    /* R272: retail never stores unstick_dir (only TST.W/NEG.W at $E15C/$E192),
     * so it stays 0: every throw is a LEFT press; RIGHT never counts. */
    hug_throw=9;unstick_dir=0;
    {s16 hp=player_energy;
     joy_edge=~(1u<<JOY_LEFT);update_wep();assert(hug_throw==8&&unstick_dir==0);
     joy_edge=~(1u<<JOY_RIGHT);update_wep();assert(hug_throw==8);
     joy_edge=~(1u<<JOY_LEFT);update_wep();assert(hug_throw==7);
     assert(player_energy==hp-30);}
    hugkill=0;joy_edge=0xffffffffu;
    return 0;
}
