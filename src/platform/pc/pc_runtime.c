#include "avp_runtime.h"
#include "pc/pc_platform.h"
#include "pc/pc_video.h"
#ifdef AVP_LOCAL_MULTIPLAYER
#include "local_mp_input.h"
#include "local_mp_bot.h"
#include "local_mp_move_guard.h"
#endif
#include "pc/pc_audio.h"
#include "collision.h"
#include "player.h"
#include "maze.h"
#include "amp.h"
#include "joypad.h"
#include "mjp.h"
#include "font.h"
#include "computer.h"
#include "avp_sfx_ids.h"
#include "music.h"
#include "levels.h"
#include "hud_message.h"
#include "collectables.h"
#include "dev_cheats.h"
#include "weapons.h"
#include "hud.h"
#include "avp_resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void noop(void *u){(void)u;}
static void noopu(void *u,unsigned v){(void)u;(void)v;}
/* Original MAZE/AMP PLAYER `sfx` events use AVPSOUND's effect allocator.
 * A raw mixer launch evades priority stealing, logical voice lifetime, and
 * handle completion.  The handle callback below goes straight to the DSP
 * mirror, so directing ordinary events to TFX does not recurse. */
static void pcsfx(void *u,unsigned v){(void)u;(void)TFX(v);}
static void pcsfxp(void*u,unsigned id,s32 l,s32 r)
{
    (void)u;
    /* AMP.S::generate .noise updates the existing generator alarm's two
     * DSP words.  It does NOT issue a new SFX event on every AI tick. */
    if(id==AVP_FX_FORCE)avp_pc_audio_update_generator_alarm(l,r);
    else if(id==AVP_FX_XP1B1)avp_pc_audio_play_explosion(l);
    else avp_pc_audio_play(id,l,r);
}
static void pcsfxh(void*u,unsigned id,u32 h,unsigned voice){(void)u;avp_pc_audio_play_handle(id,h,voice,0,0);}
static void pcstopsfx(void*u,u32 h){(void)u;avp_pc_audio_stop_handle(h);}
static int pc_sfx_handle_active(void*u,u32 h){(void)u;return avp_pc_audio_handle_active(h);}
static int pc_sfx_voice_active(void*u,unsigned voice){(void)u;return avp_pc_audio_voice_active(voice);}
static void pcunloopsfx(void*u,u32 h){(void)u;avp_pc_audio_unloop_handle(h);}
static void pcmodsfx(void*u,u32 h,s32 v){(void)u;avp_pc_audio_mod_handle(h,v);}
static void pcpossfx(void*u,u32 h,s32 x,s32 y){(void)u;avp_pc_audio_position_handle(h,x,y);}
static void pckill(void *u){(void)u;avp_pc_audio_stop_all();}
static void pcpausefxsave(void *u){(void)u;avp_pc_audio_pause_save();}
static void pcpausefxrestore(void *u,u32 weapon,u32 alarm)
{(void)u;avp_pc_audio_pause_restore(weapon,alarm);}
static void pcambient(void *u){(void)u;avp_pc_audio_start_ambient((unsigned)(cur_level<1?1:cur_level));}
static void pcstartmusic(void *u,unsigned id){(void)u;avp_pc_audio_start_music(id);}
static void pcstopmusic(void *u){(void)u;avp_pc_audio_stop_music();}
static void pcsetmaster(void *u,u32 volume){(void)u;avp_pc_audio_set_master_volume(volume);}
static void pckillambient(void *u){(void)u;avp_pc_audio_stop_ambient();}
static void pc_pred_weapon_notice(enum AvpWeaponNotice notice)
{
    const AvpHudMessageStep *steps=NULL;
    switch(notice){
    case AVP_WNOTICE_GAIN_COMBI:steps=avp_msg_gain_combi;break;
    case AVP_WNOTICE_LOSE_COMBI:steps=avp_msg_lose_combi;break;
    case AVP_WNOTICE_GAIN_DISC:steps=avp_msg_gain_disc;break;
    case AVP_WNOTICE_LOSE_DISC:steps=avp_msg_lose_disc;break;
    case AVP_WNOTICE_GAIN_CANNON:steps=avp_msg_gain_cannon;break;
    case AVP_WNOTICE_LOSE_CANNON:steps=avp_msg_lose_cannon;break;
    default:break;
    }
    if(steps)avp_hudmsg_queue(steps); /* MAZESCRN.S:1450/1476: unconditional */
}
static void pc_pred_weapon_sfx(unsigned cue)
{
    if(cue==1u)(void)TFX(AVP_FX_PHIT3);
    else if(cue==2u)(void)TFX(AVP_FX_LAFF);
}
static void pc_object_event(void*u,unsigned e,u32 a,u32 b,u32 c){
    if(e==0x201u && u && avp_pc_video_tracker_sounds_enabled((AvpPcVideo*)u)){
        if(c&1u)avp_pc_audio_play_tracker(AVP_FX_TRAC1,tracker_audio_state.volume,
            tracker_audio_state.pitch,tracker_audio_state.env_rate,tracker_audio_state.mod_depth);
        if(c&2u)(void)TFX(AVP_FX_TRAC2);
    }
    if(u)avp_pc_video_object_event((AvpPcVideo*)u,e,a,b,c);
}
static void pc_file_event(void*u,unsigned e,u32 a,u32 b,u32 c){
    (void)u;(void)b;(void)c;
    /* MAZE.S::PlayAvP::load_complete releases the looped airlock/lift
     * effect at the destination. Preserve its original handle-specific
     * StopLoop behavior rather than stopping shared multiplayer audio. */
    if(e==0x554e4c50u){StopLoop(a);return;} /* 'UNLP' */
    /* The original non-destruct level entry also clears the alarm. */
    if(e==0x4b414c4du)KillAlar(); /* 'KALM' */
}
static void pc_reset_map(void *u)
{
    if(u)avp_pc_video_reset_automap((AvpPcVideo*)u);
}
static void wait_blitter(void *u){(void)u;}

