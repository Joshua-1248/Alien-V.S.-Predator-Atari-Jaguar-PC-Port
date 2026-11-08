#include "pc/pc_platform.h"
#include "pc/pc_resources.h"
#include "pc/pc_runtime.h"
#include "pc/pc_video.h"
#include "pc/pc_original_cheats.h"
#include "map_maker.h"
#include "map_maker_objects.h"
#include "map_maker_playtest.h"
#include "pc/pc_input_routing.h"
#include "pc/pc_analog_aim.h"
#include "pc/pc_live_heading.h"
#include "pc/pc_turn_presentation.h"
#include "pc/pc_quick_turn.h"
#include "pc/pc_fade_clock.h"
#include "jaguar/avp_gpu/presentation_interp.h"
#include "pc/pc_resources.h"
#include "pc/pc_audio.h"
#include "jaguar/host/fullsynth_host.h"
#include "pc/pc_eeprom.h"
#include "pc/pc_quickstate.h"
#include "collision.h"
#include "maze.h"
#include "gpu_overlays.h"
#include "player.h"
#include "amp.h"
#include "doors.h"
#include "joypad.h"
#include "jaguar.h"
#include "levels.h"
#include "weapons.h"
#include "sprites.h"
#include "mazescrn.h"
#include "pc/pc_marine_start.h"
#include "objects.h"
#include "hud.h"
#include "hud_message.h"
#include "computer.h"
#include "font.h"
#include "savegame.h"
#include "main_game.h"
#include "eeprom.h"
#include "dev_cheats.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
#include "music.h"
#ifdef AVP_LOCAL_MULTIPLAYER
#include "local_mp_walktest.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif
#include <stdarg.h>
#include <math.h>
#include <signal.h>
#ifdef _WIN32
#include "pc/win32_posix.h"
#else
#include <unistd.h>
#endif
#include <limits.h>
#ifndef _WIN32
#include <dirent.h>
#endif
#include <sys/stat.h>

#define DIAG_W 16
#define DIAG_H 16
static u8 diag_maze[DIAG_W*DIAG_H*8];
static void make_diag_maze(void)
{
    unsigned x,y;memset(diag_maze,0,sizeof(diag_maze));
    for(y=0;y<DIAG_H;y++)for(x=0;x<DIAG_W;x++){u8*c=&diag_maze[(y*DIAG_W+x)*8u];c[4]=1;c[5]=1;if(x==0)c[0]=1;if(y==0)c[1]=1;if(x==DIAG_W-1)c[2]=1;if(y==DIAG_H-1)c[3]=1;}
    for(y=2;y<13;y++){u8*c=&diag_maze[(y*DIAG_W+7u)*8u];c[2]=1;diag_maze[(y*DIAG_W+8u)*8u+0]=1;}
    for(x=3;x<12;x++){u8*c=&diag_maze[(9u*DIAG_W+x)*8u];c[3]=1;diag_maze[((10u)*DIAG_W+x)*8u+1]=1;}
}
static FILE *pc_log;
/* Host HUD transition memory must reset when retail cheat state resets. */
static int pc_original_god_last;

static void pc_countdown_cue(AvpCountdownCue cue)
{
    unsigned id=0;
    switch(cue){
    case AVP_COUNT_CUE_2MIN:id=AVP_FX_2MINB;break; case AVP_COUNT_CUE_1MIN:id=AVP_FX_1MINB;break;
    case AVP_COUNT_CUE_10:id=AVP_FX_10B;break; case AVP_COUNT_CUE_9:id=AVP_FX_9B;break;
    case AVP_COUNT_CUE_8:id=AVP_FX_8B;break; case AVP_COUNT_CUE_7:id=AVP_FX_7B;break;
    case AVP_COUNT_CUE_6:id=AVP_FX_6B;break; case AVP_COUNT_CUE_5:id=AVP_FX_B5;break;
    case AVP_COUNT_CUE_4:id=AVP_FX_B4;break; case AVP_COUNT_CUE_3:id=AVP_FX_B3;break;
    case AVP_COUNT_CUE_2:id=AVP_FX_B2;break; case AVP_COUNT_CUE_1:id=AVP_FX_B1;break; default:break;
    }
    if(id)avp_pc_audio_play(id,0,0);
}
static void pc_alarm(int start)
{
    /* HUD.S starts FX_newalarm on AVPSOUND's reserved alarm voice.  Do not
       stop the entire mixer when the countdown finishes: that used to silence
       other weapons, creature sounds and level ambience as well.  The
       original StartAla/KillAlar CPU code already owns voice reservation and
       the unloop request; the hosted FullSynth supplies its ROM descriptor. */
    if(start)StartAla(AVP_FX_NEWAL);
    else KillAlar();
}

static volatile sig_atomic_t crash_stage;
static volatile unsigned long long crash_tick;
static char auto_rom_path[PATH_MAX];
static char default_log_path[PATH_MAX];

static const char *auto_log_path(void)
{
    char exe[PATH_MAX]; ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1); char *slash; const char suffix[]="/avp_pc.log"; size_t dl;
    if(n<=0 || (size_t)n>=sizeof(exe)) return "avp_pc.log";
    exe[n]=0; slash=strrchr(exe,'/'); if(!slash) return "avp_pc.log"; *slash=0; dl=strlen(exe);
    if(dl+sizeof(suffix)>sizeof(default_log_path)) return "avp_pc.log";
    memcpy(default_log_path,exe,dl); memcpy(default_log_path+dl,suffix,sizeof(suffix)); return default_log_path;
}

static void crash_handler(int sig)
{
    if(pc_log){fprintf(pc_log,"FATAL signal %d at stage %d tick %llu\\n",sig,(int)crash_stage,crash_tick);fflush(pc_log);}
    fprintf(stderr,"FATAL signal %d at stage %d tick %llu\\n",sig,(int)crash_stage,crash_tick);fflush(stderr);
    signal(sig,SIG_DFL);raise(sig);
}
static int ends_jag(const char *n)
{
    size_t z=strlen(n); if(z<4)return 0;
    return n[z-4]=='.' && (n[z-3]=='j'||n[z-3]=='J') && (n[z-2]=='a'||n[z-2]=='A') && (n[z-1]=='g'||n[z-1]=='G');
}
static const char *auto_find_rom(const char *argv0)
{
    char exe[PATH_MAX],dir[PATH_MAX]; ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1); int found=0;
#ifndef _WIN32
    DIR *d; struct dirent *e;
#endif
    (void)argv0;
    if(n<=0 || (size_t)n>=sizeof(exe)) return NULL;
    exe[n]=0;
    {char *slash=strrchr(exe,'/'); if(!slash)return NULL; *slash=0; snprintf(dir,sizeof(dir),"%s",exe);}
#ifdef _WIN32
    {
        char glob[PATH_MAX];WIN32_FIND_DATAA data;HANDLE h;
        static const char suffix[]="/*.jag";
        size_t dl=strlen(dir);
        /* A long executable directory cannot fit the search wildcard.
           Reject it rather than silently truncating the path.  This also
           avoids GCC 16's -Wformat-truncation warning under -Werror. */
        if(dl>sizeof(glob)-sizeof(suffix))return NULL;
        memcpy(glob,dir,dl);
        memcpy(glob+dl,suffix,sizeof(suffix));
        h=FindFirstFileA(glob,&data);
        if(h!=INVALID_HANDLE_VALUE){do{
            if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&ends_jag(data.cFileName)){
                size_t dl=strlen(dir),nl=strlen(data.cFileName);
                if(dl+1u+nl+1u<=sizeof(auto_rom_path)){
                    if(++found==1){memcpy(auto_rom_path,dir,dl);auto_rom_path[dl]='/';memcpy(auto_rom_path+dl+1,data.cFileName,nl+1);}
                }
            }
        }while(FindNextFileA(h,&data));FindClose(h);}
    }
#else
    d=opendir(dir); if(!d)return NULL;
    while((e=readdir(d))!=NULL){if(ends_jag(e->d_name)){char cand[PATH_MAX];struct stat st;size_t dl=strlen(dir),nl=strlen(e->d_name);if(dl+1u+nl+1u>sizeof(cand))continue;memcpy(cand,dir,dl);cand[dl]='/';memcpy(cand+dl+1u,e->d_name,nl+1u);if(stat(cand,&st)==0&&S_ISREG(st.st_mode)){if(++found==1){size_t cl=strlen(cand);if(cl+1u<=sizeof(auto_rom_path))memcpy(auto_rom_path,cand,cl+1u);}}}}
    closedir(d);
#endif
    if(found==1)return auto_rom_path;
    if(found>1)fprintf(stderr,"Multiple .jag files found beside executable; use --rom PATH.\n");
    return NULL;
}

static void logmsg(const char *fmt,...){va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fflush(stderr);if(pc_log){va_start(ap,fmt);vfprintf(pc_log,fmt,ap);va_end(ap);fflush(pc_log);}}
static int choose_safe_spawn(const u8 *maze,u16 w,u16 h,u32 *xo,u32 *yo)
{
    /* Do not infer walkability from panel bytes alone.  A PC test spawn must
       satisfy the same translated COLLIDE.S SafePos test used by MovePlayer,
       and it must have room for a small step in at least two directions. */
    unsigned y,x; const s32 step=0x1800;
    (void)maze;
    if(!w||!h)return 0;
    for(y=1;y+1<h;y++)for(x=1;x+1<w;x++){
        s32 px=(s32)(((u32)x<<16)|0x8000u),py=(s32)(((u32)y<<16)|0x8000u);
        unsigned exits=0;
        if(SafePos(80,px,py,40)!=AVP_COLL_SAFE)continue;
        exits+=(SafePos(80,px+step,py,40)==AVP_COLL_SAFE);
        exits+=(SafePos(80,px-step,py,40)==AVP_COLL_SAFE);
        exits+=(SafePos(80,px,py+step,40)==AVP_COLL_SAFE);
        exits+=(SafePos(80,px,py-step,40)==AVP_COLL_SAFE);
        if(exits>=2u){*xo=(u32)px;*yo=(u32)py;return 1;}
    }
    return 0;
}

/* Isolated draft playtest: launched as a separate native game process.
 * Never binds retail EEPROM backing, never overwrites the saved editor draft.
 * The original Jaguar code and player/AMP logic run normally in this child. */
static AvpMapMaker *g_map_test;
static double g_test_x,g_test_y,g_test_heading;
static u16 g_test_eeprom[AVP_EE_SIZE/2u];
static u16 map_test_ee_read(u16 index){return g_test_eeprom[index&63u];}
static void map_test_ee_write(u16 index,u16 value){g_test_eeprom[index&63u]=value;}

static AvpMazeCandidate *find_level_maze(AvpMazeCandidate *m,unsigned count,unsigned level)
{ unsigned i,target=86u+level;for(i=0;i<count;i++)if(m[i].file_index==target)return &m[i];return NULL; }

