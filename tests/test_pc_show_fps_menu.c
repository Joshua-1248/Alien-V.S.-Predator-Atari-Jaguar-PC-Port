/* F12 Video / Show FPS: standalone host settings and timing checks.
 * The test includes the real menu implementation, not a reimplementation. */
/* Verify assertions even in CMake Release builds (which define NDEBUG). */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/platform/pc/pc_video_x11.c"
/* This menu-only harness has no running sound device; link the new
 * independent R52 hissing mixer signature without changing production code. */
void avp_pc_audio_set_mix(unsigned music,unsigned sfx,unsigned ambience,
    unsigned voice,unsigned hissing,int music_on,int ambience_on,int hissing_on)
{(void)music;(void)sfx;(void)ambience;(void)voice;(void)hissing;
 (void)music_on;(void)ambience_on;(void)hissing_on;}
void avp_pc_audio_set_speaker_mode(unsigned mode,unsigned subwoofers)
{(void)mode;(void)subwoofers;}
s16 cur_level=1;
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_gpu_wall_texture_file(unsigned level,u8 wall,unsigned *file_id)
{(void)level;if(file_id)*file_id=(unsigned)wall;return 1;}
int avp_gpu_flat_texture_file(unsigned level,u8 flat,unsigned *file_id)
{(void)level;if(file_id)*file_id=(unsigned)flat;return 1;}
void avp_game_micro_opt_set(int enabled){(void)enabled;}
int avp_game_micro_opt_available(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
/* The remaining menu branches are linked but are not invoked here. */
#ifdef AVP_LOCAL_MULTIPLAYER
void avp_mp_input_write_settings(const AvpMpInputConfig *cfg, FILE *f)
{(void)cfg;(void)f;}
#endif
int avp_jaguar_framebuffer_init(AvpJaguarFramebuffer *fb,unsigned w,unsigned h)
{(void)fb;(void)w;(void)h;return 0;}
void avp_jaguar_framebuffer_destroy(AvpJaguarFramebuffer *fb){(void)fb;}
void avp_mp_input_validate(AvpMpInputConfig *cfg){(void)cfg;}
/* Menu-only link: device enumeration/capture is not exercised here. */
unsigned avp_mp_mice_discover(AvpMpPhysicalMouse out[AVP_MP_DISCOVER_MICE])
{(void)out;return 0;}
int avp_mp_input_assign_mouse(AvpMpInputConfig *cfg,unsigned p,const char *path)
{(void)cfg;(void)p;(void)path;return 0;}
int avp_mp_input_cycle_gamepad(AvpMpInputConfig *cfg,unsigned p,
    const char *const *paths,unsigned count,const char *p1,int direction)
{(void)cfg;(void)p;(void)paths;(void)count;(void)p1;(void)direction;return 0;}
int avp_mp_pad_reader_open(AvpMpPadReader *r,const char *path)
{(void)r;(void)path;return 0;}
int avp_mp_pad_reader_poll(AvpMpPadReader *r,AvpMpControllerSample *out)
{(void)r;(void)out;return 0;}
void avp_mp_pad_reader_close(AvpMpPadReader *r){(void)r;}
unsigned avp_pc_prototype_textures_apply_search(AvpPcPrototypeTextures *s,
    AvpAvpGpuState *g,const char *a,const char *b,const char *c,int d,
    char *e,size_t f)
{(void)s;(void)g;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return 0;}

int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof *v);
    FILE *f; char line[256];int saved=0;
    assert(v);
    avp_mm_init(&v->map_maker);
    settings_defaults(&v->cfg);
    /* R65: F12 New action is Enter-only and targets one selected player.
     * The current-level selection is resolved later in the bound AMP world. */
    {
        int row=-1;
        for(int i=0;i<(int)(sizeof(cheat_items)/sizeof(cheat_items[0]));i++)
            if(cheat_items[i]==175)row=i;
        assert(row>=0);
        v->menu_page=MENU_CHEATS_NEW;
        v->menu_cursor=row;
        assert(menu_id(v,row)==175);
        v->cheat_target=0;
        menu_choose(v);
        assert(avp_pc_video_take_actions(v)==AVP_PC_ACTION_CLEAR_ENEMIES);
#ifdef AVP_LOCAL_MULTIPLAYER
        v->cheat_target=2;
        menu_choose(v);
        assert(avp_pc_video_take_actions(v)==0u);
        assert(avp_pc_video_take_local_mp_cheat_actions(v,2u)==AVP_PC_ACTION_CLEAR_ENEMIES);
        assert(avp_pc_video_take_local_mp_cheat_actions(v,1u)==0u);
#endif
        /* R67: the new hazard action is also Enter-only and target-aware. */
        row=-1;
        for(int i=0;i<(int)(sizeof(cheat_items)/sizeof(cheat_items[0]));i++)
            if(cheat_items[i]==176)row=i;
        assert(row>=0);
        v->menu_cursor=row;
        assert(menu_id(v,row)==176);
        v->cheat_target=0u;
        menu_choose(v);
        assert(avp_pc_video_take_actions(v)==AVP_PC_ACTION_CLEAR_HAZARDS);
#ifdef AVP_LOCAL_MULTIPLAYER
        v->cheat_target=3u;
        menu_choose(v);
        assert(avp_pc_video_take_actions(v)==0u);
        assert(avp_pc_video_take_local_mp_cheat_actions(v,3u)==AVP_PC_ACTION_CLEAR_HAZARDS);
        assert(avp_pc_video_take_local_mp_cheat_actions(v,2u)==0u);
#endif
        v->cheat_target=0u;
    }
    assert(!v->cfg.show_fps);
    v->menu_page=MENU_VIDEO;