static int pc_noclip;
#ifdef AVP_LOCAL_MULTIPLAYER
/* Only during the currently borrowed local-multiplayer actor movement tick.
 * Never active in original single-player or while a menu is open. */
static int pc_mp_bot_floor_guard,pc_mp_player_floor_guard,pc_mp_fast_sweep_guard;
void avp_pc_runtime_set_mp_bot_floor_guard(int enabled)
{pc_mp_bot_floor_guard=!!enabled;}
void avp_pc_runtime_set_mp_player_floor_guard(int enabled)
{pc_mp_player_floor_guard=!!enabled;}
void avp_pc_runtime_set_mp_fast_sweep_guard(int enabled)
{pc_mp_fast_sweep_guard=!!enabled;}
#endif
static const AvpRomImage *pc_rom;
static unsigned pc_front_mode;
static int pc_gameplay_vblank;
#ifdef AVP_LOCAL_MULTIPLAYER
static int pc_mp_terminal_owner=-1;
static unsigned pc_mp_terminal_players;
static int pc_mp_terminal_initial_use_held;
void avp_pc_runtime_set_mp_terminal(int player,unsigned players)
{
    pc_mp_terminal_owner=(player>=0&&players>=1u&&players<=4u &&
                          (unsigned)player<players)?player:-1;
    pc_mp_terminal_players=(pc_mp_terminal_owner>=0)?players:0u;
    pc_mp_terminal_initial_use_held=(pc_mp_terminal_owner>=0);
}
#else
void avp_pc_runtime_set_mp_terminal(int player,unsigned players)
{(void)player;(void)players;}
#endif
static u8 *pc_rdb_data;
static size_t pc_rdb_size;
static AvpPcVideo *pc_video_for_hud;
static void pc_hud_clear(void)
{
    if(pc_video_for_hud)avp_pc_video_hud_message_clear(pc_video_for_hud);
}
static void pc_collect_notice(enum AvpCollectNotice notice,unsigned detail)
{
    if(notice==AVP_CNOTICE_MTRACKER)avp_dev_cheats_tracker_collected();
    avp_hudmsg_pickup(notice,detail);
}

void avp_pc_runtime_set_rom(const AvpRomImage *rom){pc_rom=rom;}
void avp_pc_runtime_set_gameplay_vblank(int active){pc_gameplay_vblank=active?1:0;}