/* MAZE.S begins duct FadeDown before LeaveLevel/LoadLevel, but waits for
 * fade completion only at the post-load `load_complete` label.  Let the
 * monotonic VBlank clock run across that synchronous host loading work.
 * The last outgoing gameplay picture is captured at fade start and held
 * until the remaining original MazeList ticks have completed.  Computers
 * differ: PlayAvP waits for FadeDown before entering Computer(). */
static void pc_finish_visible_gameplay_fade(AvpPcVideo *v,u64 started_ns,int snapshot_saved)
{
    if(!snapshot_saved){
        /* Fallback if no framebuffer can be captured.  Preserve the CPU
         * fade state even without a hosted visual snapshot. */
        while(!faded){VSync();MazeList();}
        return;
    }
    unsigned ticks=0u;
    while(!faded){
        u64 now=avp_pc_now_ns();
        unsigned due=avp_pc_fade_ticks_due(now>=started_ns?now-started_ns:0u);
        if(due<=ticks){
            const u64 next=started_ns+(u64)(ticks+1u)*AVP_PC_NTSC_VBL_NS;
            if(now<next)avp_pc_sleep_ns(next-now);
            continue;
        }
        while(ticks<due && !faded){MazeList();++ticks;}
        avp_pc_video_present_gameplay(v);
        {
            u32 pad=0xffffffffu;int quit=0;
            avp_pc_video_poll(v,&pad,&quit);
            if(quit){avp_pc_video_end_gameplay_fade(v);avp_pc_video_destroy(v);exit(0);}
            avp_pc_pad_set(pad);
        }
    }
    avp_pc_video_end_gameplay_fade(v);
}

static int enter_retail_level(const AvpRomImage *rom,AvpMazeCandidate *mazes,unsigned maze_count,
                              unsigned target,int from_gameplay,AvpPcVideo *v,unsigned *logical_level)
{
    AvpMazeCandidate *lm=find_level_maze(mazes,maze_count<64u?maze_count:64u,target);
    if(!lm)return 0;

    u64 duct_fade_start=0u;
    int duct_snapshot_saved=0;
    if(from_gameplay){
        int newduct=(target>=AVP_FIRST_DUCT&&target<=AVP_LAST_DUCT);
        int oldduct=(cur_level>=AVP_FIRST_DUCT&&cur_level<=AVP_LAST_DUCT);
        if(newduct||oldduct){
            DoEffect(AVP_FX_DUCT1);
            FadeDown();
            duct_snapshot_saved=avp_pc_video_begin_gameplay_fade(v);
            duct_fade_start=avp_pc_now_ns();
            /* Do not wait here! Retail MAZE.S proceeds straight to
             * LeaveLevel/LoadLevel while TOM is already fading the last
             * outgoing scene.  Completion is at load_complete below. */
        }
        else if(!levfx_ID&&!use_cocoon)DoEffect(AVP_FX_DDOOR);
        /* MAZE.S::PlayAvP announces ordinary level/duct connections here,
           before LeaveLevel changes the authoritative current level. */
        if(cur_level<=AVP_LAST_DUCT && target<=AVP_LAST_DUCT){
            const AvpRuntimeOps *o=avp_runtime_ops();
            if(o->set_message)o->set_message(o->user,newduct?4u:3u);
        }
        /* An edited RIGHT/BOTTOM-only door has no LEFT/TOP panel at its
         * canonical state owner, so retail SaveDoors cannot see its byte.
         * Capture only that state in this separate Map Maker child. */
        if(g_map_test && g_map_test->level==(unsigned)cur_level)
            (void)avp_mm_playtest_capture_noncanonical_door_states(
                g_map_test,(unsigned)cur_level,avp_collision_maze_data(),
                (size_t)maze_width*maze_height*8u);
        LeaveLevel(); /* SaveDoors/save_level and authoritative cur_level=new_level. */
    }else{
        cur_level=(s16)target;
    }

    *logical_level=target;
    if(g_map_test && g_map_test->level==*logical_level){
        if(!avp_mm_playtest_overlay_maze(g_map_test,*logical_level,
                                         lm->decoded,lm->decoded_size)){
            logmsg("Map playtest rejected: draft/retail dimensions differ.\n");return 0;
        }
        logmsg("Map playtest: private draft bound for level %u.\n",*logical_level);
    }
    maze_width=lm->width;maze_height=lm->height;
    avp_collision_bind_maze(lm->decoded+8,maze_width,maze_height);
    avp_doors_bind_maze((u8*)avp_collision_maze_data(),maze_width,maze_height);
    if(rom){AvpDoorPanelIds ids;(void)avp_resources_bind_retail_panel_info(rom,target,&ids);}

    /* LEVELS.S::LoadLevel after TransMaze/GetPanels. */
    ResetScale();
    ResetSprites();
    SetStart();
    restore_level();
    ccn_xsave=ccn_ysave=0;
    KillBastards();
    ResetPlayer();
    /* Retail ResetDoors restored canonical state. Restore the independent
     * PC-only states for single-faced rotated doors after that step. */
    if(g_map_test && g_map_test->level==*logical_level)
        (void)avp_mm_playtest_restore_noncanonical_door_states(
            g_map_test,*logical_level,(u8*)avp_collision_maze_data(),
            (size_t)maze_width*maze_height*8u);
    new_level=0;

    /* MAZE.S::load_complete/ResetMaze. */
    if(levfx_ID){StopLoop(levfx_ID);levfx_ID=0;}
    /* MAZE.S::got_first preserves the level destruction alarm until the
       base's countdown finishes, even when passing through a duct/lift. */
    if(!destruct_flag)KillAlar();
    use_cocoon=0;
    ResetMaze();
    /* Original PlayAvP::load_complete waits for whatever part of the
     * outgoing fade is STILL outstanding after LoadLevel and ResetMaze.
     * R181 incorrectly imposed all 64 steps before LeaveLevel, making the
     * level-load time an additional, visible pause rather than overlap. */
    if(in_fade){
        in_fade=0;
        if(duct_fade_start)
            pc_finish_visible_gameplay_fade(v,duct_fade_start,duct_snapshot_saved);
        else while(!faded){VSync();MazeList();}
        fade_level=0x80;fade_lim=0;fade_rate=-2;
    }

    if(v)avp_pc_video_reset_presentation(v);
    return 1;
}
/* The PC frame loop calls the original Computer() directly instead of
   PlayAvP::computer_entry.  Preserve the retail audio and fade hand-off so
   terminal use does not abruptly bypass the game's normal transition. */
static void enter_retail_computer(AvpPcVideo *v)
{
    const AvpRuntimeOps *o=avp_runtime_ops();
    /* MAZE.S::no_end unloop_all releases looping effects so their current
       samples can finish; killing every voice also cuts the ambient/alarm. */
    UnLoopAl();
    if(o->play_sfx)o->play_sfx(o->user,AVP_FX_COMPE);
    FadeDown();
    /* Original PlayAvP blocks here, BEFORE Computer(). Do not move this
     * wait to the duct post-load handoff; the two paths differ. */
    {
        int captured=avp_pc_video_begin_gameplay_fade(v);
        u64 start=avp_pc_now_ns();
        pc_finish_visible_gameplay_fade(v,start,captured);
    }
    Computer();
    if(o->play_sfx)o->play_sfx(o->user,AVP_FX_COMPD);
    if(!launch_flag){fade_level=0x80;fade_lim=0;fade_rate=-2;}
}

/* F12 Debug: Refresh Level is not a new game or a level transition.
 * The current actor, health, equipment, timers, other levels and campaign
 * progress survive. Reload only the current retail maze's authored bytes,
 * then run original first-visit AMP placement and random selection again. */
static int pc_debug_refresh_current_level(const AvpRomImage *rom,
    AvpMazeCandidate *mazes,unsigned maze_count,AvpPcVideo *v,
    unsigned logical_level)
{
    AvpMazeCandidate *lm=find_level_maze(mazes,maze_count<64u?maze_count:64u,
                                         logical_level);
    if(!lm || !rom || !rom->exact_retail_sha256 ||
       !avp_pc_restore_single_maze(rom,lm))return 0;
    avp_doors_bind_maze(lm->decoded+8u,lm->width,lm->height);
    avp_collision_bind_maze(lm->decoded+8u,lm->width,lm->height);
    /* Capture pristine door bytes in the existing level-specific save slot
     * BEFORE ResetDoors. This replaces saved opened positions and avoids
     * leaking a door-state arena span on repeated debug refreshes. */
    SaveDoors();
    ResetDoors();
    ResetAccess();
    avp_amp_dev_freshen_current_level();
    restore_level();
    avp_pc_video_reset_presentation(v);
    avp_pc_video_snapshot_sim(v,avp_collision_maze_data(),maze_width,maze_height);
    return 1;
}

static int read_debug_immediate_setting(void)
{
    char exe[PATH_MAX], path[PATH_MAX]; ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1); FILE *f; char line[256];
    int found=0,on=0;
    if(n<=0 || (size_t)n>=sizeof(exe)) snprintf(path,sizeof(path),"settings.cfg");
    else { char *slash; size_t dl; const char suffix[]="/settings.cfg"; exe[n]=0; slash=strrchr(exe,'/'); if(slash){*slash=0; dl=strlen(exe); if(dl+sizeof(suffix)<=sizeof(path)){memcpy(path,exe,dl);memcpy(path+dl,suffix,sizeof(suffix));}else snprintf(path,sizeof(path),"settings.cfg");} else snprintf(path,sizeof(path),"settings.cfg"); }
    f=fopen(path,"r"); if(!f) return 0;
    while(fgets(line,sizeof(line),f)){
        char *k=line,*eq,*v,*nl,*kend,*vend;
        while(*k==' '||*k=='\t')k++;
        if(*k=='#'||*k=='\n'||!*k)continue;
        eq=strchr(k,'='); if(!eq)continue;
        *eq=0; v=eq+1;
        kend=eq; while(kend>k&&(kend[-1]==' '||kend[-1]=='\t'))*--kend=0;
        while(*v==' '||*v=='\t')v++;
        nl=strpbrk(v,"\r\n"); if(nl)*nl=0;
        vend=v+strlen(v); while(vend>v&&(vend[-1]==' '||vend[-1]=='\t'))*--vend=0;
        if(!strcmp(k,"debug_mode_immediate_in_game")||!strcmp(k,"debug_mode_immediate_in-game")){
            found=1; on=(!strcmp(v,"1")||!strcasecmp(v,"true")||!strcasecmp(v,"yes")||!strcasecmp(v,"on")); break;
        }
    }
    fclose(f);
    if(!found)logmsg("settings.cfg has no debug_mode_immediate_in_game key (legacy config); using retail default 0.\n");
    return on;
}
/* AMP.S reads TOM HC ($F00004) with MOVE.W for the firing gate, but
 * its later weapon-choice read is MOVE.L ($F00004).  On the big-endian
 * Jaguar that longword contains HC in the upper half and VC ($F00006)
 * in the lower half; AND.L #1 therefore selects vertical-counter bit 0.
 * VC advances once per video HALF-line, not every horizontal pixel tick.
 * Use an NTSC half-line phase (~31.78 us) when running without Jaguar TOM.
 * The initial HC firing gate is separately emulated; this is not a cycle-
 * accurate sync to the physical display, but it uses the correct register. */
