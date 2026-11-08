/* Readable C translation of the first-person/HUD orchestration in
 * MAZE/MAZESCRN.S.  Picture decompression and Object Processor writes are
 * platform services; the original command-stream state machine lives here. */
#include "mazescrn.h"
#include "maze.h"
#include "player.h"
#include "weapons.h"
#include "collectables.h"
#include "hud.h"
#include "avp_sfx_ids.h"
#include "hud_message.h"
#include "avp_runtime.h"
#include "eeprom.h"
#include "savegame.h"
#include "joypad.h"
#include "objects.h"
#include "music.h"
#include <stddef.h>
#include <string.h>

s16 fade_rate,fade_lim,fade_level;
s8 faded,in_fade;
s16 fire_damage;
u32 fire_distance;
u16 fire_width;
s16 num_weps,active_wep,wep_action;
s32 wep_x,wep_y,wep_xvel,wep_yvel;
s16 swing_x,swing_y,swing_max;
u8 swing_on,wait_state,wep_desel;
s16 hugkill,hug_recov,hug_throw,unstick_dir;
static int pc_facehug_shake_pain_sounds_enabled=1;
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){pc_facehug_shake_pain_sounds_enabled=enabled!=0;}
s16 invis_bright,medpak_bright,inwep_bright,outwep_bright;
s16 inwep_no,outwep_no,remove_wepno;
static u8 mpak_maxed,mpak_time,mpak_state;
static u32 medpak_ID;
static u32 wepfxID;

static void *screen_a,*screen_b; static size_t screen_bytes;
static int screen_flip;
static s16 wep_low;
static u16 a1st_time=1;
static s16 chase_vel,swing_angle,gunratemul;
static const AvpWeaponDef *cur_def;
static const AvpWepCmd *wep_animptr,*wait_ptr;
static u8 cur_action;
static int mp_fast_equip;
static int host_unlimited_shooting_range;
void avp_mazescrn_set_unlimited_shooting_range(int enabled)
{host_unlimited_shooting_range=enabled!=0;}
/* A per-player presentation phase, saved alongside the native weapon scope.
 * Fast-Equip advances only original move-out/in command streams twice per
 * 10 Hz tick. It never accelerates attack, ammunition, or projectile scripts. */
static u8 mp_equip_phase;
void avp_mazescrn_mp_fast_equip(int enabled){mp_fast_equip=!!enabled;}
/* Executed only while the dying multiplayer actor's native weapon and
 * reserved audio voice are bound. KillSfx stops an active loop immediately;
 * StopLoop would merely clear its loop flag and let its current tail play. */
void avp_mazescrn_mp_stop_weapon_audio(void)
{
    if(wepfxID && wepfxID!=(u32)-1)KillSfx(wepfxID);
    wepfxID=0;
    wep_fire=0;
    wait_state=0;
}

static s16 over_id=-1,use1_id=-1,use2_id=-1;
static u8 over_visible,use1_visible,use2_visible;
static AvpAmp *(*shot_cb)(s16,u32,u16);

void avp_mazescrn_set_screen_buffers(void *a,void *b,size_t n){screen_a=a;screen_b=b;screen_bytes=n;build_screen=a;}
void avp_mazescrn_set_shot_callback(AvpAmp *(*fn)(s16,u32,u16)){shot_cb=fn;}
void avp_mazescrn_get_overlay_state(AvpFirstPersonOverlayState *out){
    if(!out)return;
    out->overlay_id=over_id;out->overlay_visible=over_visible;
    out->use1_id=use1_id;out->use2_id=use2_id;
    out->use1_visible=use1_visible;out->use2_visible=use2_visible;
    out->swing_enabled=swing_on?1u:0u;
    out->x=wep_x;out->y=wep_y;out->swing_x=swing_x;out->swing_y=swing_y;
    out->intro_preview=0;out->intro_x=out->intro_y=0;
    /* R98 Marine first-frame equip presentation, extended in R203 to the
     * three source-authored Alien introductions.  new_weapon()/frame0()
     * installs the new sprite at its settled pose before the first move_in
     * frame is executed.  This getter shows only that first authored movement
     * pose, without executing any gameplay command or changing the state.
     *
     * Unlike Marine frame0, Alien tail and claw frame0 intentionally place
     * the overlay at nonzero coordinates.  Begin from the *actual* source
     * frame0 position, not zero.  Nonzero velocity identifies a move_in that
     * has already run, so no subsequent animation frame is predicted.
     * Predator, attack commands, audio, and gameplay logic are untouched.
     * The exact Jaguar OP publication timing still requires A/B validation. */
    if((player_type==PT_HUMAN||player_type==PT_ALIEN)&&
       cur_def&&cur_def->move_in&&wep_animptr==cur_def->move_in&&
       wep_xvel==0&&wep_yvel==0&&
       (player_type!=PT_HUMAN||(wep_x==0&&wep_y==0))){
        const AvpWepCmd *c=cur_def->move_in;
        s32 px=wep_x,py=wep_y,vx=0,vy=0;
        for(unsigned i=0;i<12u;i++,c++){
            if(c->op==AVP_WC_END_ANIM)break;
            if(c->op==AVP_WC_END_FRAME){
                out->intro_preview=1;out->intro_x=px+vx;out->intro_y=py+vy;
                break;
            }
            if(c->op==AVP_WC_OFFSET_POS){px+=(s32)c->a*65536;py+=(s32)c->b*65536;}
            else if(c->op==AVP_WC_SET_POS){px=(s32)c->a*65536;py=(s32)c->b*65536;}
            else if(c->op==AVP_WC_SET_VEL){vx=(s32)c->a*65536;vy=(s32)c->b*65536;}
            else if(c->op==AVP_WC_ZERO_VEL){vx=vy=0;}
            else if(c->op==AVP_WC_ZERO_POS){px=py=0;}
            else if(c->op==AVP_WC_SET_FVEL){vx=c->a;vy=c->b;}
        }
    }
}

/* Logical overlays.  Actual pixel data is user-ROM-derived and is intentionally
 * not embedded in this code-only repository. */
enum { OV_NONE=-1,OV_GSHOT1,OV_GSHOT2,OV_GSHOT3,OV_GSHOT4,OV_PULSE,OV_FLAMER,OV_SMART,
       OV_TAIL1,OV_TAIL3,OV_BITE1,OV_BITE2,OV_BITE3,OV_SWIPE1,OV_SWIPE2,OV_SWIPE3,
       OV_STICK1,OV_STICK2,OV_STICK3,OV_STICK4,OV_DISK1,OV_DISK2,OV_DISK3,
       OV_PUNCH1,OV_PUNCH2,OV_FACEHUG };
enum { USE_NONE=-1,USE_FLASH1,USE_FLASH2A,USE_FLASH2B,USE_FLASH3A,USE_FLASH3B };
/* Weapon command operands are signed 16-bit values in the readable C model,
 * mirroring the compact source command stream.  Keep small host tokens here and
 * translate them to retail RDB.DTA descriptor identities at the SOUND opcode. */
