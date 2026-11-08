/* Predator Medkit voice lifetime through real PXFades and AVPSOUND allocation.
 * Normal source regression only: no in-game instrumentation or ROM assets. */
#include "mazescrn.h"
#include "music.h"
#include "player.h"
#include "weapons.h"
#include "collectables.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

s16 player_type=PT_PREDATOR,medpak=0,player_energy=3000,max_energy=3000;
u16 player_vel=0,invisflag=0;
u8 wep_fire=0,invis_act=0,invis_stat=0,medpak_act=0;
s16 cheat=0,cur_wepno=0,new_wepno=0,alien_bite=0;
u8 cur_weps=0,old_weps=0;
AvpAmmoInfo ammo_info[6];
s32 score=0,sin_ang=0,cos_ang=0;
volatile u32 joy_edge=0xffffffffu;
uintptr_t savegame=0;
s32 cos_d0(u16 angle){(void)angle;return 0x7fff;}

static u32 active[8],started[8],released[8];
static unsigned plays,unloops;
static void start(void *user,unsigned id,u32 handle,unsigned voice){
    (void)user;
    assert(voice<8u && handle && handle!=(u32)-1);
    active[voice]=handle;
    if(id==AVP_FX_PMED3){assert(plays<8u);started[plays++]=handle;}
}
static int is_active(void *user,u32 handle){
    (void)user;
    for(unsigned i=0;i<8u;i++)if(active[i]==handle)return 1;
    return 0;
}
static void release(void *user,u32 handle){
    (void)user;
    assert(handle && handle!=(u32)-1 && unloops<8u);
    released[unloops++]=handle;
    for(unsigned i=0;i<8u;i++)if(active[i]==handle)active[i]=0; /* completed tail */
}
static void stop_all(void *user){(void)user;memset(active,0,sizeof(active));}
static void fade_out(void){
    medpak_act=0;
    for(int i=0;i<10;i++)PXFades();
    assert(medpak_bright==0);
}
int main(void){
    AvpRuntimeOps ops={0};
    ops.play_sfx_handle=start;
    ops.sfx_handle_active=is_active;
    ops.unloop_sfx=release;
    ops.kill_sounds=stop_all;
    avp_runtime_bind(&ops);
    InitSynt();InitPXFades();
    /* One Medkit use initiates exactly one logical voice through all ticks. */
    medpak=300;ammo_info[5].cur=300;player_energy=1200;
    medpak_act=0xff;
    for(int i=0;i<6;i++)PXFades();
    assert(plays==1 && unloops==0 && medpak==240 && player_energy==1560);
    /* Rapid off/on before fade-out must not start or stack a second loop. */
    medpak_act=0;PXFades();assert(plays==1 && unloops==0);
    medpak_act=0xff;PXFades();assert(plays==1 && unloops==0);
    fade_out();assert(unloops==1 && released[0]==started[0]);
    /* Completed activation permits a new handle, never resurrects the old. */
    medpak_act=0xff;PXFades();assert(plays==2 && started[1]!=started[0]);
    fade_out();assert(unloops==2 && released[1]==started[1]);
    /* Exhaustion turns off healing and releases the loop after the fade. */
    medpak=10;ammo_info[5].cur=10;player_energy=500;
    medpak_act=0xff;PXFades();assert(plays==3 && medpak==0 && player_energy==560);
    PXFades();assert(!medpak_act);
    for(int i=0;i<10;i++)PXFades();
    assert(unloops==3 && released[2]==started[2] && !(cur_weps&(1u<<6)));
    /* Level/player reset also releases an interrupted active Medkit voice. */
    medpak=20;ammo_info[5].cur=20;medpak_act=0xff;PXFades();
    assert(plays==4 && unloops==3);
    InitPXFades();assert(unloops==4 && released[3]==started[3]);
    /* An overloaded retail-priority allocator can decline the first SFX;
     * Medkit charge and HP still advance and the cue is retried next tick. */
    {
        AvpSoundMeta high[6]={0};
        InitSynt();InitPXFades();medpak_act=0;
        for(unsigned i=0;i<6u;i++){
            high[i].priority=30000;high[i].synth_type=1;high[i].looping=1;
        }
        avp_music_bind_sfx_meta(high,6u);
        for(unsigned i=0;i<6u;i++)assert(DoEffect(i)!=(u32)-1);
        medpak=100;ammo_info[5].cur=100;player_energy=100;
        medpak_act=0xff;PXFades();
        assert(plays==4 && medpak==90 && player_energy==160);
        active[2]=0; /* One DSP voice finishes; next allocator pass frees it. */
        PXFades();assert(plays==5 && medpak==80 && player_energy==220);
        fade_out();assert(unloops==5 && released[4]==started[4]);
        avp_music_bind_sfx_meta(NULL,0u);
    }
    avp_runtime_reset_ops();
    return 0;
}