#ifdef AVP_LOCAL_MULTIPLAYER
    assert(menu_count(v)==12);
    const int row=8;
#else
    assert(menu_count(v)==10);
    const int row=6;
#endif
    snprintf(v->settings_path,sizeof v->settings_path,
        "/tmp/avp-re31-r8-showfps-%ld.cfg",(long)getpid());
    /* R75: passive SBS display/viewer support; menu exposes actual stereo controls. */
    v->menu_cursor=menu_count(v)-1;
    menu_choose(v);
    assert(v->menu_page==MENU_VR_HEADSET && menu_count(v)==7);
    assert(fabs(v->cfg.vr_convergence-4.0f)<0.001f);
    assert(v->cfg.vr_eye_aspect==1u);
    assert(!v->cfg.vr_sbs_preview);
    assert(fabs(v->cfg.vr_eye_separation-0.064f)<0.001f);
    v->menu_cursor=0;menu_choose(v);
    assert(v->cfg.vr_sbs_preview);
    v->menu_cursor=1;menu_vr_adjust(v,1,1);
    assert(fabs(v->cfg.vr_eye_separation-0.072f)<0.001f);
    /* No runtime probing, tracking, or unavailable-headset rows remain. */
    v->menu_cursor=2;menu_vr_adjust(v,2,1);
    assert(fabs(v->cfg.vr_convergence-4.5f)<0.001f);
    v->menu_cursor=3;menu_vr_adjust(v,3,1);
    assert(v->cfg.vr_eye_aspect==2u);
    v->menu_cursor=4;menu_vr_adjust(v,4,1);
    assert(v->vr_preview_yaw_deg==5);
    v->menu_cursor=5;menu_choose(v);
    assert(v->vr_preview_yaw_deg==0);
    v->menu_cursor=6;menu_choose(v);
    assert(v->vr_screenshot_requested==1);
    assert(menu_count(v)==7);
    assert(v->menu_page==MENU_VR_HEADSET);
    assert(v->cfg.vr_sbs_preview);
    {char line[256];int saw_preview=0,saw_separation=0,saw_convergence=0,saw_aspect=0;FILE *f=fopen(v->settings_path,"r");
     assert(f);
     while(fgets(line,sizeof(line),f)){
         if(strstr(line,"video.vr_sbs_preview=true"))saw_preview=1;
         if(strstr(line,"video.vr_eye_separation=0.072"))saw_separation=1;
         if(strstr(line,"video.vr_convergence=4.50"))saw_convergence=1;
         if(strstr(line,"video.vr_eye_aspect=2"))saw_aspect=1;
     }
     fclose(f);assert(saw_preview&&saw_separation&&saw_convergence&&saw_aspect);
    }
    /* Ctrl+Mouse is presentation-only. No aim/turn impulse enters the game. */
    v->menu_open=0;v->cfg.mouse_enabled=1;v->mouse_grabbed=1;
    v->key[42]=1;v->pressed_sym[42]=XK_Control_L;
    v->mouse_delta=30.0f;v->sim_turn=0.0f;
    assert(avp_pc_video_modern_turn(v)==0.0f);
    assert(v->vr_preview_yaw_deg==6);
    assert(v->mouse_delta==0.0f&&!v->mouse_aim_active);
    v->mouse_delta=10.0f;v->pressed_sym[42]=NoSymbol;v->key[42]=0;
    assert(avp_pc_video_modern_turn(v)>0.0f);
    assert(v->vr_preview_yaw_deg==6); /* regular mouse unchanged */
    v->menu_open=1;v->mouse_grabbed=0;
    v->menu_cursor=0;menu_choose(v); /* restore preview OFF */
    assert(!v->cfg.vr_sbs_preview);
    menu_back(v);assert(v->menu_page==MENU_VIDEO);
    /* Arrow and Enter are both handled by the same Video adjust logic. */
    v->menu_cursor=row;
    menu_video_adjust(v,row,1);
    assert(v->cfg.show_fps);
    f=fopen(v->settings_path,"r");assert(f);
    while(fgets(line,sizeof line,f))
        if(strstr(line,"video.show_fps=true"))saved=1;
    fclose(f);assert(saved);
    v->fps_meter.sample_valid=1;
    v->fps_meter.display_fps=240;
    menu_video_adjust(v,row,-1);
    assert(!v->cfg.show_fps);
    assert(!v->fps_meter.sample_valid && !v->fps_meter.display_fps);
    v->menu_cursor=row;
    menu_choose(v);
    assert(v->cfg.show_fps);
    /* Optional V-Sync uses the next row.  In a test without X11, model a
     * successful Present backend initialization solely for menu toggling. */
    assert(!v->cfg.video_vsync);