enum { SFX_SHOTGUN=0x101,SFX_COCK,SFX_PULSE,SFX_FLAME,SFX_SMART,SFX_TAIL,
       SFX_SWIPE,SFX_BITE,SFX_MELEE,SFX_ROCKET,SFX_DISK,SFX_WRBLADE,SFX_EMPTY };
static unsigned weapon_sfx_descriptor(s16 token){
    switch(token){
    case SFX_SHOTGUN:return AVP_FX_SHOTG; case SFX_COCK:return AVP_FX_COCK1;
    case SFX_PULSE:return AVP_FX_PLSX1; case SFX_FLAME:return AVP_FX_FLAMX;
    case SFX_SMART:return AVP_FX_SMRTX; case SFX_TAIL:return AVP_FX_TAIL;
    case SFX_SWIPE:return AVP_FX_WHOOS; case SFX_BITE:return AVP_FX_SWIPE;
    case SFX_MELEE:return AVP_FX_MELEE; case SFX_ROCKET:return AVP_FX_ROCKE;
    case SFX_DISK:return AVP_FX_DISK1; case SFX_WRBLADE:return AVP_FX_WRBLA;
    case SFX_EMPTY:return AVP_FX_MTY; default:return (unsigned)(u16)token;
    }
}

#define C0(o) {o,0,0}
#define C1(o,a) {o,a,0}
#define C2(o,a,b) {o,a,b}
#define EF C0(AVP_WC_END_FRAME)
#define EA C0(AVP_WC_END_ANIM)

static const AvpWepCmd std_in[]={C2(AVP_WC_OFFSET_POS,0,100),C2(AVP_WC_SET_VEL,0,-10),C1(AVP_WC_SET_TIME,9),EF,EA};
static const AvpWepCmd std_out[]={C2(AVP_WC_SET_VEL,0,10),C1(AVP_WC_SET_TIME,9),EF,EA};
static const AvpWepCmd no_action[]={EA};
static const AvpWepCmd no_frame0[]={C0(AVP_WC_HIDE_OVER),EA};

static const AvpWepCmd shot_f0[]={C1(AVP_WC_SET_OVER,OV_GSHOT1),C1(AVP_WC_SET_USE1,USE_FLASH1),C0(AVP_WC_HIDE_USE),EA};
static const AvpWepCmd shot_act[]={C0(AVP_WC_FIRE),C1(AVP_WC_SOUND,SFX_SHOTGUN),C0(AVP_WC_SHOW_USE1),C2(AVP_WC_OFFSET_POS,0,24),EF,
 C0(AVP_WC_HIDE_USE1),C2(AVP_WC_SET_VEL,0,-6),C1(AVP_WC_SET_TIME,2),EF,
 C1(AVP_WC_SET_OVER,OV_GSHOT2),EF,C1(AVP_WC_SOUND_ALT,SFX_COCK),C1(AVP_WC_SET_OVER,OV_GSHOT3),EF,
 C0(AVP_WC_ZERO_VEL),C1(AVP_WC_SET_OVER,OV_GSHOT4),C1(AVP_WC_SET_TIME,2),EF,C1(AVP_WC_SET_OVER,OV_GSHOT3),EF,C1(AVP_WC_SET_OVER,OV_GSHOT2),EF,EA};
static const AvpWepCmd pulse_f0[]={C1(AVP_WC_SET_OVER,OV_PULSE),C1(AVP_WC_SET_USE1,USE_FLASH2A),C1(AVP_WC_SET_USE2,USE_FLASH2B),C0(AVP_WC_HIDE_USE),EA};
static const AvpWepCmd pulse_act[]={C1(AVP_WC_SOUND,SFX_PULSE),C0(AVP_WC_SHOW_USE),C2(AVP_WC_OFFSET_POS,0,20),C0(AVP_WC_WAIT_FIRE),C0(AVP_WC_FIRE),EF,
 C0(AVP_WC_KILL_SOUND),C0(AVP_WC_HIDE_USE),C2(AVP_WC_OFFSET_POS,0,-10),EF,EA};
static const AvpWepCmd flame_f0[]={C1(AVP_WC_SET_OVER,OV_FLAMER),EA};
static const AvpWepCmd flame_act[]={C2(AVP_WC_OFFSET_POS,0,10),C1(AVP_WC_SOUND,SFX_FLAME),C0(AVP_WC_WAIT_FIRE),C1(AVP_WC_PROJECTILE,1),C1(AVP_WC_SET_TIME,2),EF,
 C0(AVP_WC_KILL_SOUND),C2(AVP_WC_OFFSET_POS,0,-5),EF,C0(AVP_WC_ZERO_POS),EF,EA};
static const AvpWepCmd smart_f0[]={C1(AVP_WC_SET_OVER,OV_SMART),C1(AVP_WC_SET_USE1,USE_FLASH3A),C1(AVP_WC_SET_USE2,USE_FLASH3B),C0(AVP_WC_HIDE_USE),EA};
static const AvpWepCmd smart_act[]={C1(AVP_WC_SOUND,SFX_SMART),C0(AVP_WC_SHOW_USE),C2(AVP_WC_OFFSET_POS,0,20),C0(AVP_WC_WAIT_FIRE),C0(AVP_WC_FIRE),EF,
 C0(AVP_WC_KILL_SOUND),C2(AVP_WC_OFFSET_POS,0,-10),EF,EA};

static const AvpWepCmd tail_in[]={C2(AVP_WC_OFFSET_POS,60,-60),C2(AVP_WC_SET_VEL,-30,30),C1(AVP_WC_SET_TIME,1),EF,EA};
static const AvpWepCmd tail_out[]={C2(AVP_WC_SET_VEL,30,-30),C1(AVP_WC_SET_TIME,1),EF,EA};
static const AvpWepCmd tail_f0[]={C1(AVP_WC_SET_OVER,OV_TAIL1),C2(AVP_WC_SET_POS,20,-30),EA};
static const AvpWepCmd tail_act[]={C1(AVP_WC_SOUND,SFX_TAIL),C2(AVP_WC_SET_POS,-20,-5),EF,C1(AVP_WC_SET_OVER,OV_TAIL3),C0(AVP_WC_FIRE),C2(AVP_WC_SET_POS,0,20),C1(AVP_WC_SET_TIME,2),EF,
 C1(AVP_WC_SET_OVER,OV_TAIL1),C2(AVP_WC_SET_POS,-20,-5),EF,C2(AVP_WC_SET_POS,20,-30),C1(AVP_WC_SET_TIME,2),EF,EA};
static const AvpWepCmd bite_in[]={C2(AVP_WC_OFFSET_POS,0,90),C2(AVP_WC_SET_VEL,0,-30),C1(AVP_WC_SET_TIME,2),EF,EA};
static const AvpWepCmd bite_out[]={C2(AVP_WC_SET_VEL,0,40),C1(AVP_WC_SET_TIME,2),EF,EA};
static const AvpWepCmd bite_f0[]={C0(AVP_WC_DISABLE_SWING),C1(AVP_WC_SET_OVER,OV_BITE1),EA};
static const AvpWepCmd bite_act[]={C0(AVP_WC_SHOW_OVER),EF,C1(AVP_WC_SOUND,SFX_BITE),C1(AVP_WC_SET_OVER,OV_BITE2),C1(AVP_WC_SET_TIME,1),EF,
 C0(AVP_WC_FIRE),C1(AVP_WC_SET_OVER,OV_BITE3),C1(AVP_WC_SET_TIME,2),EF,C1(AVP_WC_SET_OVER,OV_BITE1),C1(AVP_WC_SET_TIME,1),EF,EA};