static void pc_hud_line(unsigned line,const char *text,unsigned x_pixels)
{
    if(!pc_video_for_hud)return;
    avp_pc_video_hud_message_line(pc_video_for_hud,line,text,x_pixels);
}
static void pc_set_message(void *u,unsigned id)
{
    (void)u;
    switch(id){
    case 1u: avp_hudmsg_queue(avp_msg_dead); break;
    case 2u: avp_hudmsg_queue(avp_msg_usecocoon); break;
    case 3u:
        avp_hudmsg_set_destination_level((unsigned)(new_level>0?new_level:cur_level));
        avp_hudmsg_queue(avp_msg_newlevel);
        break;
    case 4u:
        avp_hudmsg_set_destination_level((unsigned)(new_level>0?new_level:cur_level));
        avp_hudmsg_queue(avp_msg_newduct);
        break;
    case 5u: avp_hudmsg_queue(avp_msg_cheat_on); break;
    case 6u: avp_hudmsg_queue(avp_msg_cheat_off); break;
    case 7u: avp_hudmsg_queue(avp_msg_stun); break;
    case 8u: avp_hudmsg_queue(avp_msg_cocooned); break;
    case 9u: avp_hudmsg_queue(avp_msg_ccnfail); break;
    default: break;
    }
}

#ifdef AVP_LOCAL_MULTIPLAYER
static int pc_frontend_mp_launch_pending(void *u)
{
    return avp_pc_video_local_mp_launch_pending((const AvpPcVideo*)u);
}
static int pc_frontend_mp_menu_open(void *u)
{
    return avp_pc_video_title_mp_menu_open((const AvpPcVideo*)u);
}
#endif
/* PC-only computer pad callback, including per-player co-op ownership. */
static void pc_terminal_poll(AvpPcVideo *v)
{
#ifdef AVP_LOCAL_MULTIPLAYER
    if(pc_mp_terminal_owner>=0){
        u32 pad=UINT32_MAX;int quit=0;
        (void)avp_pc_video_local_mp_terminal_poll(v,
           (unsigned)pc_mp_terminal_owner,pc_mp_terminal_players,&pad,&quit);
        if(quit){avp_pc_video_destroy(v);exit(0);}
        pad=avp_mp_terminal_initial_use_guard(pad,&pc_mp_terminal_initial_use_held);
        avp_pc_pad_set(pad);
        return;
    }
#endif
    u32 pad=UINT32_MAX;int quit=0;
    avp_pc_video_set_frontend_input(v,1);
    avp_pc_video_poll(v,&pad,&quit);
    avp_pc_video_set_frontend_input(v,0);
    if(quit){avp_pc_video_destroy(v);exit(0);}
    avp_pc_pad_set(pad);
}
static void pc_frontend_poll(AvpPcVideo *v)
{
    u32 pad=0xffffffffu;int quit=0;
    if(!v)return;
    avp_pc_video_set_frontend_input(v,1);
    avp_pc_video_poll(v,&pad,&quit);
    avp_pc_video_set_frontend_input(v,0);
    if(quit){avp_pc_video_destroy(v);exit(0);}
    avp_pc_pad_set(pad);
}
/* The original COMPUTER.S polls the joypad without waiting for a VBL while
 * held A/B is being released.  Refresh the host's active-low pad at every
 * such scan; otherwise the CPU can spin forever on a stale key snapshot.
 * This is host input plumbing only: COMPUTER.S and the pad state machine are
 * unchanged.  Temporarily route keyboard/gamepad as a native Jaguar pad so
 * A/B/C, arrows and Escape-as-C retain the original terminal semantics. */
static void pc_computer_poll_input(void)
{
    AvpPcVideo *v=pc_video_for_hud;
    if(!v||!avp_computer_active())return;
    pc_terminal_poll(v);
    avp_pc_sleep_ns(1000000ull); /* Avoid unbounded CPU spin on held buttons. */
}