#ifndef _WIN32
    assert(!avp_present_available(v));
    menu_video_adjust(v,row+1,1);
    assert(!v->cfg.video_vsync); /* unavailable must not claim success */
    v->present_library=(void *)1;v->present_event_id=1;
#endif
    assert(avp_present_available(v));
    menu_video_adjust(v,row+1,1);
    assert(v->cfg.video_vsync);
    f=fopen(v->settings_path,"r");assert(f);saved=0;
    while(fgets(line,sizeof line,f))if(strstr(line,"video.vsync=true"))saved=1;
    fclose(f);assert(saved);
    menu_video_adjust(v,row+1,-1);
    assert(!v->cfg.video_vsync);
#ifndef _WIN32
    /* R15: no partial-frame F12 menu exposure at ANY selected rate.
     * In particular both 50 Hz and rates above 60 Hz must take the
     * one-copy backbuffer path with V-Sync disabled. Normal gameplay
     * should retain direct-window presentation for performance. */
    {
        const unsigned rates[]={24,30,50,60,75,85,90,100,120,144,165,180,
            200,240,280,300,360,480,500,540,1000};
        size_t i;
        for(i=0;i<sizeof(rates)/sizeof(rates[0]);++i){
            v->cfg.presentation_hz=rates[i];
            v->menu_open=1;
            v->cfg.video_vsync=0;
            assert(!pc_x11_direct_window_allowed(v,0));
            v->menu_open=0;
            assert(pc_x11_direct_window_allowed(v,0));
            v->menu_open=1;
            v->cfg.video_vsync=1;
            assert(!pc_x11_direct_window_allowed(v,0));
            v->cfg.video_vsync=0;
        }
        v->menu_open=0;
        assert(!pc_x11_direct_window_allowed(v,1));
    }