static const AvpWepCmd swipe_in[]={C2(AVP_WC_OFFSET_POS,0,90),C2(AVP_WC_SET_VEL,0,-30),C1(AVP_WC_SET_TIME,2),EF,EA};
static const AvpWepCmd swipe_out[]={C2(AVP_WC_SET_VEL,0,30),C1(AVP_WC_SET_TIME,2),EF,EA};
static const AvpWepCmd swipe_f0[]={C1(AVP_WC_SET_OVER,OV_SWIPE1),C2(AVP_WC_OFFSET_POS,24,72),EA};
static const AvpWepCmd swipe_act[]={C2(AVP_WC_OFFSET_POS,-24,-72),C1(AVP_WC_SET_TIME,1),EF,C1(AVP_WC_SOUND,SFX_SWIPE),C1(AVP_WC_SET_OVER,OV_SWIPE2),C1(AVP_WC_SET_TIME,1),EF,
 C0(AVP_WC_FIRE),C1(AVP_WC_SET_OVER,OV_SWIPE3),C1(AVP_WC_SET_TIME,3),EF,C1(AVP_WC_SET_OVER,OV_SWIPE2),C2(AVP_WC_OFFSET_POS,6,18),C1(AVP_WC_SET_TIME,1),EF,
 C1(AVP_WC_SET_OVER,OV_SWIPE1),C2(AVP_WC_OFFSET_POS,12,36),C1(AVP_WC_SET_TIME,1),EF,C2(AVP_WC_OFFSET_POS,6,18),C1(AVP_WC_SET_TIME,2),EF,EA};

static const AvpWepCmd stick_f0[]={C1(AVP_WC_SET_OVER,OV_STICK1),EA};
static const AvpWepCmd stick_in[]={C2(AVP_WC_OFFSET_POS,60,60),C2(AVP_WC_SET_VEL,-10,-10),C1(AVP_WC_SET_TIME,5),EF,EA};
static const AvpWepCmd stick_out[]={C2(AVP_WC_SET_VEL,10,10),C1(AVP_WC_SET_TIME,5),EF,EA};
static const AvpWepCmd stick_act[]={C1(AVP_WC_SET_OVER,OV_STICK2),C1(AVP_WC_SET_TIME,1),EF,C1(AVP_WC_SET_OVER,OV_STICK3),C1(AVP_WC_SET_TIME,1),EF,
 C0(AVP_WC_FIRE),C1(AVP_WC_SOUND,SFX_MELEE),C1(AVP_WC_SET_OVER,OV_STICK4),C1(AVP_WC_SET_TIME,5),EF,C1(AVP_WC_SET_OVER,OV_STICK3),C1(AVP_WC_SET_TIME,1),EF,
 C1(AVP_WC_SET_OVER,OV_STICK2),C1(AVP_WC_SET_TIME,1),EF,C1(AVP_WC_SET_OVER,OV_STICK1),C1(AVP_WC_SET_TIME,4),EF,EA};
static const AvpWepCmd laser_act[]={C1(AVP_WC_SOUND,SFX_ROCKET),C1(AVP_WC_PROJECTILE,2),C1(AVP_WC_SET_TIME,10),EF,EA};
static const AvpWepCmd disk_f0[]={C1(AVP_WC_SET_OVER,OV_DISK1),C0(AVP_WC_HIDE_OVER),EA};
static const AvpWepCmd disk_act[]={C0(AVP_WC_SHOW_OVER),C1(AVP_WC_SET_TIME,1),EF,C1(AVP_WC_SET_OVER,OV_DISK2),C1(AVP_WC_SET_TIME,1),EF,
 C1(AVP_WC_SOUND,SFX_DISK),C1(AVP_WC_PROJECTILE,0),C1(AVP_WC_SET_OVER,OV_DISK3),C1(AVP_WC_SET_TIME,1),EF,C2(AVP_WC_OFFSET_POS,-20,20),C1(AVP_WC_SET_TIME,1),EF,
 C0(AVP_WC_HIDE_OVER),C1(AVP_WC_SET_TIME,10),EF,EA};
static const AvpWepCmd punch_f0[]={C1(AVP_WC_SET_OVER,OV_PUNCH1),C0(AVP_WC_HIDE_OVER),EA};
static const AvpWepCmd punch_act[]={C0(AVP_WC_SHOW_OVER),C1(AVP_WC_SET_TIME,1),EF,C1(AVP_WC_SOUND,SFX_WRBLADE),C0(AVP_WC_FIRE),C1(AVP_WC_SET_OVER,OV_PUNCH2),C1(AVP_WC_SET_TIME,3),EF,
 C1(AVP_WC_SET_OVER,OV_PUNCH1),C2(AVP_WC_OFFSET_POS,6,20),C1(AVP_WC_SET_TIME,1),EF,C0(AVP_WC_HIDE_OVER),C1(AVP_WC_SET_TIME,5),EF,EA};
static const AvpWepCmd invis_action[]={C0(AVP_WC_TOGGLE_INVIS),EF,EA};
/* Source medpak action is also toggle_invis. Preserve it rather than “fixing” it. */
static const AvpWepCmd medpak_action[]={C0(AVP_WC_TOGGLE_INVIS),EF,EA};

#define DEF(n,dam,dist,w,ia,ma,f0,mi,mo,a0) {n,dam,dist,w,ia,ma,f0,mi,mo,{a0,NULL},1}
static const AvpWeaponDef human_defs[]={
 DEF("shotgun",10,0x880,40,0,44,shot_f0,std_in,std_out,shot_act),
 DEF("pulse rifle",5,0x880,20,0,264,pulse_f0,std_in,std_out,pulse_act),
 DEF("flamethrower",0,0,0,0,88,flame_f0,std_in,std_out,flame_act),
 DEF("smartgun",60,0x880,20,0,88,smart_f0,std_in,std_out,smart_act)};
static const AvpWeaponDef alien_defs[]={
 DEF("tail",20,0x90,20,200,200,tail_f0,tail_in,tail_out,tail_act),
 DEF("swipe",30,0x90,20,200,200,swipe_f0,swipe_in,swipe_out,swipe_act),
 DEF("bite",100,0x90,20,200,200,bite_f0,bite_in,bite_out,bite_act)};