static u32 pc_weapon_vertical_phase(void)
{
    return (u32)(avp_pc_now_ns()/31778ull);
}
/* Start a real campaign after MJP selection (or a developer entry) using
 * LEVELS.S and the same first-level lifecycle as LoadLevel. Keep savegame
 * live until InitPlayer, Init1stOvers and SetStart have consumed its fields. */
static int pc_begin_campaign(const AvpRomImage *rom,AvpMazeCandidate *mazes,
                             unsigned maze_count,AvpPcVideo *v,
                             unsigned *logical_level,int from_title,
                             unsigned deathmatch_level)
{
    AvpMazeCandidate *lm;
    /* Retail cheat sequences are session-only. A new campaign starts with
     * all three Original modes disabled, independently of new PC cheats. */
    avp_pc_video_original_reset(v);
    pc_original_god_last=0;
    if(from_title){
        /* MAIN.S reserves seven voices for the ending/FAME presentation.
         * Unlike the retail back_in lifecycle, the PC campaign restart
         * enters PlayAvP without re-running InitSynt(), so that FRONTEND
         * reservation would leave only voice 7 usable for all gameplay SFX.
         * Restore InitSynt's normal two reserved voices as we leave the title.
         * Keep current music, descriptor tables, handle lifetimes, and
         * saved-game alarm ownership untouched (StartAla will reserve 3). */
        ProtectV(2u);
        /* A new campaign starts with the ROM's original maze doors, not
         * the writable decoded map left by the previous dead player.
         * InitDoors below resets the saved-door table separately. */
        if(rom && rom->data && !avp_pc_restore_new_campaign_mazes(
                rom,mazes,maze_count<64u?maze_count:64u)){
            logmsg("New campaign cannot restore original maze bytes.\n");
            return 0;
        }
        /* MAZE.S::InitGame resets the Jaguar double-buffer selector before
         * selecting/initializing the first campaign level.  The host must
         * not inherit a prior campaign's current buffer. */
        InitDblBufs();
        if(savegame){
            /* MAZE.S::InitGame restores species BEFORE LEVELS.S::InitLevels. */
            const u8 *sv=(const u8 *)(uintptr_t)savegame;
            player_type=(s16)((sv[19]&3u)<<2);
        }
        if(deathmatch_level>=1u && deathmatch_level<=15u){
            /* F2 Deathmatch is a NEW match, never an EEPROM continuation. */
            avp_levels_set_save_words(NULL,0u);
            savegame=0;
        }
        InitLevels();
        if(deathmatch_level>=1u && deathmatch_level<=15u){
            cur_level=(s16)deathmatch_level;
            new_x=-1;
            FirstPos();
        }
        *logical_level=(unsigned)cur_level;
    }else{
        InitDblBufs();
        player_type=PT_HUMAN;
        cur_level=(s16)*logical_level;
        new_x=-1;
        savegame=0;
    }
    /* MAZE.S::InitGame performs InitSprites after InitLevels and before
     * player/AMP initialization. A previous campaign's expanded sprite
     * pointers must not survive the new-game handoff. */
    InitSprites();
    lm=find_level_maze(mazes,maze_count<64u?maze_count:64u,*logical_level);
    if(!lm){logmsg("Campaign start map unavailable: level %u.\n",*logical_level);return 0;}
    /* The first playtest map load must use the draft, not the untouched
     * Sub-Level 1/retail maze. Later level transitions already have their
     * own private override in the change-level routine. Apply BEFORE
     * InitPlayer, InitDoors and InitAMPs can observe the map. */
    if(g_map_test && g_map_test->level==*logical_level){
        if(!avp_mm_playtest_overlay_maze(g_map_test,*logical_level,
                                         lm->decoded,lm->decoded_size)){
            logmsg("Playtest bootstrap: draft dimensions mismatch.\n");return 0;
        }
        logmsg("Playtest bootstrap: private draft installed for level %u.\n",*logical_level);
    }
    maze_width=lm->width;maze_height=lm->height;
    avp_collision_bind_maze(lm->decoded+8,maze_width,maze_height);
    avp_doors_bind_maze((u8*)avp_collision_maze_data(),maze_width,maze_height);
    if(rom){AvpDoorPanelIds ids;
        if(!avp_resources_bind_retail_panel_info(rom,*logical_level,&ids))
            logmsg("WARNING: level %u panel interaction IDs unavailable.\n",*logical_level);
    }
    cur_level=(s16)*logical_level;new_level=0;
    InitPlayer(); /* InitAccess/InitDoors/InitHUD and saved score/energy/access. */
    InitAMPs();  /* A new campaign must NOT inherit levels_visit from the last. */
    InitComp();
    ResetScale();
    /* LEVELS.S::LoadLevel resets the per-level expanded sprite records
     * after scaling and resource transfer, including on the first level.
     * InitSprites above is a distinct whole-campaign initialization. */
    ResetSprites();
    if(from_title){
        /* SetStart applies the EEPROM x/y/angle; FirstPos would discard it. */
        SetStart();
    }else{
        x_pos=((u32)(maze_width/2u)<<16)|0x8000u;
        y_pos=((u32)(maze_height/2u)<<16)|0x8000u;
        (void)choose_safe_spawn(avp_collision_maze_data(),maze_width,maze_height,&x_pos,&y_pos);
    }
    restore_level();
    ccn_xsave=ccn_ysave=0;
    KillBastards();
    ResetPlayer(); /* Rebind the current level's access and door state. */
    /* MAZE.S::PostFirst calls the complete InitScrOverlays sequence AFTER
     * LoadLevel/ResetPlayer: first-person overlays, StdHUD/InitHUD, then
     * InitHUDPal.  R189 skipped StdHUD, retaining previous HUD message,
     * tracker, map, score-brush and cocoon presentation state through a
     * new campaign.  Execute the existing Jaguar-derived composite routine
     * instead of approximating it with two of its three constituent calls.
     * Keep savegame live until Init1stOvers has consumed weapon/ammo fields. */
    InitScrOverlays();
    /* New-game PC-only Marine loadout. Never overwrite the retail save data.
     * S67 MP copies the initialized original actor, so P1 and P2 inherit
     * independently the same loadout through their own actor snapshots. */
    if(v)avp_pc_marine_start_shotgun_apply(
        avp_pc_video_marine_start_shotgun(v),savegame!=0,player_type,
        &cur_weps,&new_wepno,ammo_info);
    ScreenOff();
    /* MAIN.S -> MAZE.S::PostFirst installs a fresh maze display list and
     * marks its initial fade. The hosted campaign start previously bypassed
     * this part of PostFirst, preserving the prior death/explosion CLUT.
     * We must not call the whole PostFirst here: InitScrOverlays would
     * reinitialize weapons AFTER the EEPROM/load data was consumed. */
    SetMazeList();
    FadeUp();
    /* MAZE.S::PostFirst clears the save pointer and ending countdown only
     * AFTER installing the maze OP list and scheduling FadeUp.  Do not clear
     * them ahead of the original display-list handoff. */
    savegame=0;
    end_count=0;
    /* PlayAvP::load_complete consumes PostFirst's in_fade before starting
     * the 0x80 -> 0 display ramp. This hosted entry performs the same handoff
     * without rerunning the rest of the original gameplay initialization. */
    if(in_fade){in_fade=0;fade_level=0x80;fade_lim=0;fade_rate=-2;}
    /* MAZE.S::PlayAvP::load_complete resets cocoon transfer intent before
     * ResetMaze, separate from the earlier PostFirst countdown clear. */
    use_cocoon=0;
    /* MAZE.S::ResetMaze also clears the facehugger recovery/throw state
     * and the Alien bite view displacement. The hosted campaign entry
     * previously performed only the map/GPU/ambient subset. Preserve the
     * existing single Ambient() CPU voice allocation. */
    hug_init();
    ResetMap();
    ResetMGPU();
    alien_bite=0;
    Ambient();
    if(v)avp_pc_video_reset_presentation(v);
    logmsg("Campaign entry: species %d level %u, position %08x/%08x.\n",
           player_type,*logical_level,x_pos,y_pos);
    return 1;
}