static void wait_vblank(void *u)
{
    AvpPcVideo *v=(AvpPcVideo*)u;
    if(v){
        if(pc_gameplay_vblank){
            if(avp_computer_active()){
                /* The terminal is an original Jaguar A/B/C modal UI, even
                 * when the user's gameplay controller uses PC bindings. */
                pc_terminal_poll(v);
            }else{
                u32 pad=0xffffffffu;int quit=0;
                avp_pc_video_set_frontend_input(v,0);
                avp_pc_video_poll(v,&pad,&quit);
                if(quit){avp_pc_video_destroy(v);exit(0);}
                avp_pc_pad_set(pad);
            }
            if(avp_pc_video_pause_active(v))avp_pc_video_present_pause(v);
            else avp_pc_video_present_gameplay(v);
        }else{
            pc_frontend_poll(v);
            if(avp_pc_video_pause_active(v))avp_pc_video_present_pause(v);
            else avp_pc_video_frontend_tick(v,pc_front_mode,Choice);
        }
    }
    avp_pc_sleep_ns(16666667ull);
}
/* The original SafePos remains the authority on wall/door clearance.
 * Multiplayer player/bot floor guards add only authored floor bounds, not
 * another made-up collision geometry.  A noclip actor bypasses this callback
 * through the unmodified MovePlayer() branch. */
static int pc_mp_point_safe(s32 x,s32 y,s16 height,u16 width)
{
    int original=(int)SafePos((u8)height,x,y,width);
#ifdef AVP_LOCAL_MULTIPLAYER
    if(original==AVP_COLL_SAFE && pc_mp_bot_floor_guard &&
       !avp_mp_bot_playable_position(avp_collision_maze_data(),
           avp_collision_maze_width(),avp_collision_maze_height(),
           (u32)x,(u32)y))return AVP_COLL_WALL;
    if(original==AVP_COLL_SAFE && pc_mp_player_floor_guard &&
       !avp_mp_move_guard_floor(avp_collision_maze_data(),
           avp_collision_maze_width(),avp_collision_maze_height(),
           x,y,(unsigned)width<<9))return AVP_COLL_WALL;
#endif
    return original;
}
#ifdef AVP_LOCAL_MULTIPLAYER
typedef struct PcMpMovementProbe { s16 height;u16 width; } PcMpMovementProbe;
static int pc_mp_movement_probe(s32 x,s32 y,void *user)
{
    const PcMpMovementProbe *probe=(const PcMpMovementProbe *)user;
    return pc_mp_point_safe(x,y,probe->height,probe->width);
}
#endif
static int pc_safe_pos(void *u,s32 x,s32 y,s16 height,u16 width)
{
    (void)u;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(pc_mp_fast_sweep_guard){
        const PcMpMovementProbe probe={height,width};
        /* Original MovePlayer() checks its endpoint and possible slide
         * alternatives. Check the intermediate points of each SAME proposed
         * displacement at <=1/8 retail cell. The sweep does not alter speed,
         * acceleration, wall definitions or the original 10Hz tick count. */
        return avp_mp_move_guard_sweep((s32)x_pos,(s32)y_pos,x,y,
                     0x2000u,pc_mp_movement_probe,(void *)&probe);
    }
#endif
    return pc_mp_point_safe(x,y,height,width);
}
static int pc_amp_collision(void *u,s32 x,s32 y)
{
    (void)u;return !AMPCollisions(x,y);
}
static int pc_player_noclip(void *u){(void)u;return pc_noclip;}
void avp_pc_runtime_set_noclip(int enabled){pc_noclip=enabled?1:0;}
int avp_pc_runtime_get_noclip(void){return pc_noclip;}

static int pc_is_computer_event(unsigned e)
{
    switch(e){
    case 0x4350414cu:case 0x434c53u:case 0x434f31u:case 0x434f32u:case 0x43555253u:
    case 0x484cu:case 0x564cu:case 0x5343484du:case 0x5741524eu:case 0x44455354u:
    case 0x47454e4du:case 0x41524d4du:case 0x4d45444cu:case 0x4d45444du:case 0x46414944u:
    case 0x504f444eu:case 0x504f444du:case 0x4c41554eu:case 0x4152534eu:case 0x53504543u:
    case 0x50474654u:
        return 1;
    default:return 0;
    }
}
static const char *pc_front_text(void *u,u32 address);
/* These are the original multi-line RDB.DTA briefing records selected by
 * MJP's SELP event. They are pointers into the user's verified retail ROM,
 * not new prose or gameplay-owned text. */