static const AvpWeaponDef pred_defs[]={
 DEF("combi-stick",100,0x90,20,200,200,stick_f0,stick_in,stick_out,stick_act),
 DEF("shoulder cannon",0,0,0,200,200,no_frame0,no_action,no_action,laser_act),
 DEF("smart disc",0,0,0,200,200,disk_f0,no_action,no_action,disk_act),
 DEF("wrist blade",30,0x90,20,200,200,punch_f0,no_action,no_action,punch_act),
 DEF("invisibility",0,0,0,0,0xffff,no_frame0,no_action,no_action,invis_action),
 DEF("medpak",0,0,0,0,1000,no_frame0,no_action,no_action,medpak_action)};

const AvpWeaponDef *avp_weapon_def(s16 pt,s16 no){
    if(no<=0) return NULL;
    unsigned i=(unsigned)(no-1);
    if(pt==PT_HUMAN)return i<4?&human_defs[i]:NULL;
    if(pt==PT_ALIEN)return i<3?&alien_defs[i]:NULL;
    if(pt==PT_PREDATOR)return i<6?&pred_defs[i]:NULL;
    return NULL;
}

/* MAZESCRN.S owns ordinary-68000 pause/list control state.  The raw Object
 * Processor phrases and CLUT transfers remain platform-owned, but every CPU
 * branch/counter/input decision is retained here. */
enum {
    PAUSE_EV_SET_PAUSED_SPRITE=0x50535052u, /* PSPR */
    PAUSE_EV_LINK_PAUSE_LIST  =0x504c4e4bu, /* PLNK */
    PAUSE_EV_SAVE_OVERS       =0x5053564fu, /* PSVO */
    PAUSE_EV_RESTORE_OVERS    =0x5052564fu, /* PRVO */
    PAUSE_EV_SHOW_MENU        =0x504d454eu, /* PMEN */
    PAUSE_EV_MENU_CURSOR      =0x50435552u, /* PCUR */
    PAUSE_EV_HUD_SLIDER       =0x50534c44u, /* PSLD */
    PAUSE_EV_PULSE_FIX        =0x50504658u  /* PPFX */
};
static u32 maze_frames,save_frames;
static s8 in_pause;
static u8 trigger,allow_shift;
static s16 menu_fn;
static unsigned banana_pos;
static const s8 bnalist[]={FIRE_B,FIRE_A,KEY_9,FIRE_A,KEY_9,FIRE_A,
                          KEY_STAR,OPTION,KEY_6,KEY_HASH,KEY_STAR,KEY_STAR,
                          OPTION,KEY_2,OPTION,-1};

static int key_down(u32 v,unsigned bit){return (v&(1u<<bit))==0u;}
static void pause_event(unsigned ev,u32 a,u32 b,u32 c){
    const AvpRuntimeOps *o=avp_runtime_ops();
    if(o->object_event)o->object_event(o->user,ev,a,b,c);
}
/* MAZESCRN.S pause_sfx uses the ambient-reserved DSP voice 0, while
 * wep_sfx (the pause cheat/save cue) uses weapon-reserved voice 1.
 * Do not send either through ordinary TFX voice allocation. */
static void pause_sfx(unsigned id){
    unsigned retail=id;
    if(id>=SFX_SHOTGUN && id<=SFX_EMPTY) retail=weapon_sfx_descriptor((s16)id);
    (void)avp_music_placed_voice(retail,0u);
}
static void pause_weapon_sfx(unsigned id){
    unsigned retail=id;
    if(id>=SFX_SHOTGUN && id<=SFX_EMPTY) retail=weapon_sfx_descriptor((s16)id);
    (void)avp_music_placed_voice(retail,1u);
}
static void pause_save_overs(void){pause_event(PAUSE_EV_SAVE_OVERS,0,0,0);VSync();}
static void pause_restore_overs(void){pause_event(PAUSE_EV_RESTORE_OVERS,0,0,0);}
static void pause_branch(int enabled){pause_event(PAUSE_EV_LINK_PAUSE_LIST,(u32)(enabled!=0),0,0);}
static void pause_sprite(int visible){pause_event(PAUSE_EV_SET_PAUSED_SPRITE,(u32)(visible!=0),0,0);}

static int pause_hudbar(void)
{
    /* HUDBar: hide B-SAVE, cursor=(26,52), refresh palette twice, slider from
     * signed hud_bright using the exact hand-tuned integer expression. */
    s32 v=(s8)hud_bright;
    u32 slider;
    pause_event(PAUSE_EV_MENU_CURSOR,26u,52u,0u);
    HUDBright();VSync();VSync();SetHUDPal();
    v+=0x88;
    slider=(u32)(((u32)v*0x9280u)>>16)+110u;
    pause_event(PAUSE_EV_HUD_SLIDER,slider,0,0);
    return 1;
}
static int pause_save_slot(unsigned slot,u32 edge)
{
    static const u16 y[3]={88u,115u,144u}; /* 74/101/130 + 14 */
    pause_event(PAUSE_EV_MENU_CURSOR,68u,y[slot],1u);
    if(!key_down(edge,FIRE_B))return 1;
    SaveGame((s32)slot);
    return 0;
}
static int pause_menu_exec(s16 which,u32 edge)
{
    switch(which){
    case 0:return pause_hudbar();
    case 1:return pause_save_slot(0,edge);
    case 2:return pause_save_slot(1,edge);
    default:return pause_save_slot(2,edge);
    }
}

void InitDblBufs(void){screen_flip=0;build_screen=screen_a;}
void RestoreMazeList(void)
{
    /* Retail first blanks OLP/display work, restores the full 32-object
     * refresh list (32*16 bytes), then reinstalls MazeList.  Phrase copying is
     * OP-memory work and therefore remains a platform service; the CPU ordering
     * is explicit here. */
    VSync();
    pause_event(0x524d4c30u,0,512u,0); /* RML0: OLP/display quiesce + 512-byte restore */
    {const AvpRuntimeOps*o=avp_runtime_ops();if(o->restore_maze_list)o->restore_maze_list(o->user);}
}
void SetMazeList(void)
{
    /* Shipping NUM_OBJS=32, so OL_SIZE=PL_SIZE=512.  Build/resource identity
     * remains backend-owned; initialization/copy/reset order stays CPU-side. */
    VSync();
    pause_event(0x534d4c30u,(u32)(u16)player_type,512u,512u); /* SML0 */
    in_pause=0;maze_frames=0;
    fade_rate=0;
    {const AvpRuntimeOps*o=avp_runtime_ops();if(o->set_maze_list)o->set_maze_list(o->user);}
}

void LoseSounds(void){avp_pause_audio_save();}
void RestoreSounds(void){avp_pause_audio_restore();}
void pause_off(void)
{
    in_pause=0;
    pause_restore_overs();
    pause_branch(0);
    RestoreSounds();
    maze_frames=save_frames;
}