#endif
    /* Map Maker lives on F12 root, with real Enter navigation. The editor
     * always stages a private copy; here the no-ROM case is a blank draft. */
    v->menu_page=MENU_ROOT;
    assert(menu_count(v)==9);
    v->menu_cursor=7;
    menu_choose(v);
    assert(v->menu_page==MENU_MAP_MAKER);
    assert(v->map_maker.level==1u && v->map_maker.w==64u);
    assert(v->map_maker.h==64u && v->map_maker.custom_map);
    assert(v->map_maker.x==32u && v->map_maker.y==32u);
    assert(v->map_maker.field==5u);
    /* R21: Tab copies; regular Enter places a floor in 2D; KP Enter
     * toggles view without triggering the editor paint action. */
    map_maker_key(v,XK_Left);assert(v->map_maker.x==31u);
    map_maker_key(v,XK_Right);assert(v->map_maker.x==32u);
    map_maker_key(v,XK_Return);
    assert(v->map_maker.cells[((32u*64u+32u)*8u)+5u]==1u);
    map_maker_key(v,XK_Tab);
    assert(v->map_maker.has_copied_texture);
    assert(v->map_maker.mode_3d==0);
    map_maker_key(v,XK_KP_Enter);
    assert(v->map_maker.mode_3d==1);
    map_maker_key(v,XK_KP_Enter);
    assert(v->map_maker.mode_3d==0);
    map_maker_key(v,XK_f);
    assert(v->map_maker.field==6u);
    /* Palette remains functional for the selected Jaguar cell byte. */
    map_maker_key(v,XK_t);
    assert(v->map_maker.palette_open);
    v->map_maker.palette_id=7u;
    map_maker_key(v,XK_Return);
    assert(!v->map_maker.palette_open);
    assert(avp_mm_value(&v->map_maker)==7u);
    /* RE34: Escape, N requests a new blank draft, warns before discarding
     * edits, and permits cancellation without losing the dirty map. */
    assert(v->map_maker.dirty);
    map_maker_key(v,XK_Escape);
    assert(v->map_maker.context_open);
    map_maker_key(v,XK_n);
    assert(v->map_maker.new_confirm_open);
    map_maker_key(v,XK_n); /* No: retain edits */
    assert(!v->map_maker.new_confirm_open && v->map_maker.context_open);
    assert(avp_mm_value(&v->map_maker)==7u && v->map_maker.dirty);
    map_maker_key(v,XK_n);
    assert(v->map_maker.new_confirm_open);
    map_maker_key(v,XK_Return); /* Yes: clear */
    assert(!v->map_maker.new_confirm_open && !v->map_maker.context_open);
    assert(v->map_maker.custom_map && !v->map_maker.dirty);
    assert(v->map_maker.cells[(32u*64u+32u)*8u+5u]==0u);
    assert(!strcmp(v->map_maker.map_stem,"new_map"));
    map_maker_key(v,XK_b);
    assert(v->menu_page==MENU_MM_BINDINGS && menu_count(v)==3);
    v->menu_cursor=0;menu_choose(v);
    assert(v->menu_page==MENU_MM_KEYS && menu_count(v)==AVP_MM_ACTIONS);
    menu_back(v);assert(v->menu_page==MENU_MM_BINDINGS);
    v->menu_cursor=1;menu_choose(v);
    assert(v->menu_page==MENU_MM_MOUSE && menu_count(v)==AVP_MM_ACTIONS);
    menu_back(v);assert(v->menu_page==MENU_MM_BINDINGS);
    v->menu_cursor=2;menu_choose(v);
    assert(v->menu_page==MENU_MM_PAD && menu_count(v)==AVP_MM_ACTIONS);
    menu_back(v);assert(v->menu_page==MENU_MM_BINDINGS);
    menu_back(v);assert(v->menu_page==MENU_MAP_MAKER);
    /* Held arrow/gamepad navigation must not divide by menu_count()==0;
     * it is cell movement while the editor is active. */
    map_maker_navigation_step(&v->map_maker,4);
    assert(v->map_maker.x==33u);
    map_maker_navigation_step(&v->map_maker,3);
    assert(v->map_maker.x==32u);
    map_maker_navigation_step(&v->map_maker,2);
    assert(v->map_maker.y==33u);
    map_maker_navigation_step(&v->map_maker,1);
    assert(v->map_maker.y==32u);
    menu_back(v);
    assert(v->menu_page==MENU_ROOT);
    /* RE33: Cheat actions belong in Cheats, not P2/P3/P4 controls.
     * The selected human player is independent of the configured device. */
    v->menu_page=MENU_ROOT;v->menu_cursor=4;
    menu_choose(v);assert(v->menu_page==MENU_CHEATS && menu_count(v)==3);
    v->menu_cursor=2;menu_choose(v);
    /* Enter does nothing: this is a single inline Left/Right selector. */
    assert(v->menu_page==MENU_CHEATS && v->cheat_target==0u);
    menu_cheats_target_adjust(v,1); /* Right -> Player 2 */
    assert(v->menu_page==MENU_CHEATS && v->menu_cursor==2 && v->cheat_target==1u);
    menu_cheats_target_adjust(v,-1); /* Left -> Player 1 */
    assert(v->cheat_target==0u);
    menu_cheats_target_adjust(v,-1); /* Left wraps -> Player 4 */
    assert(v->cheat_target==3u);
    menu_cheats_target_adjust(v,1); /* Right wraps -> Player 1 */
    assert(v->cheat_target==0u);
    menu_cheats_target_adjust(v,1); /* Right -> Player 2 */
    assert(v->cheat_target==1u);
    v->menu_cursor=1;menu_choose(v);
    assert(v->menu_page==MENU_CHEATS_NEW);
    for(int row=0;row<menu_count(v);row++)if(menu_id(v,row)==50){
        v->menu_cursor=row;menu_choose(v);break;
    }
    assert(v->local_mp_controls.player[1].cheat_infinite_health==1);
    assert(v->infinite_health==0);
    for(int row=0;row<menu_count(v);row++)if(menu_id(v,row)==48){
        v->menu_cursor=row;menu_choose(v);break;
    }
    for(int row=0;row<menu_count(v);row++)if(menu_id(v,row)==173){
        v->menu_cursor=row;menu_choose(v);break;
    }
    assert(v->local_mp_controls.player[1].cheat_reveal_map==1);
    assert(v->local_mp_controls.player[0].cheat_reveal_map==0);
    assert(avp_pc_video_take_local_mp_cheat_actions(v,1u)==AVP_PC_ACTION_RESTORE_HEALTH);
    assert(avp_pc_video_take_local_mp_cheat_actions(v,0u)==0u);
    /* One unified Reveal Entire Map row. Switching Activate For to P1 must
     * reveal P1's normal single-player automap, while P2 stays independent. */
    menu_back(v);assert(v->menu_page==MENU_CHEATS);
    v->menu_cursor=2;
    menu_cheats_target_adjust(v,-1);
    assert(v->cheat_target==0u && v->menu_page==MENU_CHEATS);
    v->menu_cursor=1;menu_choose(v);
    assert(v->menu_page==MENU_CHEATS_NEW);
    for(int row=0;row<menu_count(v);row++)if(menu_id(v,row)==173){
        v->menu_cursor=row;menu_choose(v);break;
    }
    assert(v->local_mp_controls.player[0].cheat_reveal_map);
    assert(v->local_mp_controls.player[1].cheat_reveal_map);
    {
        u8 maze[8u*8u*8u]={0};
        maze[((7u*8u+7u)*8u)+0u]=1u;
        avp_gpu_automap_reset(&v->gpu);
        avp_pc_video_automap_observe(v,maze,8u,8u,1u<<16,1u<<16,0u,1u);
        assert(v->gpu.automap_seen[7u*64u+7u]==1u);
        menu_choose(v); /* P1 Reveal Entire Map OFF again. */
        assert(!v->local_mp_controls.player[0].cheat_reveal_map);
        assert(v->local_mp_controls.player[1].cheat_reveal_map);
        avp_gpu_automap_reset(&v->gpu);
        avp_pc_video_automap_observe(v,maze,8u,8u,1u<<16,1u<<16,0u,1u);
        assert(v->gpu.automap_seen[7u*64u+7u]==0u);
    }
    menu_back(v);assert(v->menu_page==MENU_CHEATS);
    v->menu_cursor=2;
    menu_cheats_target_adjust(v,1);
    menu_cheats_target_adjust(v,1);
    menu_cheats_target_adjust(v,1);
    assert(v->cheat_target==3u && v->menu_page==MENU_CHEATS);
    v->menu_cursor=0;menu_choose(v);
    assert(v->menu_page==MENU_CHEATS_ORIGINAL);
    v->menu_cursor=1;menu_choose(v);
    assert(v->menu_page==MENU_CHEATS_BANANA);
    v->menu_cursor=0;menu_choose(v);assert(v->orig_banana[3]);
    v->menu_cursor=9;menu_choose(v);
    assert(avp_pc_video_original_god_player(v,3u));
    assert(!avp_pc_video_original_god_player(v,0u));
    v->menu_cursor=4;menu_choose(v);
    assert(avp_pc_video_take_local_mp_cheat_actions(v,3u)==AVP_PC_ACTION_ORIG_WEAPON1);
    avp_pc_video_original_gps_actor(v,3u,14u,0x000A8000u,0x000B7000u);
    assert(v->orig_gps_level[3]==14u && v->orig_gps_x[3]==0x000A8000u);
    menu_back(v);v->menu_cursor=2;menu_choose(v);
    assert(v->menu_page==MENU_CHEATS_GPS);
    v->menu_cursor=0;menu_choose(v);
    assert(avp_pc_video_original_positioning_player(v,3u));