static u32 pc_select_brief_address(s32 choice)
{
    if(choice==4)return 0x0002B5F1u;  /* Alien */
    if(choice==8)return 0x0002B65Cu;  /* Predator */
    return 0x0002B538u;              /* Colonial Marine */
}
static void pc_front_event(void *u,unsigned e,s32 a,s32 b)
{
    AvpPcVideo *v=(AvpPcVideo*)u;
    /* In the retail cartridge, the pre-game text renderer/loading work
     * completes before the SELECT sound and PRESS FIRE prompt. R172's PC
     * SELP resource callback was instantaneous, so End_Sele could accept a
     * second Fire while its video text was still appearing. Restore only
     * the missing host presentation synchronization here: the original
     * MJP/MAIN functions and their input/audio sequencing stay unmodified.
     * TICK callbacks during One_Tick do not enter this branch. */
    if(v&&e==0x53454c53u) /* SELS: first post-load select sound */
        while(!avp_pc_video_frontend_select_loading_ready(v))One_Tick();
    if(v)avp_pc_video_frontend_event(v,e,a,b);
    /* Retail MJP starts the selection score before its $FA fade-up.
     * Do not replay it at SOBJ #4: that formerly restarted every envelope
     * after the master fade had already reached full volume. */
    if(v&&e==0x53454c45u){ /* SELE: original selection instructions */
        avp_pc_video_frontend_select_instructions(v,
            pc_front_text(u,0x0002B700u),pc_front_text(u,0x0002B71Bu));
    }
    if(v&&e==0x53454c50u){ /* SELP: selected campaign briefing + loading */
        avp_pc_video_frontend_select_briefing(v,
            pc_front_text(u,pc_select_brief_address(a)),
            pc_front_text(u,0x0002B737u));
    }
    if(v&&e==0x53454c54u){ /* SELT: authored bottom PRESS FIRE prompt */
        avp_pc_video_frontend_select_prompt(v,pc_front_text(u,(u32)a));
    }
    if(v){
        /* Each of these FONT.S coordinate records comes from the verified
         * cartridge RDB.DTA ($20300 base). No replacement English or artwork
         * is carried in the PC build. Rendering alone changes here: original
         * Computer() control, page progression, clearance and destruct state
         * remain owned by the Jaguar-derived game layer. */
        u32 menu=0u,overlay=0u;
        if(e==0x47454e4du){
            menu=0x2651cu;                        /* MAIN MENU: schematic/log */
            if(a>=3)overlay=0x2594cu;            /* initiate emergency destruct */
        }
        else if(e==0x41524d4du)menu=0x25ea0u;   /* arsenal / lieutenant log */
        else if(e==0x4d45444cu)menu=0x264dcu;   /* medical log / species */
        else if(e==0x4d45444du)menu=0x25feeu;   /* first aid / medical log */
        else if(e==0x504f444du)menu=0x26558u;   /* escape capsule menu */
        else if(e==0x504f444eu)menu=0x29272u;   /* inactive escape capsule */
        else if(e==0x5741524eu)menu=0x25caeu;   /* complete warning and abort */
        else if(e==0x44455354u)menu=0x25dc6u;   /* full armed/countdown notice */
        else if(e==0x4c41554eu)menu=0x292f0u;   /* launch sequence initiated */
        else if(e==0x5343484du)menu=0x25998u;   /* C.EXIT beneath schematic */
        else if(e==0x50474654u)menu=0x259feu;   /* A.NEXT B.PREVIOUS C.EXIT */
        else if(e==0x4152534eu){                 /* original arsenal title */
            static const u32 weap[]={0x25f08u,0x25f1au,0x25f2cu,0x25f3au};
            if(a>=0&&a<4)menu=weap[a];
            overlay=0x25edau;                    /* arsenal page keys */
        }
        else if(e==0x53504543u){                 /* six shipped creature records */
            static const u32 species[]={0x2612au,0x2619eu,0x26220u,0x262a6u,0x26318u,0x26414u};
            if(a>=0&&a<6)menu=species[a];
            overlay=0x25a28u;                    /* species page keys */
        }
        else if(e==0x46414944u){
            /* The historical first-aid result streams are deliberately kept
             * as authored (including the cartridge's static 75% lettering). */
            if(a>0)menu=0x260d8u;                /* restoration text */
            else if(a<0)menu=0x26100u;          /* unavailable */
            else menu=b==1000?0x2609eu:0x260b2u;/* healthy / higher than boost */
        }
        if(menu){
            const char *record=pc_front_text(u,menu);
            if(record){
                /* FONT.S, not a host pre-render, owns typewriter mode ($FE/$FD),
                 * the original three-VBlank-per-character cadence, blip SFX,
                 * glyph positioning and A/B/C interruption.  The GPU event
                 * has already cleared only the affected CTEXT layer. */
                pr_string(record);
            }
        }
        /* COMPUTER.S prints the original fixed B.SELECT/C.EXIT prompt after
         * each top-level menu; do not substitute A.NEXT/B.PREVIOUS here. */
        if(e==0x47454e4du||e==0x41524d4du||e==0x4d45444cu||
           e==0x4d45444du||e==0x504f444du){
            /* The escape capsule's retail prompt is B.LAUNCH, unlike the
               other terminals' B.SELECT/C.EXIT prompt. */
            u32 keys=e==0x504f444du?0x29396u:0x25972u;
            const char *record=pc_front_text(u,keys);
            if(record)pr_string(record);
        }
        if(overlay){
            const char *record=pc_front_text(u,overlay);
            if(record)pr_string(record);
        }
    }
    /* The original MJP character-confirmation program emits SELS with the
     retail TYP1B descriptor ($221B8).  It is the same sound dispatch
     as MJPA/typewriter events; R140 only forwarded the latter and
     silently discarded the original confirmation cue. */
    if((e==0x4d4a5041u || e==0x53454c53u) && a>=0x20300)
        avp_pc_audio_play_priority((unsigned)a,b); /* event arg = TFX D1 priority word */
    if(e==0x5545564fu)avp_pc_audio_set_master_volume(a<0?0u:(u32)a);
    if(v&&pc_is_computer_event(e)){
        avp_pc_video_present_gameplay(v);
        return;
    }
    /* MJP select_emit_objects publishes its OP carousel phrases in the
       strict slot order 0, 2, 4. The original OP sees one completed list;
       repainting after the first or second SOBJ exposes mixed old/new poses
       or a one-portrait flash on initial entry. Keep all three events in the
       Jaguar presentation state, but display only after final slot 4. */
    if(e==0x534f424au && (((u32)a>>16)&0xffffu)!=4u)return;
    /* TICK is presented from wait_vblank, before this event arrives.  Present
       transition/state changes once more so the new resource is immediately visible. */
    if(v&&e!=0x5449434bu)avp_pc_video_frontend_tick(v,pc_front_mode,Choice);
}
/* Captured BigPEmu visual cadence: title fade (~40 VBLs), then
 * approximately 188 VBLs until the first portrait. MJP mode 1 now runs
 * the missing 132-retail-callback fade-up BEFORE its portrait generation.
 * Account for those original callbacks inside the existing approximate
 * black delay, rather than adding a new 132-VBL delay to the transition.
 * 188 is still video-derived and not a verified original timer constant. */