void DoPause(void)
{
    u32 d4,d5;
    trigger=0;
    d5=joy_edge;
    if(!key_down(d5,PAUSE))return;

    save_frames=maze_frames;
    pause_sprite(0);             /* BRANCH_NEVER: paused sprite enabled by list semantics */
    in_pause=-1;
    pause_save_overs();
    pause_branch(1);
    LoseSounds();

pause_loop:
pause1:
    VSync();readpad();
    d4=joy_cur & ((1u<<KEY_1)|(1u<<KEY_3));
    if(d4==0u)goto clear_screen;
    d5=joy_edge;
    if(key_down(d5,PAUSE))goto out;
    if(!key_down(d5,OPTION))goto pause1;

    trigger=1;
    menu_fn=0;
    (void)pause_hudbar();
    pause_event(PAUSE_EV_SHOW_MENU,1,0,0);

pause2:
    VSync();readpad();
    d4=joy_cur & ((1u<<KEY_1)|(1u<<KEY_3));
    if(d4==0u)goto clear_screen;
    d5=joy_edge;
    if(key_down(d5,PAUSE))goto out;
    if(key_down(d5,OPTION)){pause_sprite(0);goto pause_loop;}
    if(key_down(d5,KEY_6))trigger=4;
    if(key_down(d5,JOY_UP)){
        pause_sfx(SFX_COCK);
        if(--menu_fn<0)menu_fn=3;
    }
    if(key_down(d5,JOY_DOWN)){
        pause_sfx(SFX_COCK);
        if(++menu_fn>3)menu_fn=0;
    }
    if(pause_menu_exec(menu_fn,d5))goto pause2;
    goto out;

clear_screen:
    pause_restore_overs();
    pause_branch(0);
    /* vf_pulse alternation changes only which OP object is hidden; backend owns
     * those phrase addresses, while CPU chooses parity here. */
    pause_event(PAUSE_EV_PULSE_FIX,maze_frames&1u,0,0);

    if(trigger!=4 && trigger!=0){
        trigger=0;readpad();d5=joy_cur;
        if(!key_down(d5,PAUSE)){
            if(key_down(d5,OPTION))trigger=2;
            else {trigger=1;pause_sfx(SFX_COCK);}
        }
    }

pause3:
    VSync();readpad();d5=joy_edge;
    if(key_down(d5,PAUSE))goto out;
    if(trigger>=4)goto banana;

    if(trigger==1 && key_down(d5,OPTION)){
        show_coords=(u8)~show_coords;
        pause_weapon_sfx(SFX_SHOTGUN);
        goto out;
    }
    if(trigger>=2){
        u32 cur=joy_cur | 0x80000000u;
        if(trigger<3){
            u32 x=~cur & ~((1u<<PAUSE)|(1u<<OPTION)|(1u<<KEY_1)|(1u<<KEY_3));
            if(x!=0u)trigger=0;
            else if(cur==~((1u<<PAUSE)|(1u<<OPTION)))trigger=3;
        }else{
            const u32 mask=(1u<<PAUSE)|(1u<<OPTION)|(1u<<KEY_2)|(1u<<KEY_5)|(1u<<KEY_7)|(1u<<KEY_9);
            u32 x=~cur & ~mask;
            if(x!=0u)trigger=0;
            else if(cur==~mask){
                pause_weapon_sfx(AVP_FX_LAFF);
                allow_shift=0xffu;allow_lcs=0;allow_god=0;cheat=0;
                goto out;
            }
        }
    }
    goto pause3;

banana:
    if(trigger!=5){
        u32 cur=joy_cur|0x80000000u;
        u32 x=~cur & ((1u<<PAUSE)|(1u<<OPTION)|(1u<<KEY_1)|(1u<<KEY_3));
        if(x!=0u)goto pause3;
        trigger=5;banana_pos=0;goto pause3;
    }
    d4=joy_edge|0x80000000u;
    d4=~d4;
    if(d4==0u)goto pause3;
    {
        unsigned key=(unsigned)(u8)bnalist[banana_pos];
        d4 &= ~(1u<<key);
        if(d4!=0u){trigger=0;goto pause3;}
        ++banana_pos;
        if(bnalist[banana_pos]>=0)goto pause3;
        pause_weapon_sfx(AVP_FX_LAFF);
        allow_shift=0xffu;allow_lcs=0xffu;allow_god=0xffu;
    }
out:
    pause_off();
}

void MazeList(void){
    /* MAZESCRN.S X_FADE block executes in the active display routine. */
    if(fade_rate){s16 n=(s16)(fade_level+fade_rate);fade_level=n;if(n==fade_lim){fade_rate=0;faded=(s8)-1;}}
    ++maze_frames;
    const AvpRuntimeOps*o=avp_runtime_ops();if(o->gpu_draw_screen)o->gpu_draw_screen(o->user);
}
void ExpandOvers(void){const AvpRuntimeOps*o=avp_runtime_ops();if(o->file_event)o->file_event(o->user,0x4f564552u,(u32)(u16)player_type,0,0);}
void SwapScreens(void){screen_flip^=1;build_screen=screen_flip?screen_b:screen_a;const AvpRuntimeOps*o=avp_runtime_ops();if(o->swap_screens)o->swap_screens(o->user);}
void PreFrame(void){UpdtScrOverlays();const AvpRuntimeOps*o=avp_runtime_ops();if(o->pre_frame)o->pre_frame(o->user);}
void PostFrame(void){
    set_avail(); fade_selects(); if(player_type==PT_PREDATOR)PXFades();
    const AvpRuntimeOps*o=avp_runtime_ops();if(o->post_frame)o->post_frame(o->user);
}

#define MAX_FADE 6
#define FADE_STEP 1
#define MED_TRANS 10
#define WEP_MEDPAK 6
#define MPAK_MARK 6
#define MPAK_SPACE 3

void InitPXFades(void){
    /* A level/player reinitialization must not orphan an active Medkit loop. */
    if(medpak_ID && medpak_ID!=(u32)-1)StopLoop(medpak_ID);
    invis_bright=0; medpak_bright=0; mpak_maxed=mpak_time=mpak_state=0; medpak_ID=0; wepfxID=0;
}

/* Source set_avail/fade_selects game-side state.  Pixel shading and cross-hatch
 * are presentation-only Blitter operations and remain a renderer responsibility. */
void set_avail(void){
    u8 now=cur_weps, changed=(u8)(now^old_weps);
    if(!changed)return;
    u8 falling=(u8)(changed & (u8)~now);
    for(s16 w=1;w<=num_weps;w++){
        u8 bit=(u8)(1u<<w);
        if(falling&bit){
            if(cur_wepno==w && (new_wepno==0 || new_wepno==w))new_wepno=-1;
            if(inwep_no==w)remove_wepno=w;
        }
    }
    old_weps=now;
}

void fade_selects(void){
    if(outwep_bright==0 && inwep_bright>=MAX_FADE){
        s16 n=new_wepno;
        if(n<=0)n=cur_wepno;
        if(n!=inwep_no){
            if(inwep_no){outwep_no=inwep_no;outwep_bright=inwep_bright;inwep_bright=MAX_FADE;}
            inwep_no=n;
            /* MAZESCRN.S:3306-3314: selecting the Predator cannon icon plays
             * `wep_sfx motor1` (PlacedEf voice 1); wepfxID is not updated. */
            if(inwep_no==2 && player_type==PT_PREDATOR)
                (void)avp_music_placed_voice(AVP_FX_MOTOR,1u);
            if(inwep_no)inwep_bright=0;
        }
    }
    if(inwep_bright<MAX_FADE)++inwep_bright;
    if(outwep_bright){
        --outwep_bright;
        if(outwep_bright==0){s16 old=outwep_no;outwep_no=0;if(old==remove_wepno)remove_wepno=0;}
    }
}