#ifdef AVP_LOCAL_MULTIPLAYER
#define AVP_MP_OPTION_HELP " [--mp-walktest-{2,3,4}|--mp-combattest-{2,3,4}]"
#else
#define AVP_MP_OPTION_HELP ""
#endif
static void usage(const char*exe){fprintf(stderr,"Usage: %s [--rom AVP.jag] [--maze-candidate N] [--diagnostic] [--log FILE]%s\n",exe,AVP_MP_OPTION_HELP);}
int main(int argc,char**argv)
{
    const char*rom_path=NULL,*log_path=NULL;int diagnostic=0,quit=0,rom_ok=0,debug_immediate=0;const char *map_test_path=NULL,*map_objects_path=NULL;unsigned map_test_level=0;int map_test_no_enemies=0;unsigned maze_pick=0,logical_level=1;AvpRomImage rom={0};AvpMazeCandidate mazes[64];unsigned maze_count=0;AvpPcVideo*v;AvpPcQuickState *quickstate=NULL;AvpGpuAutomapSnapshot quick_map={0};u64 next_sim,next_display,next_render,ticks=0;unsigned simulation_hz=10u;u32 prev_x=0,prev_y=0,prev_angle=0;double last_enhanced_delta=0.0;int last_enhanced_tick=0;AvpPcLiveHeading live_heading={0};AvpPcQuickTurn quick_turn={0};u32 live_last_game_angle=0;int i;
#ifdef AVP_LOCAL_MULTIPLAYER
    int mp_walktest=0,mp_combat=0;unsigned mp_players=2u;
    AvpLocalMpSetup mp_menu_setup;
    int mp_from_f2=0;
#endif
    for(i=1;i<argc;i++){if(!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h")){usage(argv[0]);return 0;}if(!strcmp(argv[i],"--rom")&&i+1<argc)rom_path=argv[++i];else if(!strcmp(argv[i],"--diagnostic"))diagnostic=1;
#ifdef AVP_LOCAL_MULTIPLAYER
    else if(!strcmp(argv[i],"--mp-walktest-2")||!strcmp(argv[i],"--mp-walktest-3")||!strcmp(argv[i],"--mp-walktest-4")){
        mp_walktest=1;mp_players=(unsigned)(argv[i][strlen(argv[i])-1]-'0');
    }
    else if(!strcmp(argv[i],"--mp-combattest-2")||!strcmp(argv[i],"--mp-combattest-3")||!strcmp(argv[i],"--mp-combattest-4")){
        mp_walktest=1;mp_combat=1;mp_players=(unsigned)(argv[i][strlen(argv[i])-1]-'0');
    }
#endif
else if(!strcmp(argv[i],"--map-test")&&i+1<argc)map_test_path=argv[++i];
else if(!strcmp(argv[i],"--map-test-objects")&&i+1<argc)map_objects_path=argv[++i];
else if(!strcmp(argv[i],"--map-test-no-enemies"))map_test_no_enemies=1;
else if(!strcmp(argv[i],"--map-test-level")&&i+1<argc)map_test_level=(unsigned)strtoul(argv[++i],NULL,10);
else if(!strcmp(argv[i],"--map-test-x")&&i+1<argc)g_test_x=strtod(argv[++i],NULL);
else if(!strcmp(argv[i],"--map-test-y")&&i+1<argc)g_test_y=strtod(argv[++i],NULL);
else if(!strcmp(argv[i],"--map-test-heading")&&i+1<argc)g_test_heading=strtod(argv[++i],NULL);
else if(!strcmp(argv[i],"--maze-candidate")&&i+1<argc)maze_pick=(unsigned)strtoul(argv[++i],NULL,0);else if(!strcmp(argv[i],"--log")&&i+1<argc)log_path=argv[++i];else{usage(argv[0]);return 2;}}
    if(!log_path)log_path=auto_log_path();
    if(log_path){pc_log=fopen(log_path,"a");}
    signal(SIGSEGV,crash_handler);signal(SIGABRT,crash_handler);
#ifndef _WIN32
    signal(SIGBUS,crash_handler);signal(SIGPIPE,crash_handler);signal(SIGHUP,crash_handler);
#endif
    signal(SIGFPE,crash_handler);signal(SIGILL,crash_handler);signal(SIGTERM,crash_handler);
    logmsg("avp_pc M4.4 fidelity closure startup\n");
    debug_immediate=read_debug_immediate_setting();
    logmsg("Debug immediate-in-game setting: %d.\n",debug_immediate);
    if(diagnostic||map_test_path)debug_immediate=1;
    if(map_test_path && (map_test_level<1u||map_test_level>AVP_MM_LEVELS||
       !isfinite(g_test_x)||!isfinite(g_test_y)||!isfinite(g_test_heading))){
        logmsg("Invalid map playtest launch coordinates / level.\n");return 2;
    }
    if(!rom_path&&!diagnostic){rom_path=auto_find_rom(argv[0]);if(rom_path)logmsg("Auto-selected ROM beside executable: %s\n",rom_path);}
    if(rom_path){char err[256];if(!avp_pc_rom_load(rom_path,&rom,err,sizeof(err))){logmsg("ROM error: %s\n",err);if(pc_log)fclose(pc_log);return 1;}logmsg("ROM SHA-256: %s\n",rom.sha256_hex);if(!rom.exact_retail_sha256){logmsg("This is not the canonical 4 MiB retail-world ROM expected by the current port.\n");avp_pc_rom_free(&rom);if(pc_log)fclose(pc_log);return 1;}rom_ok=1;{char cache_dir[PATH_MAX];if(avp_pc_prepare_rom_cache(&rom,argv[0],cache_dir,sizeof(cache_dir)))logmsg("ROM-derived cache ready: %s\n",cache_dir);else logmsg("WARNING: could not prepare ROM-derived asset cache; continuing with direct ROM reads.\n");}}
    /* AVP.jag is the only ROM input.  JERRY's internal waveform tables are
     * modeled in our existing synthesizer; no external hardware ROM lookup
     * or newly generated/distributed proprietary image is performed. */
    avp_amp_bind_retail_source_templates_and_random();
    if(rom_ok){if(avp_resources_bind_retail_level_info(&rom))logmsg("Retail LEVELS start-position table recovered from ROM-backed RDB.DTA.\n");else logmsg("WARNING: retail LEVELS start-position table recovery failed.\n");}
    if(rom_ok){unsigned al=0,ae=0;if(avp_resources_bind_retail_amp_placements(&rom,&al,&ae))logmsg("Retail AMP authored placement tables recovered from ROM: %u level lists / %u placement records.\n",al,ae);else logmsg("WARNING: retail AMP placement table recovery failed; campaign population remains unbound.\n");}
    if(!rom_path&&!diagnostic){logmsg("No retail ROM supplied; starting diagnostic geometry. Use --rom /path/to/AVP.jag for verified retail data.\n");diagnostic=1;}
    crash_stage=1;make_diag_maze();
    if(rom_ok){size_t files_off=0;unsigned files_records=0;if(avp_resources_find_files_archive(&rom,&files_off,&files_records))logmsg("Discovered FILES.DAT record stream at ROM offset 0x%zx (%u records).\n",files_off,files_records);else logmsg("Could not discover a valid FILES.DAT record stream.\n");maze_count=avp_resources_find_mazes(&rom,mazes,64);logmsg("Detected %u maze-shaped retail file records.\n",maze_count);if(maze_count&&maze_pick<maze_count){u8 *m=mazes[maze_pick].decoded+8;maze_width=mazes[maze_pick].width;maze_height=mazes[maze_pick].height;avp_collision_bind_maze(m,maze_width,maze_height);logical_level=(mazes[maze_pick].file_index>=87u&&mazes[maze_pick].file_index<=101u)?mazes[maze_pick].file_index-86u:1u;logmsg("Using maze candidate %u: file index %u, logical level %u, %ux%u.\n",maze_pick,mazes[maze_pick].file_index,logical_level,maze_width,maze_height);}else{logmsg("Requested maze candidate unavailable; using diagnostic geometry.\n");avp_collision_bind_maze(diag_maze,DIAG_W,DIAG_H);maze_width=DIAG_W;maze_height=DIAG_H;}}else{avp_collision_bind_maze(diag_maze,DIAG_W,DIAG_H);maze_width=DIAG_W;maze_height=DIAG_H;}
    if(map_test_path){
        AvpMazeCandidate *retail=find_level_maze(mazes,maze_count,map_test_level);
        if(!rom_ok||!retail || !(g_map_test=malloc(sizeof(*g_map_test)))){
            logmsg("Playtest requires a verified retail ROM/map.\n");return 2;
        }
        avp_mm_init(g_map_test);
        if(!avp_mm_bind(g_map_test,map_test_level,retail->decoded,
                        retail->decoded_size)||
           !avp_mm_select(g_map_test,map_test_level)||
           !avp_mm_load(g_map_test,map_test_path)||
           !avp_mm_test_start_valid(g_map_test,g_test_x,g_test_y)){
            logmsg("Playtest rejected: invalid draft or spawn location.\n");return 2;
        }
#ifndef _WIN32
        /* The temporary file is no longer needed; no persistent map is changed. */
        (void)unlink(map_test_path);
#endif
        if(g_map_test->w!=retail->width||g_map_test->h!=retail->height){
            logmsg("Playtest rejected: map dimensions do not match retail.\n");return 2;
        }
        if(map_objects_path){
            if(!avp_mm_objects_load(g_map_test,map_objects_path)){
                logmsg("Playtest rejected: invalid object sidecar.\n");return 2;
            }
#ifndef _WIN32
            (void)unlink(map_objects_path);
#endif
        }
        g_map_test->playtest_no_enemies=map_test_no_enemies;
        if(!avp_mm_playtest_bind_amp(g_map_test)){
            logmsg("Playtest rejected: invalid or excessive AMP object placements.\n");return 2;
        }
        logical_level=map_test_level;
        logmsg("Playtest loaded draft for level %u at %.4f / %.4f.\n",
               logical_level,g_test_x,g_test_y);
    }
    /* Create the host presentation before binding runtime callbacks: retail
       MJP VBLs now poll the same keyboard/gamepad path as gameplay. */
    crash_stage=2;logmsg("Creating native PC video and binding runtime...\n");
    v=avp_pc_video_create(640,456,"Alien vs Predator - Atari Jaguar PC port");
    if(!v){logmsg("Could not open native video window.\n");avp_pc_rom_free(&rom);if(pc_log)fclose(pc_log);return 1;}
    if(rom_ok)avp_pc_video_map_maker_rom_path(v,rom_path);
    if(rom_ok){
        for(unsigned mm_i=0;mm_i<maze_count && mm_i<64u;mm_i++){
            unsigned file=mazes[mm_i].file_index;
            if(file>=87u && file<=101u)
                avp_pc_video_map_maker_bind(v,file-86u,mazes[mm_i].decoded,
                    mazes[mm_i].decoded_size);
        }
    }
    quickstate=avp_pc_quickstate_create();
    if(quickstate)logmsg("Session quicksave state range: %zu bytes (F5 save / F9 load).\n",avp_pc_quickstate_core_bytes());
    else logmsg("WARNING: session quicksave state could not be initialized.\n");
    avp_pc_runtime_set_rom(rom_ok?&rom:NULL);avp_pc_bind_runtime(v);
    {char ee_path[PATH_MAX];
        if(g_map_test){
            for(unsigned n=0;n<AVP_EE_SIZE/2u;n++)g_test_eeprom[n]=0xffffu;
            avp_eeprom_bind(map_test_ee_read,map_test_ee_write,NULL);
            Init_EE();
            logmsg("Playtest uses isolated volatile EEPROM (retail saves protected).\n");
        }else if(avp_pc_eeprom_open_for_executable(argv[0],ee_path,sizeof(ee_path))){
            avp_eeprom_bind(avp_pc_eeprom_read_word,avp_pc_eeprom_write_word,NULL);
            Init_EE();
            logmsg("Jaguar EEPROM save backing ready: %s\n",ee_path);
        }else logmsg("WARNING: EEPROM backing could not be opened; original save slots will be volatile.\n");
    }
    avp_computer_bind_retail_terminals();
    logmsg("InitJaguar...\n");InitJaguar();logmsg("InitVideo...\n");InitVideo();
    if(rom_ok){
        int nt=avp_pc_video_load_retail_textures(v,&rom);
        const u8 *fa=NULL,*fb=NULL,*fc=NULL;size_t fan=0,fbn=0,fcn=0,fn=0;
        logmsg("Decoded %d retail JagPEG panel textures for PC presentation.\n",nt);
        if(avp_resources_rom_descriptor(&rom,0x815c1cu,&fa,&fan)&&
           avp_resources_rom_descriptor(&rom,0x815c24u,&fb,&fbn)&&
           avp_resources_rom_descriptor(&rom,0x815c2cu,&fc,&fcn)){
            fn=fan;if(fbn<fn)fn=fbn;if(fcn<fn)fn=fcn;
            avp_font_bind_brushes(fa,fb,fc,fn);
            logmsg("Retail COMPUTER font brushes bound from OVERS.DB descriptors (%zu bytes minimum).\n",fn);
        }else logmsg("WARNING: retail COMPUTER font brushes could not be resolved.\n");
        if(avp_pc_audio_init(&rom))logmsg("FullSynth compressed-SFX host mixer active at 20774 Hz.\n");else logmsg("Audio device unavailable; continuing without host PCM output.\n");
        avp_pc_video_audio_apply_settings(v);
    }
    /* The retail back_in entry calls InitSynt before entering the title/game.
       This PC entry bypasses back_in, so it must initialize the CPU voice
       allocator itself.  Otherwise every zero-initialized voice appears busy:
       TFX/DoEffect weapon audio is rejected while direct creature/player
       playback still works.  Initialize after the host audio backend. */
    InitSynt();
    avp_amp_set_beam_weapon_sample(pc_weapon_vertical_phase);
avp_hud_set_countdown_callbacks(pc_countdown_cue,pc_alarm);

    if(!debug_immediate && rom_ok){
        /* On the first executable boot, leave the window dark before the
         * Jaguar title appears. This host-only pause is NOT a claimed ROM
         * opcode/timer match; it does not apply on post-game title returns. */
        avp_pc_video_present_boot_black(v);
        for(unsigned boot_vbl=0u;boot_vbl<60u;boot_vbl++){
            u32 pad=0xffffffffu;int boot_quit=0;
            /* Keep the OS event queue alive during the black first-boot
             * presentation. A plain sleep left the mapped X11 window
             * apparently frozen during resize and close events. */
            avp_pc_video_set_frontend_input(v,1);
            avp_pc_video_poll(v,&pad,&boot_quit);
            avp_pc_video_set_frontend_input(v,0);
            avp_pc_pad_set(pad);
            if(boot_quit){
                avp_pc_video_destroy(v);avp_pc_rom_free(&rom);
                if(pc_log)fclose(pc_log);
                return 0;
            }
            avp_pc_sleep_ns(16666667ull);
        }
        logmsg("Retail frontend: translated NewTitle/MJP campaign selection.\n");
        NewTitle();
#ifdef AVP_LOCAL_MULTIPLAYER
        if(avp_pc_video_take_local_mp_launch(v,&mp_menu_setup)){
            mp_from_f2=1;mp_walktest=1;mp_combat=1;
            mp_players=mp_menu_setup.players;
            /* Campaign world and player character are independent: original
             * campaign bootstrap chooses the shared enemy/map objective;
             * the F2 player species are used for each live actor. */
            player_type=(s16)(mp_menu_setup.campaign<<2);
            savegame=0;
            logmsg("F2 Start Game: %u players, campaign %s, native combat runtime (independent per-player species).\n",
                   mp_players,avp_local_mp_campaign_name(mp_menu_setup.campaign));
        }
#endif
    }
    if(rom_ok){
        avp_doors_bind_retail_meta();
        /* The child Map Maker process owns a PRIVATE offset-only copy of
         * LEVDOOR.S metadata. Retail campaign globals and source tables are
         * never rewritten. Every other level keeps identical retail records. */
        if(g_map_test && !avp_mm_playtest_bind_scripts(g_map_test)){
            logmsg("Map playtest rejected: invalid relocated door/exit metadata.\n");
            avp_pc_video_destroy(v);avp_pc_rom_free(&rom);
            if(pc_log)fclose(pc_log);
            return 1;
        }
        if(g_map_test && g_map_test->script_modified)
            logmsg("Map playtest: isolated relocated LEVDOOR metadata bound for level %u.\n",g_map_test->level);
        if(!pc_begin_campaign(&rom,mazes,maze_count,v,&logical_level,
                g_map_test?1:!debug_immediate,
#ifdef AVP_LOCAL_MULTIPLAYER
                g_map_test?map_test_level:(mp_from_f2 && mp_menu_setup.game_mode==AVP_MP_DEATHMATCH?mp_menu_setup.dm_level:0u)
#else
                g_map_test?map_test_level:0u
#endif
                )){
            avp_pc_video_destroy(v);avp_pc_rom_free(&rom);if(pc_log)fclose(pc_log);return 1;
        }
    }else{
        /* Preserve the existing no-ROM developer geometry boot. */
        player_type=PT_HUMAN;cur_level=(s16)logical_level;new_level=0;savegame=0;
        InitPlayer();InitAMPs();InitComp();Init1stOvers();
        avp_pc_marine_start_shotgun_apply(
            avp_pc_video_marine_start_shotgun(v),0,player_type,
            &cur_weps,&new_wepno,ammo_info);
        InitHUDPal();
        restore_level();ResetPlayer();ScreenOff();Ambient();
        x_pos=((u32)(maze_width/2u)<<16)|0x8000u;
        y_pos=((u32)(maze_height/2u)<<16)|0x8000u;
        (void)choose_safe_spawn(avp_collision_maze_data(),maze_width,maze_height,&x_pos,&y_pos);
    }
    if(g_map_test){
        s32 nx=(s32)(g_test_x*65536.0),ny=(s32)(g_test_y*65536.0);
        if(SafePos(80,nx,ny,40)!=AVP_COLL_SAFE){
            logmsg("Playtest aborted: start is blocked by Jaguar wall collision.\n");
            return 2;
        }
        x_pos=(u32)nx;y_pos=(u32)ny;
        centre_angle=(u32)(int64_t)(-g_test_heading*(4294967296.0/6.2831853071795864769));
        logmsg("Playtest spawn committed: %08x / %08x angle %08x.\n",x_pos,y_pos,centre_angle);
    }
    prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;avp_pc_video_snapshot_sim(v,avp_collision_maze_data(),maze_width,maze_height);
    avp_pc_video_automap_observe(v,avp_collision_maze_data(),maze_width,maze_height,x_pos,y_pos,centre_angle,logical_level);
    avp_pc_video_set_render_alpha(v,1.0);
    crash_stage=4;logmsg("Presenting first frame before simulation...\n");avp_pc_video_render_maze(v,avp_collision_maze_data(),maze_width,maze_height,x_pos,y_pos,centre_angle,rom_ok,logical_level,ticks,simulation_hz);
    avp_pc_runtime_set_gameplay_vblank(1);
    next_sim=avp_pc_now_ns()+(1000000000ull/simulation_hz);
    next_display=avp_pc_now_ns()+(1000000000ull/60u);
    next_render=avp_pc_now_ns();
    logmsg("Entering main loop. maze=%ux%u x=%08x y=%08x\n",maze_width,maze_height,x_pos,y_pos);
#ifdef AVP_LOCAL_MULTIPLAYER
multiplayer_dispatch:
    if(mp_walktest){
        /* Existing no-ROM diagnostic scene does not bind DOORS.S; the
         * opt-in smoke needs a valid original door state. Retail is already
         * bound by pc_begin_campaign and is left unchanged here. */
        if(mp_from_f2)logmsg("F2 game launch confirmed: entering %u-player native session.\n",mp_players);
        if(!rom_ok){
            avp_doors_bind_maze((u8*)avp_collision_maze_data(),maze_width,maze_height);
            InitDoors();
        }
        const AvpMazeCandidate *mp_retail=rom_ok?
            find_level_maze(mazes,maze_count<64u?maze_count:64u,logical_level):NULL;
        int mp_error=(!rom_ok||mp_retail) ?
            avp_mp_walktest_run(v,logical_level,mp_retail,
                                 rom_ok?mazes:NULL,rom_ok?maze_count:0u,
                                 rom_ok?&rom:NULL,mp_combat,mp_players,
                                 mp_from_f2?&mp_menu_setup:NULL) : 1;
        if(mp_error==AVP_MP_RUN_COMPLETED || mp_error==AVP_MP_RUN_TITLE ||
           mp_error==AVP_MP_RUN_RESTART){
            /* The source MP backend has already waited out the original
             * FadeDown before freeing the level/actor snapshots. Only a
             * successful campaign-ending game_over reaches this branch.
             * Keep single-player's retail High Score flow unchanged. */
            if(mp_error==AVP_MP_RUN_COMPLETED)
                logmsg("Multiplayer campaign complete: species %d. High Scores disabled.\n",
                       player_type);
            else if(mp_error==AVP_MP_RUN_RESTART)
                logmsg("F2 Start Game: cleanly restart multiplayer with edited setup.\n");
            else
                logmsg("Multiplayer Restart Game: return to Jaguar title (not F2 setup).\n");
            avp_pc_runtime_set_gameplay_vblank(0);
            KillAlar();KillAll();KillAmbi();
            player_dead=0;
            game_over=(mp_error==AVP_MP_RUN_COMPLETED);
            launch_flag=0;score=0;savegame=0;new_level=0;
            if(mp_error==AVP_MP_RUN_COMPLETED)avp_post_game_frontend_multiplayer();
            score=0;
            if(mp_error!=AVP_MP_RUN_RESTART){
                /* Multiplayer F12 Return to Title has the same held-accept
                 * suppression as single-player. Otherwise the last input
                 * can immediately choose NEW GAME on the title screen. */
                if(mp_error==AVP_MP_RUN_TITLE)
                    avp_pc_video_prepare_title_return(v);
                NewTitle();
                mp_walktest=0;mp_from_f2=0;
            }
            /* A live F2 Start Game request is already pending. Its setup
             * must be consumed before fresh InitLevels/InitPlayer, without
             * entering the title screen or inheriting the old campaign. */
            if(avp_pc_video_take_local_mp_launch(v,&mp_menu_setup)){
                mp_from_f2=1;mp_walktest=1;mp_combat=1;
                mp_players=mp_menu_setup.players;
                player_type=(s16)(mp_menu_setup.campaign<<2);
                savegame=0;
            }else if(mp_error==AVP_MP_RUN_RESTART){
                logmsg("F2 restart request missing setup; cannot safely restart.\n");
                quit=1;goto pc_shutdown;
            }
            if(rom_ok){
                if(!pc_begin_campaign(&rom,mazes,maze_count,v,&logical_level,1,
                        mp_from_f2 && mp_menu_setup.game_mode==AVP_MP_DEATHMATCH?mp_menu_setup.dm_level:0u)){
                    logmsg("New campaign after multiplayer ending could not initialize.\n");
                    quit=1;goto pc_shutdown;
                }
            }else if(mp_error==AVP_MP_RUN_RESTART){
                /* Keep the no-ROM diagnostic useful for testing the live F2
                 * restart handoff. Never substitute this geometry for a
                 * real retail-ROM campaign. */
                logical_level=1u;
                cur_level=1;new_level=0;savegame=0;
                maze_width=DIAG_W;maze_height=DIAG_H;
                avp_collision_bind_maze(diag_maze,DIAG_W,DIAG_H);
                avp_doors_bind_maze((u8 *)avp_collision_maze_data(),
                                    maze_width,maze_height);
                InitPlayer();InitAMPs();InitComp();Init1stOvers();
                restore_level();ResetPlayer();ScreenOff();Ambient();
                x_pos=((u32)(maze_width/2u)<<16)|0x8000u;
                y_pos=((u32)(maze_height/2u)<<16)|0x8000u;
                (void)choose_safe_spawn(avp_collision_maze_data(),
                                      maze_width,maze_height,&x_pos,&y_pos);
                logmsg("F2 diagnostic new game restarted with fresh actor/world.\n");
            }else{
                logmsg("No-ROM title restart is unsupported.\n");
                quit=1;goto pc_shutdown;
            }
            ticks=0;
            prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;
            avp_pc_video_snapshot_sim(v,avp_collision_maze_data(),maze_width,maze_height);
            avp_pc_video_automap_observe(v,avp_collision_maze_data(),maze_width,
                                          maze_height,x_pos,y_pos,centre_angle,logical_level);
            avp_pc_video_reset_presentation(v);
            avp_pc_runtime_set_gameplay_vblank(1);
            next_sim=avp_pc_now_ns()+(1000000000ull/simulation_hz);
            next_display=avp_pc_now_ns()+(1000000000ull/60u);
            next_render=avp_pc_now_ns();
            goto multiplayer_dispatch;
        }
        if(mp_error==AVP_MP_RUN_ERROR)
            logmsg("Multiplayer native session faulted; no ending/score submission attempted.\n");
        quit=1;
        goto pc_shutdown;
    }
#endif
    {u32 last_pad=0xffffffffu;
    while(!quit){u32 pad;u64 now=avp_pc_now_ns();unsigned catchup=0;crash_stage=5;crash_tick=ticks;avp_pc_video_poll(v,&pad,&quit);
        if(avp_pc_video_take_restart_title(v))goto manual_return_to_title;
        if(avp_pc_video_take_refresh_level(v)){
            if(pc_debug_refresh_current_level(&rom,mazes,maze_count,v,logical_level)){
                logmsg("F12 Debug: refreshed original doors and fresh AMP population for level %u.\n",logical_level);
                prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;
                next_sim=avp_pc_now_ns()+(1000000000ull/simulation_hz);
            }else logmsg("F12 Debug: refresh rejected (retail level source unavailable).\n");
        }
        /* PC-only interpolated 180-degree Quick Turn. Queue independent input
         * edges, then move the view along the host clock rather than snapping
         * centre_angle at the first press. The original 10 Hz simulation
         * consumes samples of that same curve at its ordinary tick rate. */
        {unsigned turns=avp_pc_video_take_quick_turn(v);
         if(hugkill || !avp_pc_video_quick_turn_allowed(v))
             avp_pc_quick_turn_cancel(&quick_turn);
         else if(turns){
             avp_pc_quick_turn_enqueue(&quick_turn,centre_angle,
                                       avp_pc_now_ns(),turns);
             avp_pc_live_heading_reset(&live_heading);
             last_enhanced_tick=0;
             last_enhanced_delta=0.0;
         }
        }
        /* Native PC optional look: poll the physical stick on the host clock,
         * not in 10-Hz increments. The next original game tick consumes the
         * current high-precision heading once; display frames can use it now. */
        {float live_axis=0.0f;
         u64 input_now=avp_pc_now_ns();
         if(!quick_turn.active&&!hugkill&&avp_pc_video_live_stick_turn(v,&live_axis)){
             if(live_heading.active&&centre_angle!=live_last_game_angle)
                 avp_pc_live_heading_reset(&live_heading);
             (void)avp_pc_live_heading_poll(&live_heading,centre_angle,input_now,
                  live_axis,avp_pc_video_turn_sensitivity(v),
                  avp_pc_video_turn_speed(v),max_avel,simulation_hz);
         }else avp_pc_live_heading_reset(&live_heading);
         live_last_game_angle=centre_angle;
        }
        {unsigned actions=avp_pc_video_take_actions(v);
            if(actions&AVP_PC_ACTION_KILL_SELECTED){unsigned killed=avp_amp_dev_kill_enemies(avp_pc_video_kill_selected_type(v));logmsg("Cheat: killed %u enemies of selected type.\n",killed);}
            if(actions&AVP_PC_ACTION_KILL_ALL){unsigned killed=avp_amp_dev_kill_enemies(-1);logmsg("Cheat: killed %u enemies in current level.\n",killed);}
            if(actions&AVP_PC_ACTION_CLEAR_ENEMIES){unsigned removed=avp_amp_dev_clear_enemies();logmsg("Cheat: removed %u enemies from current level.\n",removed);}
            if(actions&AVP_PC_ACTION_CLEAR_HAZARDS){unsigned removed=avp_amp_dev_clear_hazards();logmsg("Cheat: removed %u hazards from current level.\n",removed);}
            if(actions&AVP_PC_ACTION_RESTORE_HEALTH)avp_dev_cheats_restore_health();
            if(actions&AVP_PC_ACTION_RESTORE_AMMO)avp_dev_cheats_restore_ammo();
            if(actions&AVP_PC_ACTION_GIVE_WEAPONS)avp_dev_cheats_give_weapons();
            if(actions&AVP_PC_ACTION_SECURITY_UP)avp_dev_cheats_adjust_security(1);
            if(actions&AVP_PC_ACTION_SECURITY_DOWN)avp_dev_cheats_adjust_security(-1);
            avp_pc_original_cheat_apply(actions);
            if(actions&AVP_PC_ACTION_QUICKSAVE){
                if(quickstate&&avp_pc_quickstate_save(quickstate,avp_collision_maze_data(),maze_width,maze_height,ticks)){
                    avp_pc_video_automap_save(v,&quick_map);
                    if(!g_map_test && rom_ok && avp_pc_quickstate_disk_save(quickstate,&rom,
                        (unsigned)cur_level,&quick_map,sizeof(quick_map)))
                        logmsg("Quicksave captured and stored on disk: level %d, tick %llu.\n",
                               (int)cur_level,(unsigned long long)ticks);
                    else logmsg("Session quicksave captured; durable save not available.\n");
                }else logmsg("Quicksave failed.\n");
            }
            if(actions&AVP_PC_ACTION_QUICKLOAD){u64 restored_ticks=0;
                if(!g_map_test&&quickstate&&!avp_pc_quickstate_valid(quickstate)&&rom_ok){
                    if(avp_pc_quickstate_disk_load(quickstate,&rom,mazes,
                        maze_count<64u?maze_count:64u,&quick_map,sizeof(quick_map)))
                        logmsg("Verified compatible disk quicksave for restore.\n");
                    else logmsg("No compatible, safe disk quicksave found.\n");
                }
                if(quickstate&&avp_pc_quickstate_load(quickstate,&restored_ticks)){
                    /* As on original hardware, a restored save does not
                     * re-enter Banana Stops. A saved zero-health actor must
                     * therefore face the original death check after load. */
                    avp_pc_video_original_reset(v);
                    pc_original_god_last=0;
                    cheat=0;
                    avp_pc_quick_turn_cancel(&quick_turn);
                    avp_player_host_quick_turn_reset();
                    ticks=restored_ticks;
                    avp_dev_cheats_tracker_restored();
                    logical_level=(unsigned)(cur_level>0?cur_level:1);
                    /* Quicksave is a full session snapshot, so restore the host's
                       level-view dimensions too.  The collision maze pointer itself
                       lives in the captured core state and still points into the
                       stable decoded retail-level allocation. */
                    maze_width=avp_pc_quickstate_width(quickstate);
                    maze_height=avp_pc_quickstate_height(quickstate);
                    avp_pc_video_automap_load(v,&quick_map);
                    avp_doors_bind_maze((u8*)avp_collision_maze_data(),maze_width,maze_height);
                    if(rom_ok){AvpDoorPanelIds ids;(void)avp_resources_bind_retail_panel_info(&rom,logical_level,&ids);}
                    /* Runtime callbacks contain host pointers; rebind them after the CPU snapshot. */
                    avp_pc_bind_runtime(v);avp_pc_runtime_set_gameplay_vblank(1);
                    avp_pc_pad_set(0xffffffffu);readpad();
                    avp_pc_audio_stop_all();if(current_music)avp_pc_audio_start_music(current_music);avp_pc_audio_start_ambient(logical_level);
                    prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;avp_pc_video_reset_presentation(v);avp_pc_video_snapshot_sim(v,avp_collision_maze_data(),maze_width,maze_height);
                    next_sim=avp_pc_now_ns()+(1000000000ull/simulation_hz);
                    next_display=avp_pc_now_ns()+(1000000000ull/60u);
                    logmsg("Quickload restored game CPU/maze state: level %d, tick %llu.\n",(int)cur_level,(unsigned long long)ticks);
                }else logmsg("No valid session quicksave to load.\n");
            }
        }
        /* On Jaguar MazeList executes in the display interrupt, independently
           of the original 10 Hz game simulation.  Advance its CPU-visible
           frame/fade state at 60 Hz here; the GPU host renderer still owns
           output and interpolation.  Never replay a backlog of missed VBLs. */
        if(now>=next_display){
            MazeList();
            next_display+=(1000000000ull/60u);
            if(now>=next_display && now-next_display>1000000000ull/60u)
                next_display=now+(1000000000ull/60u);
        }
        avp_dev_cheats_set_infinite_health(avp_pc_video_infinite_health(v));
        avp_dev_cheats_set_infinite_ammo(avp_pc_video_infinite_ammo(v));
        avp_dev_cheats_set_tracker(avp_pc_video_tracker_cheat(v));
        avp_amp_set_enemies_frozen(avp_pc_video_freeze_enemies(v));
        avp_amp_set_attacks_disabled(avp_pc_video_attacks_disabled(v));
        avp_amp_set_marine_pulse_los_hits(avp_pc_video_marine_pulse_los_hits(v));
        avp_mazescrn_set_unlimited_shooting_range(
            avp_pc_video_unlimited_shooting_range(v));
        avp_amp_set_marine_fire_interrupt(avp_pc_video_marine_fire_interrupt(v));
        avp_amp_set_normalize_diagonal_contact_range(avp_pc_video_normalize_diagonal_contact_range(v));
        avp_amp_set_enemy_activation_distance(avp_pc_video_enemy_activation_distance(v));
        avp_amp_set_facehug_attach_disabled(avp_pc_video_facehug_attach_disabled(v));
        {int hd=avp_pc_video_take_hz_delta(v);if(hd){int nh=(int)simulation_hz+hd;if(nh<1)nh=1;if(nh>60)nh=60;simulation_hz=(unsigned)nh;next_sim=now+(1000000000ull/simulation_hz);logmsg("Simulation rate -> %u Hz.\n",simulation_hz);}}
        {int sd=avp_pc_video_take_stage_delta(v);if(sd&&rom_ok){avp_pc_quick_turn_cancel(&quick_turn);avp_player_host_quick_turn_reset();
            int nl=(int)logical_level+sd;
            if(nl<1)nl=15;
            if(nl>15)nl=1;
            AvpMazeCandidate *lm=find_level_maze(mazes,maze_count<64u?maze_count:64u,(unsigned)nl);
            if(lm){
                /* The old developer-only shortcut skipped LEVELS.S's
                 * LeaveLevel/ResetPlayer/ResetMaze lifecycle and could carry
                 * stale airlock and door state into the target maze. */
                new_level=(s16)nl;new_x=-1;
                if(enter_retail_level(&rom,mazes,maze_count,(unsigned)nl,1,v,&logical_level)){
                    x_pos=((u32)(maze_width/2u)<<16)|0x8000u;
                    y_pos=((u32)(maze_height/2u)<<16)|0x8000u;
                    (void)choose_safe_spawn(avp_collision_maze_data(),maze_width,maze_height,&x_pos,&y_pos);
                    KillBastards();InitMove();Ambient();
                    prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;
                    avp_pc_video_reset_presentation(v);
                    logmsg("Developer stage switch -> level %u / FILES %u.\n",logical_level,lm->file_index);
                }
            }else logmsg("Stage %d map unavailable.\n",nl);
        }}
        {int sr=avp_pc_video_take_species_request(v);s16 want=sr==1?PT_ALIEN:(sr==2?PT_PREDATOR:PT_HUMAN);if(sr>=0&&want!=player_type){avp_pc_quick_turn_cancel(&quick_turn);avp_player_host_quick_turn_reset();save_level();player_type=want;InitPlayer();Init1stOvers();
            avp_pc_marine_start_shotgun_apply(
                avp_pc_video_marine_start_shotgun(v),0,player_type,
                &cur_weps,&new_wepno,ammo_info);
            ResetPlayer();InitAccess();InitHUDPal();restore_level();prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;avp_pc_video_reset_presentation(v);logmsg("Developer species -> %s; current level population rebuilt through retail AMP lifecycle.\n",want==PT_HUMAN?"Human":(want==PT_ALIEN?"Alien":"Predator"));}}
        avp_pc_video_original_gps_actor(v,0,(unsigned)cur_level,x_pos,y_pos);
        avp_pc_video_original_actor_status(v,0,(unsigned)player_type,
            (unsigned)cur_weps,show_mt!=0);
        if(pad!=last_pad){logmsg("input pad=%08x at tick %llu\n",(unsigned)pad,(unsigned long long)ticks);last_pad=pad;}avp_pc_runtime_set_noclip(avp_pc_video_noclip(v));avp_pc_pad_set(pad);
        /* Run the original 10 Hz simulation.  Never subtract next_sim from an
           earlier `now`: that unsigned underflow was the M1.4 event-starvation
           bug.  Bound catch-up so X11 input/window events are always serviced. */
        {int wd=avp_pc_video_take_weapon_delta(v);if(wd&&player_type!=PT_ALIEN)avp_weapons_cycle(wd,4);}
        while(now>=next_sim&&!quit&&catchup<5u){
            /* Capture short host-frame presses exactly once at the next original
               10 Hz tick. The optional enhancement never advances simulation. */
            u32 sim_pad=avp_pc_video_simulation_pad(v,pad);
            const int separate_use=player_type==PT_ALIEN&&avp_pc_video_alien_use_separate(v);
            const int use_now=avp_pc_video_take_alien_use(v);
            avp_doors_pc_alien_use(separate_use,use_now);
            /* PC analog lift chord is handed to the original DOORS.S routine;
               its face and level checks remain Jaguar-authored. Isolate the
               host Use input from B so an Alien bite never operates a lift. */
            if(avp_pc_video_analog_enabled(v)&&
               (separate_use?use_now:!(sim_pad&(1u<<FIRE_B)))){
                float forward=0.0f;
                avp_pc_video_analog_motion(v,&forward,NULL);
                if(separate_use){
                    const u32 original_b=sim_pad&(1u<<FIRE_B);
                    sim_pad=avp_pc_lift_analog_pad(sim_pad&~(1u<<FIRE_B),1,
                                      avp_doors_player_facing_lift(),forward);
                    sim_pad=(sim_pad&~(1u<<FIRE_B))|original_b;
                }else sim_pad=avp_pc_lift_analog_pad(sim_pad,1,
                                      avp_doors_player_facing_lift(),forward);
            }
            /* Alien attack toggles apply only through the PC host input
             * adapter; all retail input is unchanged with default gear. */
            sim_pad=avp_pc_original_cheat_alien_pad(sim_pad);
            avp_pc_pad_set(sim_pad);
            crash_stage=6;crash_tick=ticks;if(ticks==0)logmsg("First simulation tick...\n");prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;avp_pc_video_snapshot_sim(v,avp_collision_maze_data(),maze_width,maze_height);{
                float forward=0.0f,strafe=0.0f;
                avp_pc_video_analog_motion(v,&forward,&strafe);
                avp_player_host_analog_motion(avp_pc_video_analog_enabled(v),forward,strafe);
                avp_player_host_keyboard_strafe_during_turn(
                    avp_pc_video_keyboard_strafe_during_turn(v));
                /* Evaluate mouse movement before its dedicated sensitivity: C
                   does not specify argument evaluation order. Movement stays
                   at 1.0x regardless of either look sensitivity setting. */
                {
                    float turn=avp_pc_video_modern_turn(v);
                    float sensitivity=avp_pc_video_aim_sensitivity(v);
                    float speed=avp_pc_video_aim_speed(v);
                    avp_player_host_analog_aim(avp_pc_video_aim_enabled(v),turn,sensitivity,speed);
                    avp_player_host_aim_options(avp_pc_video_enhanced_analog_enabled(v),avp_pc_video_mouse_aim_active(v));
                    avp_player_host_live_heading(live_heading.active&&
                        !avp_pc_video_mouse_aim_active(v),
                        avp_pc_live_heading_word(&live_heading));
                    avp_player_host_quick_turn_heading(quick_turn.active,
                        quick_turn.active ? avp_pc_quick_turn_angle(&quick_turn,now) : 0u);
                    /* Record the actual signed enhanced-stick turn across this
                     * authoritative tick. Host presentation distributes that
                     * exact turn at 60 Hz without touching game simulation. */
                    last_enhanced_tick=avp_pc_video_enhanced_analog_enabled(v)&&
                        !avp_pc_video_mouse_aim_active(v)&&!hugkill&&
                        turn>=-1.0f&&turn<=1.0f;
                    last_enhanced_delta=last_enhanced_tick ?
                        -(double)turn*(double)sensitivity*(double)speed*(double)max_avel : 0.0;
                }
            }{
                /* Retail MAZE.S::NextFrame publishes the GPU-facing state
                 * BEFORE UpdatePlayer changes the 68000 player position.
                 * The port had left all these source-owned fields at their
                 * initialization values despite maintaining PC interpolation
                 * separately. Keep the Jaguar snapshot exact, including the
                 * Alien bite's view-only displacement (not a physical move). */
                const u16 ang=(u16)(centre_angle>>16);
                gpu_xpos=x_pos;gpu_ypos=y_pos;gpu_angle=centre_angle;
                gpu_amps_at=amps_at;gpu_gmps_at=gmps_at;gpu_clist_at=clist_at;
                sin_ang=sin_d0(ang);cos_ang=cos_d0(ang);
                gpu_sin_ang=sin_ang;gpu_cos_ang=cos_ang;
                if(alien_bite){
                    /* Mirror MAZE.S 68000 ADD.L/SUB.L wrap exactly. */
                    gpu_xpos += (u32)(cos_ang >> alien_bite);
                    gpu_ypos -= (u32)(sin_ang >> alien_bite);
                }
            }crash_stage=7;
            if(avp_pc_video_full_cpu_systems(v)){
                /* CPU-side equivalent of the active NextFrame update domain.
                   Rendering/pause/VBL presentation remain host-owned here so
                   diagnostic mode cannot block the X11 event pump. */
                if(avp_campaign_advance_end_count())goto campaign_completed;
                UpdatePlayer();
                /* MAZE.S::PlayAvP calls Computer() only AFTER NextFrame
                 * finishes UpdateAMPs, Process_EndGame, PreFrame,
                 * SwapScreens, PostFrame and DoPause. The previous PC route
                 * entered the terminal mid-frame immediately after
                 * UpdatePlayer, then advanced enemies/HUD after returning. */
                /* AMP.S samples Jaguar's live video beam register ($F00004)
                 * when deciding whether a ranged Marine can attack. Without
                 * a host value beam_counter remains zero forever and every
                 * enemy Marine's ranged attack is suppressed. Advance the
                 * emulated 60 Hz display phase at the configured simulation
                 * cadence; retain the retail eight-phase firing gate. */
                /* The Jaguar beam and the 68000 enemy update loop are not
                 * phase-locked. A fixed phase increment aliases with Marine
                 * AI's every-other-tick evaluation at some simulation rates.
                 * Drift the emulated eight-phase beam once every 16 logical
                 * ticks so both AI parities encounter its firing window.
                 * No movement speed or attack probability is changed. */
                avp_amp_set_beam_counter((u32)(ticks + (ticks >> 4)));
                UpdateAMPs();
                /* HUD.S::HP2 reads two current stereo samples from the Jaguar
                 * sound buffer. Supply the hosted FullSynth output, rather
                 * than leaving both inputs permanently zero. */
                {s32 left=0,right=0;
                    avp_jaguar_fullsynth_meter_samples(&left,&right);
                    avp_hud_set_pred_meter_samples(left,right);
                }
                avp_dev_cheats_tick();
                if(player_energy<0)player_energy=0;
                /* Banana God Mode lets damage reach zero without triggering
                 * the original death path. It never heals the player. */
                {int god_now=avp_pc_video_original_god(v);
                 if(god_now!=pc_original_god_last){
                     const AvpRuntimeOps *ops=avp_runtime_ops();
                     cheat=god_now?(s16)-1:0;
                     pc_original_god_last=god_now;
                     /* Retail PLAYER.S::ExtraKeys routes 5/6 to the existing
                      * CHEAT ON/OFF HUD_MSG records (five 10-Hz ticks). The
                      * F12 switch and Option+5 must use that same path. */
                     if(ops && ops->set_message)
                         ops->set_message(ops->user,god_now?5u:6u);
                 }
                }
                Process_EndGame();
                avp_hud_set_clock((u32)ticks,simulation_hz);
                /* HUD_human calls Countdown during UpdtHUD. Calling it here
                 * as well doubled the flash update on each 10 Hz tick. */
                PreFrame();
                /* NextFrame calls SwapScreens between PreFrame and PostFrame;
                 * this flips the CPU-visible build_screen each simulation
                 * tick even when the hosted OP swap callback is a no-op. */
                SwapScreens();
                PostFrame();
                /* MAZE/HUD_MSG.S expects a message-state advance before the
                 * HUD draw. The PC loop previously never called UpdtHUDMsg,
                 * so queued pickups, countdown and GAME OVER were not advanced. */
                UpdtHUDMsg();
                UpdtHUD();
                if(msg_status==AVP_HUDMSG_STOP)
                    avp_pc_video_hud_message_clear(v);
                {int pause_edge=((joy_edge&(1u<<PAUSE))==0u);DoPause();if(pause_edge){
                    prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;
                    avp_pc_video_reset_presentation(v);
                    next_sim=avp_pc_now_ns()+(1000000000ull/simulation_hz);
                }}
                /* The original PlayAvP checks comp_panel after NextFrame,
                 * not between UpdatePlayer and UpdateAMPs. This also keeps
                 * an escape-pod launch from skipping the preceding frame's
                 * enemy/HUD bookkeeping. No extra simulation tick is run. */
                if(comp_panel){
                    enter_retail_computer(v);
                    prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;
                    avp_pc_video_reset_presentation(v);
                    next_sim=avp_pc_now_ns()+(1000000000ull/simulation_hz);
                }
                if(launch_flag)goto campaign_completed;
            }else{
                readpad();
                MovePlayer();
            }
            /* The override is tick-scoped. If scripts or digital input changed
             * the CPU's angle, reanchor the host camera on the next poll. */
            avp_player_host_live_heading(0,0);
            avp_player_host_quick_turn_heading(0,0);
            if(avp_pc_quick_turn_complete(&quick_turn,now)){
                avp_pc_quick_turn_cancel(&quick_turn);
                /* The last authoritative sample already equals the precise
                 * target. Never interpolate backward to a previous tick. */
                prev_angle=centre_angle;
                last_enhanced_tick=0;
                last_enhanced_delta=0.0;
            }
            if(live_heading.active&&
               centre_angle!=avp_pc_live_heading_word(&live_heading))
                avp_pc_live_heading_reset(&live_heading);
            live_last_game_angle=centre_angle;
            if(new_level>0&&(new_level!=cur_level||use_cocoon)&&rom_ok){
                unsigned nl=(unsigned)new_level;
                const int duct_crossing=(nl>=AVP_FIRST_DUCT&&nl<=AVP_LAST_DUCT)||
                    (cur_level>=AVP_FIRST_DUCT&&cur_level<=AVP_LAST_DUCT);
                if(enter_retail_level(&rom,mazes,maze_count,nl,1,v,&logical_level)){
                    avp_pc_quick_turn_cancel(&quick_turn);
                    avp_player_host_quick_turn_reset();
                    prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;
                    /* A real-time duct fade has advanced 64 VBlanks. Do not
                     * replay that elapsed wall time as a backlog of gameplay
                     * simulation ticks upon arrival at the new level. */
                    if(duct_crossing){
                        next_sim=avp_pc_now_ns()+(1000000000ull/simulation_hz);
                        next_display=avp_pc_now_ns()+(1000000000ull/60u);
                    }
                    logmsg("Retail transition -> level %u.\n",nl);
                }else{logmsg("Retail transition target %u unavailable.\n",nl);new_level=0;}
            }
            /* Observe after any retail level transition: ResetMaze clears the
               outgoing map, and the first frame on the incoming level must
               already contain the entry cell.  Sampling before transition
               left the newly reset map completely blank until another tick. */
            avp_pc_video_automap_observe(v,avp_collision_maze_data(),maze_width,maze_height,x_pos,y_pos,centre_angle,logical_level);
            crash_stage=8;next_sim+=(1000000000ull/simulation_hz);++ticks;++catchup;if(ticks==1)logmsg("First simulation tick complete.\n");if(ticks<=10 || (ticks%600u)==0u)logmsg("tick %llu x=%08x y=%08x pad=%08x\n",(unsigned long long)ticks,x_pos,y_pos,(unsigned)pad);}
        goto campaign_frame_complete;
manual_return_to_title:
        logmsg("Manual Restart Game: return to Jaguar title.\n");
        /* Host-only F12 action. Skip death/victory/High Scores and release
         * gameplay voices, then enter MAIN.S NewTitle like a clean start. */
        avp_pc_quick_turn_cancel(&quick_turn);
        avp_player_host_quick_turn_reset();
        avp_pc_runtime_set_gameplay_vblank(0);
        KillAlar();KillAll();KillAmbi();
        player_dead=0;game_over=0;launch_flag=0;savegame=0;
        score=0;new_level=0;end_count=0;
        /* Suppress only the initiating F12 accept until released on title. */
        avp_pc_video_prepare_title_return(v);
        goto restart_title_from_game;
campaign_completed:
        avp_pc_quick_turn_cancel(&quick_turn);
        avp_player_host_quick_turn_reset();
        /* PlayAvP has returned: hand death or victory to MAIN.S's authored
         * ending, Hall of Fame and title screens. Reinitialize every gameplay
         * system before the selected next campaign; do not reuse old AMPs. */
        logmsg("Campaign ended: species %d, dead %d, launch %d.\n",
               player_type,player_dead,launch_flag);
        /* MAZE.S FadeDown is 0 -> 0x80 in steps of two per VBL.  The
         * host 10-Hz ending countdown can expire before that last VBL,
         * abruptly exposing the front end over an un-faded gameplay frame.
         * Complete the same original fade (no new rate or screen effect)
         * before relinquishing the gameplay display list. */
        if(player_dead && fade_rate>0 && !faded){
            while(!faded){VSync();MazeList();}
        }
        avp_pc_runtime_set_gameplay_vblank(0);
        /* MAZE.S::PlayAvP exit clears the countdown alarm, ends playing
         * effects, then releases the ambient voice before MAIN.S starts
         * its ending/High Scores presentation. KillAll alone stops the
         * hosted samples but retains alarm_id/ambient_id in CPU state. */
        KillAlar();
        KillAll();
        KillAmbi();
        avp_post_game_frontend();
restart_title_from_game:
        NewTitle();
        if(!pc_begin_campaign(&rom,mazes,maze_count,v,&logical_level,1,0u)){
            logmsg("Next campaign cannot start: missing level resources.\n");
            quit=1;
        }
        /* Never load a previous campaign's in-memory quickstate as if it
         * belonged to the newly selected game. */
        avp_pc_quickstate_destroy(quickstate);
        quickstate=avp_pc_quickstate_create();
        ticks=0;
        avp_pc_video_reset_presentation(v);
        prev_x=x_pos;prev_y=y_pos;prev_angle=centre_angle;
        avp_pc_video_snapshot_sim(v,avp_collision_maze_data(),maze_width,maze_height);
        avp_pc_runtime_set_gameplay_vblank(1);
        next_sim=avp_pc_now_ns()+(1000000000ull/simulation_hz);
        next_display=avp_pc_now_ns()+(1000000000ull/60u);
        next_render=avp_pc_now_ns();
        continue;
campaign_frame_complete:
        /* If the process was suspended for a long time, resynchronise without
           trying to replay an unbounded backlog. */
        /* The timestamp captured before X11 polling and up to five game
         * updates can be stale by an entire display interval.  Refresh the
         * host clock before deciding whether a presentation slot is due. */
        now=avp_pc_now_ns();
        if(catchup==5u && now>=next_sim && (now-next_sim)>1000000000ull)next_sim=now+(1000000000ull/simulation_hz);
        /* Display frames are paced to the Jaguar's 60 Hz VBL.  The previous
         * unconditional redraw loop repeatedly rasterized the full 1440p
         * world at the X11 event-polling rate, wasting many framebuffers. */
        if(now>=next_render){
            /* Preserve the original 60-Hz host frame-grid phase when late.
             * Neither next_sim nor the Jaguar update cadence is affected. */
            next_render=avp_presentation_next_display_deadline(next_render,now,
                               avp_presentation_host_frame_period(avp_pc_video_presentation_hz(v)));
            crash_stage=9;{u32 rx=x_pos,ry=y_pos,ra=centre_angle;double alpha=1.0;
             const int smooth=avp_pc_video_smooth_presentation(v);
             if(ticks>0&&(smooth||quick_turn.active)){
                u64 render_now=avp_pc_now_ns();
                AvpPresentationPose pp={prev_x,prev_y,prev_angle},cp={x_pos,y_pos,centre_angle},rp;
                if(render_now>=next_sim)alpha=1.0;
                else alpha=1.0-(double)(next_sim-render_now)/(1000000000.0/(double)simulation_hz);
                if(alpha<0.0)alpha=0.0;
                if(alpha>1.0)alpha=1.0;
                if(smooth){
                    if((avp_pc_video_enhanced_analog_enabled(v)
                        ? avp_presentation_interpolate_pose_continuous(&pp,&cp,alpha,&rp)
                        : avp_presentation_interpolate_pose(&pp,&cp,alpha,&rp))){
                        rx=rp.x;ry=rp.y;ra=rp.angle;
                    }else{rx=x_pos;ry=y_pos;ra=centre_angle;}
                }
                /* The standard previous -> current pose interpolation already
                 * gives continuous keyboard yaw across 10-Hz tick boundaries.
                 * Predicting the NEXT step from the live key state made the
                 * visual heading leap forward when pressed and jump backward
                 * when released between ticks. The authoritative Jaguar
                 * MovePlayer turn, acceleration and snap remain unchanged. */
                const int keyboard_dir=smooth&&!quick_turn.active ?
                    avp_pc_video_keyboard_turn_direction(v):0;
                /* Enhanced joystick aiming keeps full precision inside the
                 * 10-Hz gameplay state in either mode. Only its VIDEO heading
                 * is interpolated when Smooth Presentation is enabled. */
                if(avp_pc_turn_visual_override_allowed(smooth,
                     !keyboard_dir&&avp_pc_video_enhanced_analog_enabled(v)&&last_enhanced_tick)){
                    u32 visual_angle;
                    if(avp_pc_continuous_heading_frame(prev_angle,centre_angle,
                          last_enhanced_delta,alpha,&visual_angle))ra=visual_angle;
                }
                /* When a real gamepad is present, visual yaw is integrated at
                 * host input-poll cadence, not inferred from 10-Hz endpoints. */
                if(avp_pc_turn_visual_override_allowed(smooth,!keyboard_dir&&live_heading.active))
                    ra=avp_pc_live_heading_word(&live_heading);
                if(quick_turn.active)
                    ra=avp_pc_quick_turn_angle(&quick_turn,render_now);
            }avp_pc_video_set_render_alpha(v,alpha);avp_pc_video_render_maze(v,avp_collision_maze_data(),maze_width,maze_height,rx,ry,ra,rom_ok,logical_level,ticks,simulation_hz);}        }
        /* The legacy unconditional 2-ms polling sleep could cross a nearer
         * 60-Hz rendering/VBL deadline.  Sleep no longer than the closest
         * pending event; keep the same 2-ms event-poll upper bound. */
        {u64 after_render=avp_pc_now_ns();
         u64 budget=avp_presentation_sleep_budget(after_render,next_sim,
                                                  next_display,next_render,
                                                  2000000ull);
         if(budget)avp_pc_sleep_ns(budget);
        }
    }}
#ifdef AVP_LOCAL_MULTIPLAYER
pc_shutdown:
#endif
    logmsg("Shutting down normally at tick %llu.\n",(unsigned long long)ticks);avp_pc_runtime_set_gameplay_vblank(0);avp_pc_audio_shutdown();avp_pc_eeprom_close();avp_pc_quickstate_destroy(quickstate);avp_pc_video_destroy(v);avp_resources_free_mazes(mazes,maze_count<64?maze_count:64);avp_pc_rom_free(&rom);avp_runtime_reset_ops();if(pc_log)fclose(pc_log);return 0;
}