#define PC_SELECT_FADE_VBLS 40u
#define PC_SELECT_BLACK_TOTAL_VBLS 188u
#define PC_SELECT_ORIGINAL_FADE_VBLS 132u
#define PC_SELECT_BLACK_VBLS (PC_SELECT_BLACK_TOTAL_VBLS-PC_SELECT_ORIGINAL_FADE_VBLS)
static void pc_front_title_to_select(AvpPcVideo *v)
{
    if(!v)return;
    for(unsigned i=0;i<=PC_SELECT_FADE_VBLS;i++){
        unsigned phase=(PC_SELECT_FADE_VBLS-i)*16u/PC_SELECT_FADE_VBLS;
        unsigned volume=(PC_SELECT_FADE_VBLS-i)*0x7fffu/PC_SELECT_FADE_VBLS;
        avp_pc_video_frontend_event(v,0x54534644u,(s32)phase,0);
        avp_pc_audio_set_master_volume(volume);
        avp_pc_video_frontend_tick(v,AVP_MJP_TITLE,Choice);
        pc_frontend_poll(v);
        avp_pc_sleep_ns(16666667ull);
    }
    StopMusi();
    /* Host's earlier title fade modified the DSP mixer directly, not the
     * music.c CPU mirror. Match the zero written by that transition so the
     * source-authored mode-1 fade-up advances from silence, not $7FFF. */
    fx_fade_level=0;
    fx_fade_target=0;
    for(unsigned i=0;i<PC_SELECT_BLACK_VBLS;i++){
        avp_pc_video_frontend_tick(v,AVP_MJP_TITLE,Choice);
        pc_frontend_poll(v);
        avp_pc_sleep_ns(16666667ull);
    }
}
/* In the user's retail 2026-10-21 BigPEmu recording, HIGH SCORES is
 * preceded by a black title departure, then the engraved picture brightens
 * and the records rise from below.  The host cannot expose the new MJP image
 * over the still-visible title while its FILES assets are loading.  Render
 * only this TITLE -> FAME departure here; post-game LOSE already has the
 * recovered eight-tick MJP transitions and must not be double-faded. */