void PXFades(void){
    if(invis_stat==0){
        if(invis_act){if(invis_bright<MAX_FADE && ++invis_bright>=MAX_FADE){invis_bright=MAX_FADE;invis_stat=1;}}
        else if(invis_bright>0)--invis_bright;
    }
    s16 charge=medpak;
    if(!(cur_weps&(1u<<WEP_MEDPAK)) && charge)cur_weps=(u8)(cur_weps|(1u<<WEP_MEDPAK));
    if(medpak_act){
        if(!medpak_ID){
            /* The hosted FullSynth needs the actual effect handle to release
             * the retail Medkit loop; a Boolean lets successive uses stack. */
            u32 h=TFX(AVP_FX_PMED3);
            if(h && h!=(u32)-1)medpak_ID=h;
        }
        if(charge){
            s16 transfer=MED_TRANS;if(charge<transfer)transfer=charge;charge=(s16)(charge-transfer);medpak=charge;
            ammo_info[5].cur=(u16)charge;
            s32 energy=(s32)player_energy+(s32)transfer*6;int maxed=0;if(energy>=max_energy){energy=max_energy;maxed=1;}player_energy=(s16)energy;
            if(medpak_bright<MAX_FADE)++medpak_bright;
            else if(maxed){
                if(!mpak_maxed){mpak_maxed=0xff;mpak_time=0;mpak_state=0;}
                if((s8)--mpak_time<0){mpak_state=(u8)~mpak_state;if(mpak_state){mpak_time=MPAK_MARK-1;medpak_bright=MAX_FADE;}else{mpak_time=MPAK_SPACE-1;medpak_bright=0;}}
            }else if(mpak_maxed){mpak_maxed=0;if(!mpak_state)medpak_bright=MAX_FADE;}
        }else medpak_act=0;
    }
    if(!medpak_act){
        mpak_maxed=0;
        if(medpak_bright>0)--medpak_bright;
        if(medpak_bright==0){
            if(medpak_ID && medpak_ID!=(u32)-1)StopLoop(medpak_ID);
            medpak_ID=0;
            if(charge==0)cur_weps=(u8)(cur_weps&~(1u<<WEP_MEDPAK));
        }
    }
}

static u32 save_be32(const u8 *p);

void InitScrOverlays(void){const AvpRuntimeOps*o=avp_runtime_ops();if(o->init_screen_overlays)o->init_screen_overlays(o->user);StdHUD();Init1stOvers();InitHUDPal();}
/* MAZESCRN.S hug_init only expands the facehugger graphic; unstick_dir is
 * never stored by the retail code (only TST.W/NEG.W at $E15C/$E192), so it
 * stays 0 from power-on and the release loop always tests LEFT. */
void hug_init(void){hug_recov=0;hug_throw=0;}
void StdHUD(void)
{
    InitHUD();
    /* A loaded Marine's tracker was restored by InitPlayer, but PostFirst's
     * second InitHUD clears show_mt.  Consume the original saved inventory
     * bit again while savegame is still live; PostFirst clears the pointer
     * only after the complete overlay initialization has finished.  A new
     * game (no savegame), or a save without the pickup, still starts with
     * no tracker.  Leave the original 20-byte EEPROM layout untouched. */
    if (player_type==PT_HUMAN && savegame) {
        const u8 *sv=(const u8 *)(uintptr_t)savegame;
        show_mt=(save_be32(sv+16)&(1u<<11)) ? -1 : 0;
    }
}

void enable_swing(void){swing_on=0xffu;} void disable_swing(void){swing_on=0;}
void InitSwing(void){static const s16 rate[3]={0x6000,0x4000,0x6000};unsigned i=(unsigned)player_type>>2;if(i>2)i=0;swing_max=0x2000;gunratemul=rate[i];swing_angle=0x4000;chase_vel=0;SwingPos();}
void SwingPos(void){
    s16 target=(s16)player_vel;
    if(chase_vel<target){s32 n=chase_vel+0x1000;chase_vel=(s16)(n>target?target:n);} else if(chase_vel>target){s32 n=chase_vel-0x1000;chase_vel=(s16)(n<target?target:n);}
    u32 rate=(u32)(u16)gunratemul*(u16)chase_vel; swing_angle=(s16)(swing_angle+(s16)(rate>>16));
    s32 cx=cos_d0((u16)swing_angle),cy=cos_d0((u16)((u16)swing_angle<<1)); s16 amp=chase_vel;if(amp>swing_max)amp=swing_max;
    s32 xx=(s32)(s16)cx*amp, yy=(s32)(s16)cy*amp; swing_x=(s16)((xx>>16)>>8); swing_y=(s16)(-(((yy>>16)>>9)&~1));
}