#ifndef _WIN32
    assert(!pc_x11_direct_window_allowed(v,0));
#endif
    /* Every fresh campaign clears Reveal Entire Map independently for P1-P4. */
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_PLAYERS;p++)
        v->local_mp_controls.player[p].cheat_reveal_map=1;
    avp_pc_video_original_reset(v);
    assert(!avp_pc_video_original_god_player(v,3u));
    assert(!avp_pc_video_original_positioning_player(v,3u));
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_PLAYERS;p++)
        assert(!v->local_mp_controls.player[p].cheat_reveal_map);
    /* RE35: one shared Configure Player selector, no four separate menus;
     * preferences and binding pages operate on the selected profile. */
    v->menu_page=MENU_ROOT;
    assert(menu_count(v)==9);
    v->menu_cursor=8;menu_choose(v);
    assert(v->menu_page==MENU_PLAYER_OPTIONS && menu_count(v)==3);
    assert(v->local_mp_control_player==0);
    v->menu_cursor=0;menu_choose(v);
    assert(v->local_mp_control_player==1);
    v->menu_cursor=1;menu_choose(v);
    assert(v->menu_page==MENU_MP_PREFERENCES && menu_count(v)==5);
    {
        int old=v->local_mp_controls.player[1].audio_motion_tracker;
        int first_player=v->cfg.tracker_sounds_enabled;
        v->menu_cursor=0;menu_choose(v);
        assert(v->local_mp_controls.player[1].audio_motion_tracker==!old);
        assert(v->cfg.tracker_sounds_enabled==first_player);
        v->menu_cursor=4;menu_choose(v);
        assert(v->local_mp_controls.player[1].remember_automap==1);
        v->menu_cursor=0;menu_choose(v); /* restore initial value */
        assert(v->local_mp_controls.player[1].audio_motion_tracker==old);
    }
    menu_back(v);assert(v->menu_page==MENU_PLAYER_OPTIONS);
    v->menu_cursor=2;menu_choose(v);
    assert(v->menu_page==MENU_MP_CONTROLS);
    assert(menu_count(v)==AVP_MP_CONTROL_BIND_ROW+(int)AVP_MP_INPUT_ACTIONS);
    menu_back(v);assert(v->menu_page==MENU_PLAYER_OPTIONS);
    menu_back(v);assert(v->menu_page==MENU_ROOT);
    unlink(v->settings_path);
    avp_mm_destroy(&v->map_maker);
    free(v);
    puts("F12 Show FPS/V-Sync settings and atomic overlay routing at 21 rates PASS");
    return 0;
}