static void pc_front_title_to_fame(AvpPcVideo *v)
{
    enum { FADE_VBLS=40u, BLACK_VBLS=10u };
    if(!v)return;
    for(unsigned i=0u;i<=FADE_VBLS;i++){
        const unsigned phase=(FADE_VBLS-i)*16u/FADE_VBLS;
        avp_pc_video_frontend_event(v,0x54534644u,(s32)phase,0);
        avp_pc_video_frontend_tick(v,AVP_MJP_TITLE,Choice);
        pc_frontend_poll(v);
        avp_pc_sleep_ns(16666667ull);
    }
    for(unsigned i=0u;i<BLACK_VBLS;i++){
        avp_pc_video_frontend_tick(v,AVP_MJP_TITLE,Choice);
        pc_frontend_poll(v);
        avp_pc_sleep_ns(16666667ull);
    }
}
/* BigPEmu retail Hall-of-Fame exit (2026-10-21 15-32-16): fade begins
 * just after 50.2s and reaches black around 50.9s, approximately 40 VBLs.
 * This host video effect runs before the next title assets are exposed.
 * The original MJP choice/game logic and score storage are unchanged. */
static void pc_front_fame_to_title(AvpPcVideo *v)
{
    enum { FADE_VBLS=40u };
    if(!v)return;
    for(unsigned i=0u;i<=FADE_VBLS;i++){
        const unsigned phase=(FADE_VBLS-i)*16u/FADE_VBLS;
        avp_pc_video_frontend_event(v,0x46534644u,(s32)phase,0);
        avp_pc_video_frontend_tick(v,AVP_MJP_FAME,Choice);
        pc_frontend_poll(v);
        avp_pc_sleep_ns(16666667ull);
    }
}
static void pc_front_load(void *u,unsigned mode,unsigned file_index,unsigned role)
{
    AvpPcVideo *v=(AvpPcVideo*)u;
    if(v&&mode==AVP_MJP_SELECT&&pc_front_mode==AVP_MJP_TITLE)
        pc_front_title_to_select(v);
    if(v&&mode==AVP_MJP_FAME&&pc_front_mode==AVP_MJP_TITLE)
        pc_front_title_to_fame(v);
    if(v&&mode==AVP_MJP_TITLE&&pc_front_mode==AVP_MJP_FAME)
        pc_front_fame_to_title(v);
    pc_front_mode=mode;
    if(v&&pc_rom){
        (void)avp_pc_video_frontend_load(v,pc_rom,mode,file_index,role);
        /* FILES 235 is the original small MJP font used for the bottom
         * title copyright. Its presentation-only resource load does not
         * alter the original MJP title file list or CPU state. */
        if(mode==AVP_MJP_TITLE && file_index==238u)
            (void)avp_pc_video_frontend_load(v,pc_rom,mode,235u,8u);
    }
}
static void pc_draw_text(void *u,unsigned line,const char *text,unsigned x)
{
    if(u)avp_pc_video_frontend_draw_text((AvpPcVideo*)u,line,text,x);
}
static void pc_draw_font_glyph(void *u,unsigned brush,unsigned source_x,unsigned width,
                               unsigned x,unsigned y,int clear)
{
    if(u)avp_pc_video_computer_draw_glyph((AvpPcVideo*)u,brush,source_x,width,x,y,clear);
}
static int pc_transition_done(void *u,unsigned phase){return avp_pc_video_frontend_transition_done((AvpPcVideo*)u,phase);}