static void reset_frame0(void){active_wep=0;wep_x=wep_y=wep_xvel=wep_yvel=0;use1_id=use2_id=-1;use1_visible=use2_visible=0;over_visible=1;enable_swing();}
static void execute_cmd(const AvpWepCmd **pp,const AvpWepCmd *c){
    switch(c->op){
    case AVP_WC_ZERO_POS:wep_x=wep_y=0;break;
    /* The Jaguar shifts signed 16-bit weapon commands into 16.16 fixed
     * point.  Signed left shift of a negative operand is undefined in C;
     * signed multiplication by 65536 is defined for every s16 input and
     * yields precisely the original 32-bit two's-complement bit pattern. */
    case AVP_WC_SET_POS:wep_x=(s32)c->a*65536L;wep_y=(s32)c->b*65536L;break;
    case AVP_WC_OFFSET_POS:wep_x+=(s32)c->a*65536L;wep_y+=(s32)c->b*65536L;break;
    case AVP_WC_ZERO_VEL:wep_xvel=wep_yvel=0;break;
    case AVP_WC_SET_VEL:wep_xvel=(s32)c->a*65536L;wep_yvel=(s32)c->b*65536L;break;
    case AVP_WC_SET_FVEL:wep_xvel=c->a;wep_yvel=c->b;break;
    case AVP_WC_SET_TIME:a1st_time=(u16)c->a;break;
    case AVP_WC_WAIT_FIRE:wait_state=0xffu;wait_ptr=*pp;break;
    case AVP_WC_FIRE:fire_wep();break;
    case AVP_WC_PROJECTILE:{s32 sy=-sin_ang;player_weapon(cos_ang,sy,(u16)c->a,(u16)0,sin_ang,cos_ang);fire_damage=-1;break;}
    case AVP_WC_TOGGLE_INVIS:toggle_invis();break;
    case AVP_WC_SET_OVER:over_id=c->a;over_visible=1;break;
    case AVP_WC_SET_USE1:use1_id=c->a;break;
    case AVP_WC_SET_USE2:use2_id=c->a;break;
    case AVP_WC_SHOW_OVER:over_visible=1;break;case AVP_WC_HIDE_OVER:over_visible=0;break;
    case AVP_WC_SHOW_USE1:use1_visible=1;break;case AVP_WC_HIDE_USE1:use1_visible=0;break;
    case AVP_WC_SHOW_USE:use1_visible=use2_visible=1;break;case AVP_WC_HIDE_USE:use1_visible=use2_visible=0;break;
    /* MAZESCRN.S wep_fx: `wep_sfx` = PlacedEf into reserved voice 1;
     * xwep_fx (cocking, "noise on another channel"): ordinary `sfx`. */
    case AVP_WC_SOUND:
        wepfxID=avp_music_placed_voice(weapon_sfx_descriptor(c->a),1u);
        break;
    case AVP_WC_SOUND_ALT:
        wepfxID=TFX(weapon_sfx_descriptor(c->a));
        break;
    case AVP_WC_KILL_SOUND:
        if(wepfxID && wepfxID!=(u32)-1){StopLoop(wepfxID);wepfxID=0;}
        break;
    case AVP_WC_ENABLE_SWING:enable_swing();break;case AVP_WC_DISABLE_SWING:disable_swing();break;
    case AVP_WC_SET_BITE:alien_bite=c->a;break;case AVP_WC_CLEAR_BITE:alien_bite=0;break;
    default:break;
    }
}
static int play_frame(const AvpWepCmd **pp){
    const AvpWepCmd *p=*pp;
    for(;;){const AvpWepCmd *c=p++;if(c->op==AVP_WC_END_FRAME){*pp=p;return 1;}if(c->op==AVP_WC_END_ANIM){*pp=p;return 0;}execute_cmd(&p,c);}
}
void frame0(void){reset_frame0();if(cur_def&&cur_def->frame0){const AvpWepCmd*p=cur_def->frame0;while(p->op!=AVP_WC_END_ANIM){execute_cmd(&p,p);p++;}}}
void new_weapon(void){
    cur_def=avp_weapon_def(player_type,cur_wepno);if(!cur_def){cur_wepno=0;use1_id=use2_id=-1;over_visible=use1_visible=use2_visible=0;wep_animptr=NULL;return;}frame0();wep_animptr=cur_def->move_in;wep_action=0;wep_desel=0;wait_state=0;fire_damage=0;mp_equip_phase=1;}
void fire_wep(void){
    if(!cur_def)return;
    fire_damage=cur_def->damage;
    fire_distance=(u32)cur_def->distance<<8;
    fire_width=cur_def->width;
    /* The retail weapon definitions use a finite 0x880 shot trace. Opt-in
     * Marine hitscan firearms traverse to the first wall/map edge instead.
     * The signed 16-bit shot vector used by TestSpark bounds this value:
     * 0x7fff00 is sufficient to cross even a 64x64 retail map diagonal.
     * Never extend Alien/Predator melee or alter projectile travel physics. */
    if(host_unlimited_shooting_range && player_type==PT_HUMAN &&
       (cur_wepno==1 || cur_wepno==2 || cur_wepno==4))
        fire_distance=0x7fff00u;
}
void toggle_invis(void){invisflag^=(1u<<AMP_INVHIT);}

static u16 save_be16(const u8 *p){return (u16)(((u16)p[0]<<8)|p[1]);}
static u32 save_be32(const u8 *p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}

/* MAZESCRN.S weapon definitions: bar x,y are local to the 320x200 HUD
   object (screen offset +14), in weapon order for each species. */
static const s16 human_bar_xy[4][2]={{251,34},{251,74},{251,116},{251,163}};
static const s16 alien_bar_xy[3][2]={{248,135},{248,29},{248,87}};
static const s16 pred_bar_xy[6][2]={{250,41},{250,75},{250,116},{250,161},{30,131},{30,131}};

void InitWeps(void){
    unsigned n=player_type==PT_HUMAN?4:player_type==PT_ALIEN?3:6;num_weps=(s16)n;
    memset(ammo_info,0,sizeof(ammo_info));
    for(unsigned i=0;i<n&&i<6;i++){const AvpWeaponDef*d=avp_weapon_def(player_type,(s16)(i+1));AvpAmmoInfo*a=&ammo_info[i];if(player_type==PT_HUMAN || i>=4){a->cur=d->init_or_dec;a->max=d->max_or_inc;a->dec=0;a->inc=0;}else{a->cur=AVP_APAMM_MAX;a->max=AVP_APAMM_MAX;a->dec=d->init_or_dec;a->inc=d->max_or_inc;}a->old=0xffffu;
        /* MAZESCRN.S::InitWAmmo: (44*65536/max)-1, truncated to 16-bit. */
        a->rescale=a->max?(u16)(((44u*65536u)/a->max)-1u):0;
        {const s16 (*xy)[2]=(player_type==PT_HUMAN)?human_bar_xy:
             (player_type==PT_ALIEN)?alien_bar_xy:pred_bar_xy;
         a->x=xy[i][0];a->y=xy[i][1];}}
    if(savegame){
        const u8 *sv=(const u8 *)(uintptr_t)savegame;
        /* MAZESCRN.S::InitWeps restores only CPU-side mutable ammo state:
         * Human save LONGs 2/3 are four big-endian 16-bit ammo counts;
         * Predator save LONG 2 begins with the medpak charge. */
        if(player_type==PT_HUMAN){for(unsigned i=0;i<4u;i++)ammo_info[i].cur=save_be16(sv+8u+i*2u);}
        else if(player_type==PT_PREDATOR)ammo_info[5].cur=save_be16(sv+8u);
    }
    /* Predator medpak charge and slot-6 ammo are the same Jaguar value.
     * Restore both when loading; reset stale charge when starting anew. */
    if(player_type==PT_PREDATOR){
        if(ammo_info[5].cur>ammo_info[5].max)ammo_info[5].cur=ammo_info[5].max;
        medpak=(s16)ammo_info[5].cur;
    }
}
void fill_weps(void){
    for(int i=0;i<num_weps&&i<6;i++)ammo_info[i].cur=ammo_info[i].max;
    /* A Predator ammo refill also refills the portable medpak.  Its ammo
     * word and player-use charge must not become separate values. */
    if(player_type==PT_PREDATOR&&num_weps>=6)
        medpak=(s16)ammo_info[5].cur;
}
void init_fades(void){outwep_bright=0;inwep_bright=MAX_FADE;inwep_no=0;outwep_no=0;remove_wepno=0;}
void Init1stOvers(void){
    InitSwing();InitWeps();if(player_type==PT_PREDATOR){InitPredAvail();InitPXFades();}init_fades();
    fire_damage=0;new_wepno=player_type==PT_PREDATOR?4:1;cur_wepno=0;cur_def=NULL;wep_low=0;
    cur_weps=player_type==PT_HUMAN?0:player_type==PT_ALIEN?0x0e:0x30;
    if(savegame){const u8 *sv=(const u8 *)(uintptr_t)savegame;u32 packed=save_be32(sv+16);cur_weps=(u8)((packed>>5)&0x3eu);}
    old_weps=0xff;hugkill=0;hug_recov=0;hug_throw=0;mp_equip_phase=0;
}

static void start_action(void){if(!cur_def||cur_def->action_count==0)return;unsigned a=cur_action++;if(cur_action>=cur_def->action_count)cur_action=0;wep_animptr=cur_def->actions[a];active_wep^=-1;}
void update_wep(void){
    if(hugkill){
        /* MAZESCRN.S::update_wep/.inhug.  The attach overlay and alternating
         * left/right release sequence belong to the original game, not the
         * host controller.  joy_edge is active-low and fires once per press. */
        if(wepfxID && wepfxID!=(u32)-1){StopLoop(wepfxID);wepfxID=0;}
        if(hug_recov){
            hugkill=0;hug_recov=0;hug_throw=0;
            over_visible=0;wep_fire=0;wait_state=0;
            if(cur_def){frame0();wep_animptr=cur_def->move_in;}
            return;
        }
        if(hug_throw==0){
            hug_throw=9;over_id=OV_FACEHUG;
            over_visible=1;use1_visible=use2_visible=0;
            TFX(AVP_FX_SUCK1);
        }
        player_energy=(s16)(player_energy-10);
        /* TST.W unstick_dir; BPL .left_uns: zero tests LEFT.  NEG.W of the
         * retail value 0 is 0, so every throw needs a fresh LEFT press. */
        if(((joy_edge&(1u<<(unstick_dir>=0?JOY_LEFT:JOY_RIGHT)))==0u)){
            unstick_dir=(s16)-unstick_dir;
            if(--hug_throw==0){
                TFX(AVP_FX_SUCK1);
                hug_recov=-1;
            }else if(hug_throw==4||hug_throw==2){
                TFX(AVP_FX_SUCK1);
            }else if(hug_throw>4){
                if(pc_facehug_shake_pain_sounds_enabled)TFX(player_type==PT_PREDATOR?AVP_FX_PHIT3:AVP_FX_UHH3A);
            }
        }
        return;
    }
    if(new_wepno>=0 && ((cur_weps&(1u<<new_wepno))==0))new_wepno=0;
    if(!wep_desel&&new_wepno==cur_wepno)new_wepno=0;
    if(!cur_def){if(new_wepno){cur_wepno=new_wepno;new_wepno=0;new_weapon();}return;}
    if(new_wepno)wait_state=0;
    if(!wep_animptr){
        if(new_wepno){wep_desel=0xffu;wep_animptr=cur_def->move_out;mp_equip_phase=1;}
        else if(wep_fire){AvpAmmoInfo*a=&ammo_info[cur_wepno-1];if(a->cur>wep_low)start_action();}
    }
    /* Advance the same authored equipment scripts at two presentation steps
     * per one original 10 Hz game tick. Exactly the ordinary single-player
     * step is used when disabled and during firing/attacks. Each equipment
     * substep advances its original velocity to avoid shortcutting the
     * animation or leaving an offset at the settled weapon pose. */
    {
        unsigned equip_fast=(mp_fast_equip &&
            (player_type==PT_HUMAN || player_type==PT_PREDATOR) &&
            (wep_desel || mp_equip_phase) && wep_animptr);
        unsigned steps=equip_fast?2u:1u;
        for(unsigned step=0;step<steps;step++){
            if(step && (!wep_animptr || !(wep_desel || mp_equip_phase)))break;
            if(wep_animptr){
                if(a1st_time>1){--a1st_time;}else{
                    a1st_time=1;
                    if(wait_state){AvpAmmoInfo*a=&ammo_info[cur_wepno-1];if(a->cur&&wep_fire)wep_animptr=wait_ptr;else wait_state=0;}
                    if(!wait_state){
                        if(!play_frame(&wep_animptr)){
                            if(wep_desel){wep_desel=0;cur_wepno=new_wepno;new_wepno=0;new_weapon();}
                            else{wep_animptr=NULL;frame0();mp_equip_phase=0;}
                        }
                    }else play_frame(&wep_animptr);
                }
            }
            if(step==0)SwingPos();
            wep_x+=wep_xvel;wep_y+=wep_yvel;
        }
    }
    (void)over_id;(void)over_visible;(void)use1_visible;(void)use2_visible;
}

/* Player melee contacts use the shared CLAW2 hit effect, distinct from
 * NPC Alien vocalizations in AMP.S and the attack-start effects.  Only a confirmed successful melee
 * hit should play one of these cues; missing a target remains silent.  Keep
 * this in the PC presentation/audio bridge, outside Jaguar hit geometry. */
unsigned avp_mazescrn_melee_contact_sfx(s16 species,s16 weapon)
{
    if(species==PT_ALIEN){
        switch(weapon){
        case 1:return AVP_FX_CLAW2; /* Alien tail: same contact effect as claw */
        case 2:return AVP_FX_CLAW2; /* Alien swipe fight damage event */
        case 3:return AVP_FX_CLAW2; /* Alien bite: same contact effect as claw */
        default:return 0u;
        }
    }
    if(species==PT_PREDATOR){
        switch(weapon){
        case 1:return AVP_FX_MELEE; /* combi-stick fight damage event */
        case 4:return AVP_FX_CLAW2; /* wrist-blades fight damage event */
        default:return 0u;
        }
    }
    return 0u;
}

void process_shot(void){
    if(fire_damage==0||!cur_def||cur_wepno<=0) return;
    AvpAmmoInfo *a=&ammo_info[cur_wepno-1];
    u16 charge=a->cur;
    if(player_type==PT_HUMAN&&charge){charge--;if(charge==0&&cheat)charge=a->max;a->cur=charge;}
    if(fire_damage>0){s16 dam=fire_damage;if(player_type!=PT_HUMAN){u32 d=(u32)charge*(u16)dam;d>>=14;dam=(s16)((d+(u16)dam)>>1);fire_damage=dam;}AvpAmp*hit=shot_cb?shot_cb(dam,fire_distance,fire_width):TestSpark();if(hit){
        unsigned contact_sfx=avp_mazescrn_melee_contact_sfx(player_type,cur_wepno);
        if(contact_sfx)(void)TFX(contact_sfx);
        if(player_type==PT_ALIEN&&cur_wepno!=2){
            if(cur_wepno==1)hit->flags^=(1u<<AMP_STUN);
            else hit->flags&=(u16)~(1u<<AMP_STUN);
        }
    }}
    fire_damage=0;
}
void recharge(void){if(player_type==PT_HUMAN)return;for(int i=0;i<num_weps&&i<6;i++){AvpAmmoInfo*a=&ammo_info[i];s32 v;if(i==cur_wepno-1&&active_wep)v=(s32)a->cur-a->dec;else v=(s32)a->cur+a->inc;if(v<0)v=0;if(v>a->max)v=a->max;a->cur=(u16)v;}}
void UpdtScrOverlays(void){if(player_type==PT_PREDATOR)PredAvail();update_wep();process_shot();recharge();}

#ifdef AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE
#include "../../new/multiplayer/local_mp_weapon_native.inc"
#endif