/* Retail MJP strings live in RDB.DTA.  The cartridge stores that exact data
   as the gzip stream whose source label is DATA in ROM.RSP.  On the canonical
   4 MiB retail image the gzip header begins at ROM offset $E840; decompress it
   lazily from the user's ROM and resolve historical RAM addresses relative to
   RDB.DTA's source load base $20300. */
static int ensure_rdb_data(void)
{
    if(pc_rdb_data)return 1;
    if(!pc_rom||!pc_rom->data||!pc_rom->exact_retail_sha256)return 0;
    return avp_resources_decode_rdb(pc_rom,&pc_rdb_data,&pc_rdb_size);
}

static const char *pc_front_text(void *u,u32 address)
{
    (void)u;
    if(!ensure_rdb_data())return NULL;
    return avp_resources_rdb_address(pc_rdb_data,pc_rdb_size,address);
}


void avp_pc_bind_runtime(void *user)
{
    AvpRuntimeOps o;pc_video_for_hud=(AvpPcVideo*)user;avp_hudmsg_set_draw_callback(pc_hud_line);avp_hudmsg_set_clear_callback(pc_hud_clear);avp_collect_set_callbacks(pc_collect_notice,NULL);avp_weapons_set_callbacks(pc_pred_weapon_notice,pc_pred_weapon_sfx);memset(&o,0,sizeof(o));o.user=user;
    o.wait_vblank=wait_vblank;o.wait_blitter=wait_blitter;o.screen_off=noop;o.set_message=pc_set_message;o.set_maze_list=noop;o.restore_maze_list=noop;o.swap_screens=noop;o.pre_frame=noop;o.post_frame=noop;o.pause=noop;
    o.gpu_draw_screen=noop;o.gpu_reset_maze=noop;o.gpu_init_overlays=noop;o.gpu_reset_overlays=noop;o.gpu_load_base=noop;o.gpu_load_overlay=noopu;o.gpu_fix_overlay=noopu;o.init_screen_overlays=noop;o.init_sprites=noop;o.reset_sprites=noop;o.init_computer=noop;o.reset_map=pc_reset_map;
    o.ambient=pcambient;o.kill_ambient=pckillambient;o.kill_sounds=pckill;o.pause_sfx_save=pcpausefxsave;o.pause_sfx_restore=pcpausefxrestore;o.play_sfx=pcsfx;o.play_sfx_params=pcsfxp;o.play_sfx_handle=pcsfxh;o.sfx_handle_active=pc_sfx_handle_active;o.sfx_voice_active=pc_sfx_voice_active;o.stop_sfx=pcstopsfx;o.unloop_sfx=pcunloopsfx;o.mod_sfx=pcmodsfx;o.position_sfx=pcpossfx;o.start_music=pcstartmusic;o.stop_music=pcstopmusic;o.set_master_volume=pcsetmaster;
    o.frontend_event=pc_front_event;o.frontend_transition_done=pc_transition_done;o.frontend_text=pc_front_text;o.frontend_load_resource=pc_front_load;o.draw_text=pc_draw_text;o.draw_font_glyph=pc_draw_font_glyph;
#ifdef AVP_LOCAL_MULTIPLAYER
    o.frontend_mp_launch_pending=pc_frontend_mp_launch_pending;
    o.frontend_mp_menu_open=pc_frontend_mp_menu_open;
#endif
    o.object_event=pc_object_event;o.file_event=pc_file_event;o.safe_pos=pc_safe_pos;o.amp_collision=pc_amp_collision;o.player_noclip=pc_player_noclip;
    avp_runtime_bind(&o);
    avp_pc_pad_set_poll_callback(pc_computer_poll_input);
    /* The sound allocator reads module priority from the original retail
     * RDB descriptors; bind it before InitSynt / the first gameplay SFX.
     * Missing or unverified ROM resources retain the existing fallback. */
    if(ensure_rdb_data())avp_music_bind_retail_sfx_descriptors(pc_rdb_data,pc_rdb_size);
    else avp_music_bind_retail_sfx_descriptors(NULL,0u);
}
