#include "pc/pc_original_cheats.h"
#include "pc/pc_original_cheat_input.h"
#include "collectables.h"
#include "pc/pc_video.h"
#include "pc/pc_audio_layout.h"
#include "pc/pc_audio.h"
#ifndef _WIN32
#include "pc_idle_inhibit.h"
#endif
#include "pc/pc_platform.h"
#include "joypad.h"
#include "mjp.h"
#include "jaguar/avp_gpu/state.h"
#include "jaguar/avp_gpu/host_distance.h"
#include "jaguar/avp_gpu/assets.h"
#include "pc/pc_prototype_textures.h"
#include "jaguar/avp_gpu/software_renderer.h"
#include "jaguar/avp_gpu/host_widescreen.h"
#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/object_processor.h"
#include "jaguar/framebuffer.h"
#include "jaguar/cry.h"
#include "player.h"
#include "amp.h"
#include "mazescrn.h"
#include "weapons.h"
#include "amp.h"
#include "levels.h"
#include "mazescrn.h"
#include "maze.h"
#include "hud.h"
#include "computer.h"
#ifdef _WIN32
#include "pc/win32_xlib_compat.h"
#include "pc/win32_posix.h"
#include <xinput.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifndef _WIN32
#include <X11/XKBlib.h>
#endif
#include <X11/keysym.h>
#ifdef AVP_HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#ifdef AVP_HAVE_XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif
#endif
#include <errno.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <math.h>
#include <stdint.h>
#include "pc/pc_joybinding.h"
#include "pc/pc_input_routing.h"
#include "pc/pc_mouse_look.h"
#include "pc/pc_mouse_capture.h"
#include "pc/pc_axis_deadzone.h"
#include "pc/pc_input_buffer.h"
#include "pc/pc_x11_held_keys.h"
#include "pc/pc_analog_buffer.h"
#include "platform_pc_scale.h"
#include "pc/pc_fps_meter.h"
#include "jaguar/avp_gpu/presentation_interp.h"
#ifdef AVP_LOCAL_MULTIPLAYER
#include "local_mp.h"
#include "local_mp_input.h"
#include "local_mp_mice.h"
#include "local_mp_worlds.h"
#include "local_mp_commands.h"
#include "local_mp_gamepads.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#ifndef _WIN32
#include <dirent.h>
#include <strings.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

#define AVP_JS_MAX_AXES 16
#define AVP_JS_MAX_BUTTONS 32
#define AVP_JS_MAX_DEVICES 16
#define AVP_MENU_ITEMS 178
#define AVP_QUICK_TURN_SLOTS 4
#define AVP_MOUSE_ACTIONS 11
#define AVP_MM_ACTIONS 23
enum {MM_MODE,MM_FORWARD,MM_BACK,MM_LEFT,MM_RIGHT,MM_TURN_LEFT,MM_TURN_RIGHT,MM_PAINT,MM_TEX_MINUS,MM_TEX_PLUS,MM_GHOST_WALLS,MM_OVERHEAD_SURFACE,MM_PLAY_TEST,MM_OBJECT_PLACE,MM_OBJECT_REMOVE,MM_OBJECT_NEXT,MM_OBJECT_GROUP,MM_LOOK_UP,MM_LOOK_DOWN,MM_COPY,MM_HIGHLIGHT,MM_ROTATE_CCW,MM_ROTATE_CW};
static const char *const mm_action_names[AVP_MM_ACTIONS]={
    "Toggle 2D / 3D", "Move Forward", "Move Backward", "Strafe Left",
    "Strafe Right", "Turn Left", "Turn Right", "Apply Texture",
    "Previous Texture", "Next Texture", "Toggle Empty Wall Slots", "Floor / Ceiling Overhead", "Play Test From Here",
    "Place Object", "Remove Object", "Next Object Type", "Object Placement Group",
    "Look Up", "Look Down", "Copy Target Texture", "Toggle Surface Highlight",
    "Rotate Panel Counter-Clockwise", "Rotate Panel Clockwise"};
#include "game_micro_optimization.h"
#include "map_maker.h"
#include "map_maker_overhead.h"
#include "map_maker_texture_browser.h"
#include "map_maker_objects.h"
#include "map_maker_highlight.h"
#define AVP_MP_CONTROL_ANALOG_ROW 15
#define AVP_MP_CONTROL_ENHANCED_ROW 16
#define AVP_MP_CONTROL_SENS_ROW 17
#define AVP_MP_CONTROL_SPEED_ROW 18
#define AVP_MP_CONTROL_BIND_ROW 19
enum {MENU_ROOT, MENU_CONTROLS, MENU_GAMEPAD, MENU_KEYS, MENU_MOUSE, MENU_CHEATS, MENU_MISC,
      MENU_GAMEPAD_AXES, MENU_GAMEPAD_PC, MENU_GAMEPAD_JAGUAR, MENU_GAMEPAD_FRONTEND,
      MENU_KEYS_PC, MENU_KEYS_FRONTEND, MENU_MOUSE_SETTINGS, MENU_MOUSE_BINDINGS,
      MENU_ENHANCEMENTS, MENU_VIDEO, MENU_AUDIO, MENU_MAP_MAKER, MENU_MM_BINDINGS, MENU_MM_KEYS, MENU_MM_MOUSE, MENU_MM_PAD,
      MENU_CHEATS_ORIGINAL, MENU_CHEATS_NEW, MENU_CHEATS_FUJI,
      MENU_CHEATS_BANANA, MENU_CHEATS_GPS, MENU_CHEATS_LEVEL, MENU_DEBUG
#ifdef AVP_LOCAL_MULTIPLAYER
      ,MENU_MP_CONTROLS
#endif
};

typedef struct AvpPcSettings {
    int smooth_presentation;
    unsigned presentation_hz; /* PC rasterization only, validated against 21 host rates. */
    int show_fps; /* host presentation frame counter; off by default. */
    int video_vsync; /* host-only VBlank wait, off by default; requires backend support. */
    int disable_fog; /* PC-only rendering: no distance darkening. */
    unsigned audio_speaker_mode,audio_subwoofers;
    unsigned audio_music_volume,audio_sfx_volume,audio_ambience_volume,audio_voice_volume;
    unsigned audio_hissing_volume;
    int audio_music_on,audio_ambience_on,audio_hissing_on;
    unsigned long mm_key[AVP_MM_ACTIONS];
    int mm_mouse[AVP_MM_ACTIONS],mm_button[AVP_MM_ACTIONS];
    int game_micro_optimization; /* optional original-game SafePos fast path, off by default. */
    int tracker_sounds_enabled;
    int player_pain_sounds_enabled,facehug_pain_sounds_enabled,facehug_shake_pain_sounds_enabled;
    int high_resolution_world;
    int extend_draw_distance,extend_fog_distance; /* numbered distance settings, default 0 */
    int unlimited_shooting_range; /* original hitscan firearm traces, optional */
    int prototype_texture_restoration;
    unsigned video_aspect; /* 0 4:3, 1 16:9, 2 16:10, 3 21:9, 4 32:9 */
    unsigned video_window_mode; /* 0 windowed, 1 borderless, 2 fullscreen */
    unsigned video_window_width,video_window_height; /* remembered windowed client dimensions */
    unsigned video_refresh; /* 0 = desktop default, otherwise validated monitor Hz */
    int analog_movement,enhanced_analog_controls; float turn_sensitivity,turn_speed;
    int mouse_enabled; float mouse_sensitivity,mouse_acceleration; int mouse_button[AVP_MOUSE_ACTIONS];
    unsigned long action_key[8];
    int action_button[8];
    int gamepad_enabled;
    char gamepad_device[64];
    float deadzone,axis_deadzone[AVP_JS_MAX_AXES];
    int buffer_short_presses,buffer_short_analog;
    int remember_automap;
    int marine_pulse_los_hits,marine_fire_interrupt;
    int normalize_diagonal_contact_range; /* opt-in enemy contact reach; OFF default */
    int enemy_activation_distance; /* 0 original, 1 2x, 2 3x, 3 4x, 4 unlimited */
    int marine_start_shotgun;
    int hud_fixed_hue; /* OFF: original Jaguar RMW; ON: PC neutral reference hue. */
    int move_axis,move_invert;
    int strafe_axis,strafe_invert;
    int turn_axis,turn_invert;
    int fire_button,use_button,noclip_button,map_button;
    int pc_pause_button,pc_option_button;
    /* Host-only right 180-degree Quick Turn [Extra], four independent bindings. */
    unsigned long quick_turn_keys[AVP_QUICK_TURN_SLOTS];
    int quick_turn_buttons[AVP_QUICK_TURN_SLOTS];
    int strafe_left_button,strafe_right_button;
    int forward_button,backward_button;
    int predator_medkit_button,predator_cloak_button,predator_vision_button;
    unsigned long key_pause,key_option,key_predator_medkit,key_predator_cloak,key_predator_vision;
    unsigned long key_forward,key_back,key_turn_left,key_turn_right,key_strafe_left,key_strafe_right,key_fire,key_use,key_map,key_noclip;
    /* Frontend-only keyboard bindings; Jaguar game-input bindings unchanged. */
    unsigned long menu_key_up,menu_key_down,menu_key_left,menu_key_right,menu_key_accept,menu_key_back,menu_key_open;
    /* Additional host-only frontend joystick bindings; -1 leaves existing Jaguar controls intact. */
    int menu_button_up,menu_button_down,menu_button_left,menu_button_right,menu_button_accept,menu_button_back,menu_button_open;
    int full_cpu_systems;
    int debug_immediate_in_game;
    int controller_mode_jaguar;
    int jag_dpad_x_axis,jag_dpad_y_axis,jag_dpad_x_invert,jag_dpad_y_invert;
    int jag_up_button,jag_down_button,jag_left_button,jag_right_button;
    int jag_a_button,jag_b_button,jag_c_button,jag_pause_button,jag_option_button;
    int jag_keypad_button[12]; /* 0..9, star, hash */
} AvpPcSettings;

typedef struct AvpJsDevice { char path[64],name[128]; int axes,buttons; } AvpJsDevice;

struct AvpPcVideo {
    Display *dpy; int screen; Window win; GC gc; Pixmap backbuf; XImage *img;
#ifndef _WIN32
    AvpPcIdleInhibit *idle_inhibit;
    int idle_window_mapped, idle_window_focused;
#endif
    AvpJaguarFramebuffer fb,world_fb;
    double local_mp_render_alpha;
#ifdef AVP_LOCAL_MULTIPLAYER
    /* Modal terminal does not replace the four-player composed window. */
    uint32_t *local_mp_terminal_background;
    unsigned local_mp_terminal_width,local_mp_terminal_height;
    unsigned local_mp_terminal_owner;
    int local_mp_terminal_visible;
#endif
    /* MJP frontend target: rows 14..253 are the Jaguar's 240 displayed lines
     * from VDB (title row 0); rows 0..13 keep the established +14 frontend
     * coordinate space.  Gameplay keeps the 320x228 fb. */
    AvpJaguarFramebuffer frontend_fb;
    int presenting_frontend;
    int presenting_world_hires;
    u32 *gameplay_fade_snapshot;
    unsigned gameplay_fade_width,gameplay_fade_height;
    int gameplay_fade_hires;
#ifdef AVP_HAVE_XSHM
    /* Host presentation only: MIT-SHM lets the X server read the scaled frame
     * from shared memory instead of receiving it over the socket. */
    XShmSegmentInfo shm; int shm_active,shm_unavailable;
#endif
    unsigned win_w,win_h,present_w,present_h;
    int video_switch_pending;
#ifdef AVP_HAVE_XRANDR
    SizeID video_saved_rr_size;
    Rotation video_saved_rr_rotation;
    short video_saved_rr_rate;
    int video_rr_changed;
#endif
    uint32_t *present_pixels; XImage *present_img; unsigned char key[256]; KeySym pressed_sym[256]; Atom wm_protocols,wm_delete; int noclip;
    int menu_open,menu_index,menu_page,menu_cursor; int capture_mode,capture_target; int frontend_input;
    int title_return_wait_release;
    AvpMapMaker map_maker;
    char map_test_rom_path[640]; /* validated retail ROM path, parent only */
#ifndef _WIN32
    pid_t map_test_child;
#endif
    AvpAvpGpuState *map_editor_gpu; /* Borrows the loaded retail textures. */
    AvpJaguarFramebuffer map_editor_fb;
    uint64_t mm_last_move_ns;
    uint64_t mm_last_zoom_ns;
    int mm_drag_paint;
    int mm_select_drag_active; /* Shift+LMB 2D range selection (never paints) */
    int mm_select_3d_pending,mm_select_3d_drag_active;
    int mm_select_3d_start_x,mm_select_3d_start_y;
    int mm_select_3d_last_x,mm_select_3d_last_y;
    int mm_select_drag_px,mm_select_drag_py; /* pointer in window coordinates */
    uint64_t mm_select_drag_last_ns; /* time-based 2D selection edge scrolling */
    int mm_enter_paint_active; /* held Enter: 2D cell or 3D aimed face, one Undo */
    int mm_enter_last_valid;
    unsigned mm_enter_last_x,mm_enter_last_y;
    unsigned char mm_enter_painted[AVP_MM_3D_PAINT_VISITED_BYTES];
    uint64_t mm_nav_next_ns; /* held 2D arrow repeat */
    int mm_nav_direction;
    int mm_nav_shift_latched; /* keep shift-selection through physical key repeat */
    int mm_pan_active,mm_pan_last_x,mm_pan_last_y;
#ifdef AVP_LOCAL_MULTIPLAYER
    int local_mp_paused;
    char local_mp_dm_status[256];
#endif
    int menu_repeat_direction; uint64_t menu_repeat_next_ns;
    int mouse_grabbed,mouse_focused,mouse_recenter,mouse_aim_active,mouse_last_x,mouse_motion_seen; float mouse_delta; unsigned char mouse_down[16];
#ifndef _WIN32
    Cursor invisible_cursor;
#endif
    int stage_delta,species_request,species_dirty,hz_delta,weapon_delta; float modern_turn,modern_forward,modern_strafe;
    float sim_turn,sim_forward,sim_strafe;
    unsigned pending_actions,quick_turn_pending; int infinite_health,infinite_ammo,freeze_enemies,tracker_cheat;
    /* Session-only retail cheat switches. None are persisted in settings.cfg. */
    unsigned cheat_target; /* 0=P1 (default), 1..3=P2..P4; F12-only. */
    int orig_fuji[4],orig_banana[4],orig_position[4],orig_god[4];
    AvpPcOriginalCheatInput orig_input[4];
    u32 orig_gps_x[4],orig_gps_y[4];
    unsigned orig_gps_level[4];
    unsigned orig_species[4],orig_weapons[4];
    int orig_tracker[4],orig_actor_valid[4];
    int attacks_disabled,facehug_attach_disabled,kill_type_choice;
    AvpPcSettings cfg; char settings_path[512];
    AvpPcPrototypeTextures prototype_textures;
#ifdef AVP_LOCAL_MULTIPLAYER
    AvpLocalMpSetup local_mp;
    AvpLocalMpSetup local_mp_runtime_setup;
    AvpMpInputConfig local_mp_controls;
    unsigned local_mp_cheat_actions[AVP_LOCAL_MP_MAX_PLAYERS];
    int local_mp_walktest;
    unsigned local_mp_control_player; /* 1..3, Player 1 uses the existing settings. */
    unsigned local_mp_control_kind; /* 0=keyboard, 1=mouse, 2=gamepad */
    uint32_t local_mp_capture_prev_buttons;
    int16_t local_mp_capture_prev_axes[AVP_MP_COMMAND_AXES];
    int local_mp_open, local_mp_suppress_input;
    int local_mp_launch_pending; /* one-shot F2 -> native game handoff */
    int local_mp_hud_owner; /* -1: legacy renderer; 0..3: scoped actor HUD brush */
    unsigned local_mp_cursor;
    unsigned local_mp_page; /* R56 F2 root/Bots/Modifiers/Options */
    AvpMpCommandInput local_mp_commands;
    AvpMpMouseReader local_mp_readers[AVP_MP_INPUT_PLAYERS];
    AvpMpPadReader local_mp_pad_readers[AVP_MP_INPUT_PLAYERS];
    /* Per-player original-source software renderer output. Resource pointers
     * are borrowed from gpu; scratch/occlusion and the full frame are owned. */
    struct {
        AvpAvpGpuState *state;
        AvpJaguarFramebuffer frame;
    } local_mp_views[AVP_LOCAL_MP_MAX_PLAYERS];
#endif
    AvpJsDevice devices[AVP_JS_MAX_DEVICES]; int device_count;
    int js_fd; char js_path[64],js_name[128];
    int16_t js_axes[AVP_JS_MAX_AXES],js_prev_axes[AVP_JS_MAX_AXES];
    unsigned char js_buttons[AVP_JS_MAX_BUTTONS],js_prev_buttons[AVP_JS_MAX_BUTTONS];
    int js_axes_count,js_buttons_count;
    u32 pad_buffered,previous_poll_pad; int use_button_held,use_button_buffered;
    AvpPcAnalogBuffer analog_buffer;
    AvpAvpGpuState gpu;
    AvpGpuAutomapHistory automap_history; /* legacy P1/singleplayer level history */
    unsigned singleplayer_map_zoom_held; /* rising edges for P1 host zoom */
#ifdef AVP_LOCAL_MULTIPLAYER
    AvpGpuAutomapHistory local_mp_automap_history[AVP_LOCAL_MP_MAX_PLAYERS];
#endif
    u64 vblank_epoch_ns;
    AvpPcFpsMeter fps_meter;
#ifndef _WIN32
    /* Optional X Present (libXpresent).  No new mandatory dependency. */
    void *present_library;
    int present_opcode;
    XID present_event_id;
    Bool (*present_query_extension)(Display *,int *,int *,int *);
    XID (*present_select_input)(Display *,Window,unsigned int);
    void (*present_free_input)(Display *,Window,XID);
    void (*present_pixmap)(Display *,Window,Pixmap,uint32_t,
        unsigned long,unsigned long,int,int,unsigned long,unsigned long,
        unsigned long,uint32_t,uint64_t,uint64_t,uint64_t,void *,int);
#endif

};

static int pc_x_error(Display*d,XErrorEvent*e){char buf[256];XGetErrorText(d,e->error_code,buf,sizeof(buf));fprintf(stderr,"X11 error: code=%u request=%u minor=%u resource=0x%lx: %s\n",e->error_code,e->request_code,e->minor_code,e->resourceid,buf);fflush(stderr);return 0;}
static int pc_x_io_error(Display*d){(void)d;fprintf(stderr,"Display I/O error\n");fflush(stderr);
#ifdef _WIN32
    ExitProcess(111);
#else
    _exit(111);
#endif
    return 0;}

/* The original Jaguar renderer is a software framebuffer.  XSync() alone
 * only flushes the X protocol; it is NOT V-Sync.  When available, the X11
 * Present extension schedules a back-buffer pixmap at display VBlank.
 * The request uses PresentOptionCopy so the pixmap is idle after the scanout
 * operation.  We explicitly await PresentIdleNotify before overwriting it.
 * This is fully disabled in the original direct XShm path when V-Sync is Off.
 * XPresent is optional, loaded at runtime (no new Linux build dependency). */
#ifndef _WIN32
#define AVP_PRESENT_IDLE_NOTIFY 2
#define AVP_PRESENT_IDLE_MASK (1u << AVP_PRESENT_IDLE_NOTIFY)
#define AVP_PRESENT_OPTION_COPY 2u
static Bool avp_present_idle_predicate(Display *dpy,XEvent *event,XPointer data)
{
    const AvpPcVideo *v=(const AvpPcVideo *)data;
    (void)dpy;
    return event->type==GenericEvent &&
           event->xcookie.extension==v->present_opcode &&
           event->xcookie.evtype==AVP_PRESENT_IDLE_NOTIFY;
}
static void avp_present_init(AvpPcVideo *v)
{
    int opcode=0,event_base=0,error_base=0;
    v->present_library=dlopen("libXpresent.so.1",RTLD_NOW|RTLD_LOCAL);
    if(!v->present_library)return;
#define LOAD_PRESENT_SYMBOL(dst,name) (*(void **)(&(dst))=dlsym(v->present_library,(name)))
    LOAD_PRESENT_SYMBOL(v->present_query_extension,"XPresentQueryExtension");
    LOAD_PRESENT_SYMBOL(v->present_select_input,"XPresentSelectInput");
    LOAD_PRESENT_SYMBOL(v->present_free_input,"XPresentFreeInput");
    LOAD_PRESENT_SYMBOL(v->present_pixmap,"XPresentPixmap");
#undef LOAD_PRESENT_SYMBOL
    if(!v->present_query_extension||!v->present_select_input||
       !v->present_free_input||!v->present_pixmap||
       !v->present_query_extension(v->dpy,&opcode,&event_base,&error_base)){
        dlclose(v->present_library);v->present_library=NULL;
        v->present_pixmap=NULL;return;
    }
    v->present_opcode=opcode;
    v->present_event_id=v->present_select_input(v->dpy,v->win,AVP_PRESENT_IDLE_MASK);
    if(!v->present_event_id){dlclose(v->present_library);v->present_library=NULL;
        v->present_pixmap=NULL;}
}
static void avp_present_shutdown(AvpPcVideo *v)
{
    if(v->present_library){
        if(v->dpy&&v->win&&v->present_event_id)
            v->present_free_input(v->dpy,v->win,v->present_event_id);
        dlclose(v->present_library);v->present_library=NULL;
    }
}
static int avp_present_available(const AvpPcVideo *v)
{
    return v && v->present_library && v->present_event_id;
}
static void avp_present_wait_vblank(AvpPcVideo *v)
{
    XEvent event;
    /* PresentOptionCopy, no Async: schedule at next MSC/vblank. */
    v->present_pixmap(v->dpy,v->win,v->backbuf,0u,None,None,0,0,
        None,None,None,AVP_PRESENT_OPTION_COPY,0u,0u,0u,NULL,0);
    XFlush(v->dpy);
    /* This idle notification guarantees the pixmap may be reused safely.
     * XIfEvent leaves input/window events queued for the normal event loop. */
    XIfEvent(v->dpy,&event,avp_present_idle_predicate,(XPointer)v);
}
#else
/* Software Win32/GDI frontend: DwmFlush waits for the Desktop Window
 * Manager to present its pending frame.  When DWM is unavailable we retain
 * the legacy path and report V-Sync as unavailable. */
typedef HRESULT (WINAPI *AvpDwmFlushFn)(void);
static AvpDwmFlushFn avp_dwm_flush_fn(void)
{
    static int checked=0;
    static AvpDwmFlushFn flush_fn=NULL;
    if(!checked){HMODULE lib=LoadLibraryA("dwmapi.dll");checked=1;
        if(lib)*(void **)(&flush_fn)=(void *)GetProcAddress(lib,"DwmFlush");}
    return flush_fn;
}
static int avp_present_available(const AvpPcVideo *v)
{
    (void)v;return avp_dwm_flush_fn()!=NULL;
}
static void avp_present_wait_vblank(AvpPcVideo *v)
{
    AvpDwmFlushFn fn=avp_dwm_flush_fn();(void)v;if(fn)(void)fn();
}
#endif
static void avp_fps_count_host_frame(AvpPcVideo *v)
{
    if(v->cfg.show_fps)avp_pc_fps_meter_present(&v->fps_meter,avp_pc_now_ns());
}

static void settings_defaults(AvpPcSettings*c){memset(c,0,sizeof(*c));c->smooth_presentation=1;c->presentation_hz=60u;c->audio_speaker_mode=1u;c->audio_subwoofers=0u;
    c->audio_music_volume=c->audio_sfx_volume=100u;
    c->audio_ambience_volume=c->audio_voice_volume=c->audio_hissing_volume=100u;
    c->audio_music_on=c->audio_ambience_on=c->audio_hissing_on=1;
    c->tracker_sounds_enabled=1;c->player_pain_sounds_enabled=1;c->facehug_pain_sounds_enabled=1;c->facehug_shake_pain_sounds_enabled=1;c->high_resolution_world=0;c->video_window_width=640;c->video_window_height=456;c->analog_movement=0;c->enhanced_analog_controls=0;c->turn_sensitivity=1.0f;c->turn_speed=1.0f;c->mouse_sensitivity=1.0f;c->mouse_acceleration=0.0f;for(int i=0;i<AVP_MOUSE_ACTIONS;i++)c->mouse_button[i]=-1;c->mouse_button[0]=1;c->mouse_button[1]=3;for(int i=0;i<8;i++)c->action_button[i]=-1;for(int i=0;i<AVP_QUICK_TURN_SLOTS;i++)c->quick_turn_buttons[i]=-1;c->quick_turn_keys[0]=XK_r;
    c->action_key[0]=XK_comma;c->action_key[1]=XK_period;
    /* The Jaguar Alien always has three attack buttons (A=swipe,
       B=bite, C=tail).  Keep PC mode's separate movement bindings, but
       make the third retail attack accessible by default on the keyboard
       and on the gamepad's original Jaguar C position (button 2). */
    c->action_key[3]=XK_v; c->action_key[4]=XK_c;c->action_button[4]=2;
    c->action_key[5]=XK_n;c->action_key[6]=XK_b;c->action_key[7]=XK_BackSpace;c->gamepad_enabled=1;c->deadzone=0.20f;for(int i=0;i<AVP_JS_MAX_AXES;i++)c->axis_deadzone[i]=0.20f;c->move_axis=1;c->move_invert=1;c->strafe_axis=0;c->turn_axis=2;c->fire_button=0;c->use_button=1;c->noclip_button=-1;c->map_button=-1;c->pc_pause_button=-1;c->pc_option_button=-1;c->strafe_left_button=c->strafe_right_button=-1;c->forward_button=c->backward_button=-1;c->predator_medkit_button=c->predator_cloak_button=c->predator_vision_button=-1;c->key_pause=XK_p;c->key_option=XK_o;c->key_predator_medkit=XK_7;c->key_predator_cloak=0;c->key_predator_vision=XK_6;c->key_forward=XK_w;c->key_back=XK_s;c->key_turn_left=XK_a;c->key_turn_right=XK_d;c->key_strafe_left=XK_q;c->key_strafe_right=XK_e;c->key_fire=XK_space;c->key_use=XK_Shift_L;c->key_map=XK_Tab;c->key_noclip=XK_F8;c->full_cpu_systems=1;c->debug_immediate_in_game=0;c->controller_mode_jaguar=0;
    {const unsigned long defaults[AVP_MM_ACTIONS]={XK_KP_Enter,XK_w,XK_s,XK_a,XK_d,XK_q,XK_e,XK_Return,XK_bracketleft,XK_bracketright,XK_g,XK_c,XK_p,XK_o,XK_Delete,XK_i,XK_j,XK_Home,XK_End,XK_Tab,XK_h,XK_comma,XK_period};
     for(int i=0;i<AVP_MM_ACTIONS;i++){c->mm_key[i]=defaults[i];c->mm_mouse[i]=-1;c->mm_button[i]=-1;}
     c->mm_mouse[MM_PAINT]=-1;c->mm_button[MM_MODE]=3;c->mm_button[MM_PAINT]=0;
     c->mm_button[MM_TEX_MINUS]=4;c->mm_button[MM_TEX_PLUS]=5;}
    c->menu_key_up=XK_Up;c->menu_key_down=XK_Down;
    c->menu_key_left=XK_Left;c->menu_key_right=XK_Right;
    c->menu_key_accept=XK_Return;c->menu_key_back=XK_Escape;
    c->menu_key_open=0; /* F12 remains a permanent hardwired fallback. */
    c->menu_button_open=-1;
    c->menu_button_up=c->menu_button_down=c->menu_button_left=c->menu_button_right=-1;
    c->menu_button_accept=c->menu_button_back=-1;
    c->jag_dpad_x_axis=0;c->jag_dpad_y_axis=1;c->jag_dpad_x_invert=0;c->jag_dpad_y_invert=0;
    c->jag_up_button=c->jag_down_button=c->jag_left_button=c->jag_right_button=-1;c->jag_a_button=0;c->jag_b_button=1;c->jag_c_button=2;c->jag_option_button=6;c->jag_pause_button=7;
    for(int i=0;i<12;i++)c->jag_keypad_button[i]=-1;}
static void make_settings_path(AvpPcVideo*v){char exe[512];ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1);char*s;size_t dl;const char suffix[]="/settings.cfg";if(n<=0||(size_t)n>=sizeof(exe)){memcpy(v->settings_path,"settings.cfg",13);return;}exe[n]=0;s=strrchr(exe,'/');if(!s){memcpy(v->settings_path,"settings.cfg",13);return;}*s=0;dl=strlen(exe);if(dl+sizeof(suffix)>sizeof(v->settings_path)){memcpy(v->settings_path,"settings.cfg",13);return;}memcpy(v->settings_path,exe,dl);memcpy(v->settings_path+dl,suffix,sizeof(suffix));}
static int parse_bool(const char*s,int fallback){if(!strcasecmp(s,"true")||!strcasecmp(s,"yes")||!strcmp(s,"1")||!strcasecmp(s,"on"))return 1;if(!strcasecmp(s,"false")||!strcasecmp(s,"no")||!strcmp(s,"0")||!strcasecmp(s,"off"))return 0;return fallback;}
/* Migrate existing R38 true/false settings to modes 1/0; new files use
 * numeric indices. Reject out-of-range/corrupt values instead of wrapping. */
static int parse_distance_mode(const char *s,int fallback,int choices)
{
    char *end=NULL;long n;
    if(!strcasecmp(s,"true")||!strcasecmp(s,"yes")||!strcasecmp(s,"on"))return 1;
    if(!strcasecmp(s,"false")||!strcasecmp(s,"no")||!strcasecmp(s,"off"))return 0;
    n=strtol(s,&end,10);
    return end&&end!=s&&!*end&&n>=0&&n<choices?(int)n:fallback;
}
static void load_settings(AvpPcVideo*v){FILE*f;char linebuf[256];settings_defaults(&v->cfg);
#ifdef AVP_LOCAL_MULTIPLAYER
    avp_local_mp_defaults(&v->local_mp);
    avp_mp_input_defaults(&v->local_mp_controls);
    v->local_mp_control_player=1;
#endif
    make_settings_path(v);f=fopen(v->settings_path,"r");if(!f)return;while(fgets(linebuf,sizeof(linebuf),f)){char*k=linebuf,*eq,*val,*nl;while(*k==' '||*k=='\t')k++;if(*k=='#'||*k=='\n'||!*k)continue;eq=strchr(k,'=');if(!eq)continue;*eq=0;val=eq+1;nl=strpbrk(val,"\r\n");if(nl)*nl=0;
#ifdef AVP_LOCAL_MULTIPLAYER
        if(avp_mp_input_parse_setting(&v->local_mp_controls,k,val))continue;
        if(!strcmp(k,"local_multiplayer.enabled"))v->local_mp.enabled=(unsigned)parse_bool(val,(int)v->local_mp.enabled);
        else if(!strcmp(k,"local_multiplayer.game_mode"))v->local_mp.game_mode=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.dm_score_limit"))v->local_mp.dm_score_limit=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.dm_time_limit"))v->local_mp.dm_time_limit=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.dm_fast_movement"))v->local_mp.dm_fast_movement=(unsigned)parse_bool(val,(int)v->local_mp.dm_fast_movement);
        else if(!strcmp(k,"local_multiplayer.dm_fast_equip"))v->local_mp.dm_fast_equip=(unsigned)parse_bool(val,(int)v->local_mp.dm_fast_equip);
        else if(!strcmp(k,"local_multiplayer.dm_enemies"))v->local_mp.dm_enemies=(unsigned)parse_bool(val,(int)v->local_mp.dm_enemies);
        else if(!strcmp(k,"local_multiplayer.dm_level"))v->local_mp.dm_level=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.dm_level_travel"))v->local_mp.dm_level_travel=(unsigned)parse_bool(val,(int)v->local_mp.dm_level_travel);
        else if(!strcmp(k,"local_multiplayer.dm_cloak_damage"))v->local_mp.dm_cloak_damage=(unsigned)parse_bool(val,(int)v->local_mp.dm_cloak_damage);
        else if(!strcmp(k,"local_multiplayer.bot_count"))v->local_mp.bot_count=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.random_species_on_respawn"))v->local_mp.random_species_on_respawn=(unsigned)parse_bool(val,(int)v->local_mp.random_species_on_respawn);
        else if(!strcmp(k,"local_multiplayer.show_whole_map"))v->local_mp.show_whole_map=(unsigned)parse_bool(val,(int)v->local_mp.show_whole_map);
        else if(!strcmp(k,"local_multiplayer.show_other_players_on_automap"))v->local_mp.show_other_players_on_automap=(unsigned)parse_bool(val,(int)v->local_mp.show_other_players_on_automap);
        else if(!strncmp(k,"local_multiplayer.bot_",22)){
            const char *end=NULL;char *stop=NULL;long i=strtol(k+22,&stop,10);
            end=stop;
            if(end && i>=0 && i<(long)AVP_LOCAL_MP_MAX_BOTS){
                if(!strcmp(end,".species"))v->local_mp.bot_species[i]=(unsigned)atoi(val);
                else if(!strcmp(end,".behavior"))v->local_mp.bot_behavior[i]=(unsigned)atoi(val);
            }
        }
        else if(!strcmp(k,"local_multiplayer.players"))v->local_mp.players=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.campaign"))v->local_mp.campaign=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.layout"))v->local_mp.layout=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.three_layout"))v->local_mp.three_layout=(unsigned)atoi(val);
        else if(!strcmp(k,"local_multiplayer.respawn_campaign_start"))v->local_mp.respawn_campaign_start=(unsigned)parse_bool(val,(int)v->local_mp.respawn_campaign_start);
        else if(!strncmp(k,"local_multiplayer.player_",25)){
            const char *tail=k+25;char *end=NULL;long i=strtol(tail,&end,10);
            if(end && i>=0 && i<4){
                if(!strcmp(end,".species"))v->local_mp.species[i]=(unsigned)atoi(val);
                else if(!strcmp(end,".bot_enabled"))v->local_mp.bot_enabled[i]=(unsigned)parse_bool(val,(int)v->local_mp.bot_enabled[i]);
            }
        }
        else
#endif
        if(!strncmp(k,"map_maker.",10)){
            unsigned index;
            if(sscanf(k,"map_maker.key_%u",&index)==1&&index<AVP_MM_ACTIONS)v->cfg.mm_key[index]=strtoul(val,NULL,0);
            else if(sscanf(k,"map_maker.mouse_%u",&index)==1&&index<AVP_MM_ACTIONS)v->cfg.mm_mouse[index]=atoi(val);
            else if(sscanf(k,"map_maker.button_%u",&index)==1&&index<AVP_MM_ACTIONS)v->cfg.mm_button[index]=atoi(val);
        }
        else if(!strcmp(k,"audio.speaker_mode")){
            unsigned n=(unsigned)atoi(val);if(n<AVP_AUDIO_LAYOUT_COUNT)v->cfg.audio_speaker_mode=n;
        }
        else if(!strcmp(k,"audio.subwoofer_channels")){
            unsigned n=(unsigned)atoi(val);if(n<=2u)v->cfg.audio_subwoofers=n;
        }
        else if(!strcmp(k,"audio.music_volume")){
            int n=atoi(val);if(n>=0&&n<=100)v->cfg.audio_music_volume=(unsigned)n;
        }
        else if(!strcmp(k,"audio.sfx_volume")){
            int n=atoi(val);if(n>=0&&n<=100)v->cfg.audio_sfx_volume=(unsigned)n;
        }
        else if(!strcmp(k,"audio.ambience_volume")){
            int n=atoi(val);if(n>=0&&n<=100)v->cfg.audio_ambience_volume=(unsigned)n;
        }
        else if(!strcmp(k,"audio.ambient_hissing_noise_volume")){
            int n=atoi(val);if(n>=0&&n<=100)v->cfg.audio_hissing_volume=(unsigned)n;
        }
        else if(!strcmp(k,"audio.voice_volume")){
            int n=atoi(val);if(n>=0&&n<=100)v->cfg.audio_voice_volume=(unsigned)n;
        }
        else if(!strcmp(k,"audio.music"))v->cfg.audio_music_on=parse_bool(val,v->cfg.audio_music_on);
        /* Legacy R47 key meant level ambience; keep old settings compatible. */
        else if(!strcmp(k,"audio.ambient_noises")||!strcmp(k,"audio.ambience"))
            v->cfg.audio_ambience_on=parse_bool(val,v->cfg.audio_ambience_on);
        else if(!strcmp(k,"audio.ambient_hissing_noises"))
            v->cfg.audio_hissing_on=parse_bool(val,v->cfg.audio_hissing_on);
        else if(!strcmp(k,"video.aspect")){unsigned val_aspect=(unsigned)atoi(val);v->cfg.video_aspect=val_aspect<5u?val_aspect:0u;}
        else if(!strcmp(k,"video.window_mode")){unsigned m=(unsigned)atoi(val);v->cfg.video_window_mode=m<3u?m:0u;}
        else if(!strcmp(k,"video.window_width")){unsigned n=(unsigned)atoi(val);if(n>=320u&&n<=7680u)v->cfg.video_window_width=n;}
        else if(!strcmp(k,"video.window_height")){unsigned n=(unsigned)atoi(val);if(n>=228u&&n<=4320u)v->cfg.video_window_height=n;}
        else if(!strcmp(k,"video.refresh")){unsigned n=(unsigned)atoi(val);if(n<=360u)v->cfg.video_refresh=n;}
        else if(!strcmp(k,"video.presentation_hz")){v->cfg.presentation_hz=avp_presentation_valid_rate((unsigned)strtoul(val,NULL,10));}
        else if(!strcmp(k,"video.show_fps"))v->cfg.show_fps=parse_bool(val,v->cfg.show_fps);
        else if(!strcmp(k,"video.vsync"))v->cfg.video_vsync=parse_bool(val,v->cfg.video_vsync);
        else if(!strcmp(k,"video.disable_fog"))v->cfg.disable_fog=parse_bool(val,v->cfg.disable_fog);
        else if(!strcmp(k,"enhancement.game_micro_optimization"))v->cfg.game_micro_optimization=parse_bool(val,v->cfg.game_micro_optimization);
        else if(!strcmp(k,"controls.analog_movement"))v->cfg.analog_movement=parse_bool(val,v->cfg.analog_movement);else if(!strcmp(k,"enhancement.enhanced_analog_controls"))v->cfg.enhanced_analog_controls=parse_bool(val,v->cfg.enhanced_analog_controls);else if(!strcmp(k,"controls.turn_sensitivity"))v->cfg.turn_sensitivity=(float)atof(val);else if(!strcmp(k,"controls.turn_speed"))v->cfg.turn_speed=(float)atof(val);else if(!strcmp(k,"mouse.enabled"))v->cfg.mouse_enabled=parse_bool(val,v->cfg.mouse_enabled);else if(!strcmp(k,"mouse.sensitivity"))v->cfg.mouse_sensitivity=(float)atof(val);else if(!strcmp(k,"mouse.acceleration"))v->cfg.mouse_acceleration=(float)atof(val);else if(!strncmp(k,"mouse.button_",13)){int i=atoi(k+13);if(i>=0&&i<AVP_MOUSE_ACTIONS)v->cfg.mouse_button[i]=atoi(val);}else if(!strcmp(k,"audio.motion_tracker_sounds"))v->cfg.tracker_sounds_enabled=parse_bool(val,v->cfg.tracker_sounds_enabled);else if(!strcmp(k,"audio.player_pain_sounds"))v->cfg.player_pain_sounds_enabled=parse_bool(val,v->cfg.player_pain_sounds_enabled);else if(!strcmp(k,"audio.facehug_pain_sounds"))v->cfg.facehug_pain_sounds_enabled=parse_bool(val,v->cfg.facehug_pain_sounds_enabled);else if(!strcmp(k,"audio.facehug_shake_pain_sounds"))v->cfg.facehug_shake_pain_sounds_enabled=parse_bool(val,v->cfg.facehug_shake_pain_sounds_enabled);else if(!strcmp(k,"enhancement.high_resolution_world"))v->cfg.high_resolution_world=parse_bool(val,v->cfg.high_resolution_world);else if(!strcmp(k,"enhancement.extend_draw_distance"))v->cfg.extend_draw_distance=parse_distance_mode(val,v->cfg.extend_draw_distance,AVP_HOST_DRAW_DISTANCE_CHOICES);else if(!strcmp(k,"enhancement.unlimited_shooting_range"))v->cfg.unlimited_shooting_range=parse_bool(val,v->cfg.unlimited_shooting_range);else if(!strcmp(k,"enhancement.extend_fog_distance"))v->cfg.extend_fog_distance=parse_distance_mode(val,v->cfg.extend_fog_distance,AVP_HOST_FOG_DISTANCE_CHOICES);else if(!strcmp(k,"enhancement.prototype_texture_restoration"))v->cfg.prototype_texture_restoration=parse_bool(val,v->cfg.prototype_texture_restoration);else if(!strcmp(k,"enhancement.smooth_presentation"))v->cfg.smooth_presentation=parse_bool(val,v->cfg.smooth_presentation);else if(!strcmp(k,"gamepad.enabled"))v->cfg.gamepad_enabled=parse_bool(val,v->cfg.gamepad_enabled);else if(!strcmp(k,"gamepad.device_path"))snprintf(v->cfg.gamepad_device,sizeof(v->cfg.gamepad_device),"%s",val);else if(!strcmp(k,"gamepad.deadzone")){v->cfg.deadzone=(float)atof(val);if(!isfinite(v->cfg.deadzone))v->cfg.deadzone=0.20f;for(int i=0;i<AVP_JS_MAX_AXES;i++)v->cfg.axis_deadzone[i]=v->cfg.deadzone;}
else if(!strncmp(k,"gamepad.axis_deadzone_",22)){int i=atoi(k+22);if(i>=0&&i<AVP_JS_MAX_AXES)v->cfg.axis_deadzone[i]=(float)atof(val);}
else if(!strcmp(k,"misc.hud_fixed_hue"))v->cfg.hud_fixed_hue=parse_bool(val,v->cfg.hud_fixed_hue);
else if(!strcmp(k,"misc.marine_start_shotgun"))v->cfg.marine_start_shotgun=parse_bool(val,v->cfg.marine_start_shotgun);else if(!strcmp(k,"enhancement.marine_pulse_los_hits"))v->cfg.marine_pulse_los_hits=parse_bool(val,v->cfg.marine_pulse_los_hits);else if(!strcmp(k,"enhancement.marine_fire_interrupt"))v->cfg.marine_fire_interrupt=parse_bool(val,v->cfg.marine_fire_interrupt);else if(!strcmp(k,"enhancement.normalize_diagonal_contact_range"))v->cfg.normalize_diagonal_contact_range=parse_bool(val,v->cfg.normalize_diagonal_contact_range);else if(!strcmp(k,"enhancement.enemy_activation_distance"))v->cfg.enemy_activation_distance=parse_distance_mode(val,v->cfg.enemy_activation_distance,5);else if(!strcmp(k,"enhancement.buffer_short_presses"))v->cfg.buffer_short_presses=parse_bool(val,v->cfg.buffer_short_presses);else if(!strcmp(k,"enhancement.buffer_short_analog"))v->cfg.buffer_short_analog=parse_bool(val,v->cfg.buffer_short_analog);else if(!strcmp(k,"enhancement.remember_automap"))v->cfg.remember_automap=parse_bool(val,v->cfg.remember_automap);else if(!strcmp(k,"gamepad.move_axis"))v->cfg.move_axis=atoi(val);else if(!strcmp(k,"gamepad.move_invert"))v->cfg.move_invert=parse_bool(val,v->cfg.move_invert);else if(!strcmp(k,"gamepad.strafe_axis"))v->cfg.strafe_axis=atoi(val);else if(!strcmp(k,"gamepad.strafe_invert"))v->cfg.strafe_invert=parse_bool(val,v->cfg.strafe_invert);else if(!strcmp(k,"gamepad.turn_axis"))v->cfg.turn_axis=atoi(val);else if(!strcmp(k,"gamepad.turn_invert"))v->cfg.turn_invert=parse_bool(val,v->cfg.turn_invert);else if(!strcmp(k,"gamepad.fire_button"))v->cfg.fire_button=atoi(val);else if(!strcmp(k,"gamepad.use_button"))v->cfg.use_button=atoi(val);else if(!strcmp(k,"gamepad.noclip_button"))v->cfg.noclip_button=atoi(val);else if(!strcmp(k,"gamepad.map_button"))v->cfg.map_button=atoi(val);else if(!strcmp(k,"gamepad.forward_button"))v->cfg.forward_button=atoi(val);else if(!strcmp(k,"gamepad.backward_button"))v->cfg.backward_button=atoi(val);else if(!strcmp(k,"gamepad.strafe_left_button"))v->cfg.strafe_left_button=atoi(val);else if(!strcmp(k,"gamepad.strafe_right_button"))v->cfg.strafe_right_button=atoi(val);else if(!strcmp(k,"gamepad.predator_medkit_button"))v->cfg.predator_medkit_button=atoi(val);else if(!strcmp(k,"gamepad.predator_cloak_button"))v->cfg.predator_cloak_button=atoi(val);else if(!strcmp(k,"gamepad.predator_vision_button"))v->cfg.predator_vision_button=atoi(val);else if(!strcmp(k,"keyboard.forward"))v->cfg.key_forward=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.back"))v->cfg.key_back=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.turn_left"))v->cfg.key_turn_left=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.turn_right"))v->cfg.key_turn_right=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.strafe_left"))v->cfg.key_strafe_left=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.strafe_right"))v->cfg.key_strafe_right=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.fire"))v->cfg.key_fire=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.use"))v->cfg.key_use=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.map"))v->cfg.key_map=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.noclip"))v->cfg.key_noclip=strtoul(val,NULL,0);else if(!strcmp(k,"menu.key_up"))v->cfg.menu_key_up=strtoul(val,NULL,0);
else if(!strcmp(k,"menu.key_down"))v->cfg.menu_key_down=strtoul(val,NULL,0);
else if(!strcmp(k,"menu.key_left"))v->cfg.menu_key_left=strtoul(val,NULL,0);
else if(!strcmp(k,"menu.key_right"))v->cfg.menu_key_right=strtoul(val,NULL,0);
else if(!strcmp(k,"menu.key_open"))v->cfg.menu_key_open=strtoul(val,NULL,0);
else if(!strcmp(k,"menu.button_open"))v->cfg.menu_button_open=atoi(val);
else if(!strcmp(k,"menu.key_accept"))v->cfg.menu_key_accept=strtoul(val,NULL,0);
else if(!strcmp(k,"menu.key_back"))v->cfg.menu_key_back=strtoul(val,NULL,0);
else if(!strcmp(k,"menu.button_up"))v->cfg.menu_button_up=atoi(val);
else if(!strcmp(k,"menu.button_down"))v->cfg.menu_button_down=atoi(val);
else if(!strcmp(k,"menu.button_left"))v->cfg.menu_button_left=atoi(val);
else if(!strcmp(k,"menu.button_right"))v->cfg.menu_button_right=atoi(val);
else if(!strcmp(k,"menu.button_accept"))v->cfg.menu_button_accept=atoi(val);
else if(!strcmp(k,"menu.button_back"))v->cfg.menu_button_back=atoi(val);
else if(!strncmp(k,"gamepad.quick_turn_button_",26)){int i=atoi(k+26);if(i>=0&&i<AVP_QUICK_TURN_SLOTS)v->cfg.quick_turn_buttons[i]=atoi(val);}
else if(!strncmp(k,"keyboard.quick_turn_",20)){int i=atoi(k+20);if(i>=0&&i<AVP_QUICK_TURN_SLOTS)v->cfg.quick_turn_keys[i]=strtoul(val,NULL,0);}
else if(!strcmp(k,"gamepad.pause_button"))v->cfg.pc_pause_button=atoi(val);
else if(!strcmp(k,"gamepad.option_button"))v->cfg.pc_option_button=atoi(val);
else if(!strcmp(k,"keyboard.pause"))v->cfg.key_pause=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.option"))v->cfg.key_option=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.predator_medkit"))v->cfg.key_predator_medkit=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.predator_cloak"))v->cfg.key_predator_cloak=strtoul(val,NULL,0);else if(!strcmp(k,"keyboard.predator_vision"))v->cfg.key_predator_vision=strtoul(val,NULL,0);else if(!strcmp(k,"developer.full_cpu_systems"))v->cfg.full_cpu_systems=parse_bool(val,v->cfg.full_cpu_systems);else if(!strcmp(k,"debug_mode_immediate_in_game")||!strcmp(k,"debug_mode_immediate_in-game"))v->cfg.debug_immediate_in_game=parse_bool(val,v->cfg.debug_immediate_in_game);
else if(!strncmp(k,"action.key_",11)){int a=atoi(k+11);if(a>=0&&a<8)v->cfg.action_key[a]=strtoul(val,NULL,0);}
else if(!strncmp(k,"action.button_",14)){int a=atoi(k+14);if(a>=0&&a<8)v->cfg.action_button[a]=atoi(val);}
else if(!strcmp(k,"controller_mode"))v->cfg.controller_mode_jaguar=!strcasecmp(val,"jaguar")||!strcmp(val,"1");
else if(!strcmp(k,"jaguar.dpad_x_axis"))v->cfg.jag_dpad_x_axis=atoi(val);
else if(!strcmp(k,"jaguar.dpad_y_axis"))v->cfg.jag_dpad_y_axis=atoi(val);
else if(!strcmp(k,"jaguar.dpad_x_invert"))v->cfg.jag_dpad_x_invert=parse_bool(val,v->cfg.jag_dpad_x_invert);
else if(!strcmp(k,"jaguar.dpad_y_invert"))v->cfg.jag_dpad_y_invert=parse_bool(val,v->cfg.jag_dpad_y_invert);
else if(!strcmp(k,"jaguar.up_button"))v->cfg.jag_up_button=atoi(val);
else if(!strcmp(k,"jaguar.down_button"))v->cfg.jag_down_button=atoi(val);
else if(!strcmp(k,"jaguar.left_button"))v->cfg.jag_left_button=atoi(val);
else if(!strcmp(k,"jaguar.right_button"))v->cfg.jag_right_button=atoi(val);
else if(!strcmp(k,"jaguar.a_button"))v->cfg.jag_a_button=atoi(val);
else if(!strcmp(k,"jaguar.b_button"))v->cfg.jag_b_button=atoi(val);
else if(!strcmp(k,"jaguar.c_button"))v->cfg.jag_c_button=atoi(val);
else if(!strcmp(k,"jaguar.pause_button"))v->cfg.jag_pause_button=atoi(val);
else if(!strcmp(k,"jaguar.option_button"))v->cfg.jag_option_button=atoi(val);
else {int ji=-1;if(sscanf(k,"jaguar.keypad_%d_button",&ji)==1&&ji>=0&&ji<=9)v->cfg.jag_keypad_button[ji]=atoi(val);else if(!strcmp(k,"jaguar.keypad_star_button"))v->cfg.jag_keypad_button[10]=atoi(val);else if(!strcmp(k,"jaguar.keypad_hash_button"))v->cfg.jag_keypad_button[11]=atoi(val);}}fclose(f);if(!isfinite(v->cfg.deadzone))v->cfg.deadzone=0.20f;if(v->cfg.deadzone<0.0f)v->cfg.deadzone=0.0f;if(v->cfg.deadzone>0.95f)v->cfg.deadzone=0.95f;
for(int i=0;i<AVP_JS_MAX_AXES;i++){
    if(!isfinite(v->cfg.axis_deadzone[i])||v->cfg.axis_deadzone[i]<0.0f)v->cfg.axis_deadzone[i]=0.0f;
    if(v->cfg.axis_deadzone[i]>0.95f)v->cfg.axis_deadzone[i]=0.95f;
}
#ifdef AVP_LOCAL_MULTIPLAYER
    avp_local_mp_validate(&v->local_mp);
    avp_mp_input_validate(&v->local_mp_controls);
#endif
if(!isfinite(v->cfg.turn_sensitivity))v->cfg.turn_sensitivity=1.0f;
if(v->cfg.turn_sensitivity<0.25f)v->cfg.turn_sensitivity=0.25f;
if(v->cfg.turn_sensitivity>5.0f)v->cfg.turn_sensitivity=5.0f;
if(!isfinite(v->cfg.turn_speed))v->cfg.turn_speed=1.0f;
if(v->cfg.turn_speed<0.25f)v->cfg.turn_speed=0.25f;
if(v->cfg.turn_speed>5.0f)v->cfg.turn_speed=5.0f;
if(!isfinite(v->cfg.mouse_sensitivity)||v->cfg.mouse_sensitivity<0.1f)v->cfg.mouse_sensitivity=0.1f;
if(v->cfg.mouse_sensitivity>5.0f)v->cfg.mouse_sensitivity=5.0f;
if(!isfinite(v->cfg.mouse_acceleration)||v->cfg.mouse_acceleration<0.0f)v->cfg.mouse_acceleration=0.0f;
if(v->cfg.mouse_acceleration>3.0f)v->cfg.mouse_acceleration=3.0f;
for(int i=0;i<AVP_MOUSE_ACTIONS;i++)if(v->cfg.mouse_button[i]<1||v->cfg.mouse_button[i]>15)v->cfg.mouse_button[i]=-1;
/* Number keys 1-4 are retail weapon shortcuts, even if an older settings
 * file accidentally assigns the map action to the same keys. */
if(v->cfg.key_map>=XK_1&&v->cfg.key_map<=XK_4)v->cfg.key_map=XK_Tab;}
static void save_settings(const AvpPcVideo*v){
    FILE*f=fopen(v->settings_path,"w");if(!f)return;
    fprintf(f,"# AVP Jaguar PC port host options\n# controller_mode=jaguar exposes a virtual Atari Jaguar pad to the retail code.\n# controller_mode=pc retains the modern/custom action mapping.\n\n");
    fprintf(f,"controller_mode=%s\n",v->cfg.controller_mode_jaguar?"jaguar":"pc");
#ifdef AVP_LOCAL_MULTIPLAYER
    fprintf(f,"local_multiplayer.enabled=%s\n",v->local_mp.enabled?"true":"false");
    fprintf(f,"local_multiplayer.game_mode=%u\nlocal_multiplayer.dm_score_limit=%u\nlocal_multiplayer.dm_time_limit=%u\nlocal_multiplayer.dm_fast_movement=%s\n", 
        v->local_mp.game_mode,v->local_mp.dm_score_limit,v->local_mp.dm_time_limit,
        v->local_mp.dm_fast_movement?"true":"false");
    fprintf(f,"local_multiplayer.dm_fast_equip=%s\nlocal_multiplayer.dm_enemies=%s\nlocal_multiplayer.dm_level=%u\nlocal_multiplayer.dm_level_travel=%s\n",
        v->local_mp.dm_fast_equip?"true":"false",v->local_mp.dm_enemies?"true":"false",v->local_mp.dm_level,
        v->local_mp.dm_level_travel?"true":"false");
    fprintf(f,"local_multiplayer.dm_cloak_damage=%s\n",v->local_mp.dm_cloak_damage?"true":"false");
    fprintf(f,"local_multiplayer.show_other_players_on_automap=%s\n",
        v->local_mp.show_other_players_on_automap?"true":"false");
    fprintf(f,"local_multiplayer.bot_count=%u\nlocal_multiplayer.random_species_on_respawn=%s\nlocal_multiplayer.show_whole_map=%s\n",
        v->local_mp.bot_count,v->local_mp.random_species_on_respawn?"true":"false",v->local_mp.show_whole_map?"true":"false");
    for(unsigned bi=0;bi<AVP_LOCAL_MP_MAX_BOTS;bi++)
        fprintf(f,"local_multiplayer.bot_%u.species=%u\nlocal_multiplayer.bot_%u.behavior=%u\n",
            bi,v->local_mp.bot_species[bi],bi,v->local_mp.bot_behavior[bi]);
    fprintf(f,"local_multiplayer.players=%u\nlocal_multiplayer.campaign=%u\nlocal_multiplayer.layout=%u\n",v->local_mp.players,v->local_mp.campaign,v->local_mp.layout);
    fprintf(f,"local_multiplayer.respawn_campaign_start=%s\n",v->local_mp.respawn_campaign_start?"true":"false");
    fprintf(f,"local_multiplayer.three_layout=%u\n",v->local_mp.three_layout);
    for(unsigned mp_i=0;mp_i<AVP_LOCAL_MP_MAX_PLAYERS;mp_i++)
        {
            fprintf(f,"local_multiplayer.player_%u.species=%u\n",mp_i,v->local_mp.species[mp_i]);
            fprintf(f,"local_multiplayer.player_%u.bot_enabled=%s\n",mp_i,
                    v->local_mp.bot_enabled[mp_i]?"true":"false");
        }
    avp_mp_input_write_settings(&v->local_mp_controls,f);
#endif
    fprintf(f,"video.aspect=%u\n",v->cfg.video_aspect);
    for(int i=0;i<AVP_MM_ACTIONS;i++)
        fprintf(f,"map_maker.key_%d=0x%lx\nmap_maker.mouse_%d=%d\nmap_maker.button_%d=%d\n",
            i,v->cfg.mm_key[i],i,v->cfg.mm_mouse[i],i,v->cfg.mm_button[i]);
    fprintf(f,"video.window_mode=%u\nvideo.window_width=%u\nvideo.window_height=%u\nvideo.refresh=%u\nvideo.presentation_hz=%u\n",v->cfg.video_window_mode,v->cfg.video_window_width,v->cfg.video_window_height,v->cfg.video_refresh,v->cfg.presentation_hz);
    fprintf(f,"video.show_fps=%s\nvideo.vsync=%s\nvideo.disable_fog=%s\n",v->cfg.show_fps?"true":"false",v->cfg.video_vsync?"true":"false",v->cfg.disable_fog?"true":"false");
    fprintf(f,"audio.speaker_mode=%u\naudio.subwoofer_channels=%u\n"
        "audio.music_volume=%u\naudio.sfx_volume=%u\naudio.ambience_volume=%u\n"
        "audio.voice_volume=%u\naudio.ambient_hissing_noise_volume=%u\n"
        "audio.music=%s\naudio.ambience=%s\naudio.ambient_hissing_noises=%s\n",
        v->cfg.audio_speaker_mode,v->cfg.audio_subwoofers,
        v->cfg.audio_music_volume,v->cfg.audio_sfx_volume,
        v->cfg.audio_ambience_volume,v->cfg.audio_voice_volume,
        v->cfg.audio_hissing_volume,
        v->cfg.audio_music_on?"true":"false",v->cfg.audio_ambience_on?"true":"false",
        v->cfg.audio_hissing_on?"true":"false");
    fprintf(f,"enhancement.game_micro_optimization=%s\n",v->cfg.game_micro_optimization?"true":"false");
    fprintf(f,"enhancement.high_resolution_world=%s\n",v->cfg.high_resolution_world?"true":"false");
    fprintf(f,"enhancement.extend_draw_distance=%d\n",v->cfg.extend_draw_distance);
    fprintf(f,"enhancement.unlimited_shooting_range=%s\n",v->cfg.unlimited_shooting_range?"true":"false");
    fprintf(f,"enhancement.extend_fog_distance=%d\n",v->cfg.extend_fog_distance);
    fprintf(f,"enhancement.prototype_texture_restoration=%s\n",v->cfg.prototype_texture_restoration?"true":"false");
    fprintf(f,"enhancement.buffer_short_presses=%s\n",v->cfg.buffer_short_presses?"true":"false");
    fprintf(f,"enhancement.buffer_short_analog=%s\n",v->cfg.buffer_short_analog?"true":"false");
    fprintf(f,"enhancement.remember_automap=%s\n",v->cfg.remember_automap?"true":"false");
    fprintf(f,"enhancement.marine_pulse_los_hits=%s\n",v->cfg.marine_pulse_los_hits?"true":"false");
    fprintf(f,"enhancement.marine_fire_interrupt=%s\n",v->cfg.marine_fire_interrupt?"true":"false");
    fprintf(f,"enhancement.normalize_diagonal_contact_range=%s\n",v->cfg.normalize_diagonal_contact_range?"true":"false");
    fprintf(f,"enhancement.enemy_activation_distance=%d\n",v->cfg.enemy_activation_distance);
    fprintf(f,"misc.marine_start_shotgun=%s\n",v->cfg.marine_start_shotgun?"true":"false");
    fprintf(f,"misc.hud_fixed_hue=%s\n",v->cfg.hud_fixed_hue?"true":"false");
    fprintf(f,"enhancement.enhanced_analog_controls=%s\n",v->cfg.enhanced_analog_controls?"true":"false");
    fprintf(f,"audio.player_pain_sounds=%s\n",v->cfg.player_pain_sounds_enabled?"true":"false");
    fprintf(f,"audio.facehug_pain_sounds=%s\naudio.facehug_shake_pain_sounds=%s\n",v->cfg.facehug_pain_sounds_enabled?"true":"false",v->cfg.facehug_shake_pain_sounds_enabled?"true":"false");
    fprintf(f,"audio.motion_tracker_sounds=%s\ncontrols.analog_movement=%s\ncontrols.turn_sensitivity=%.2f\ncontrols.turn_speed=%.2f\n",v->cfg.tracker_sounds_enabled?"true":"false",v->cfg.analog_movement?"true":"false",v->cfg.turn_sensitivity,v->cfg.turn_speed);
    fprintf(f,"mouse.enabled=%s\nmouse.sensitivity=%.2f\nmouse.acceleration=%.2f\n",v->cfg.mouse_enabled?"true":"false",v->cfg.mouse_sensitivity,v->cfg.mouse_acceleration);
    for(int i=0;i<AVP_MOUSE_ACTIONS;i++)fprintf(f,"mouse.button_%d=%d\n",i,v->cfg.mouse_button[i]);
    fprintf(f,"enhancement.smooth_presentation=%s\ngamepad.enabled=%s\ngamepad.device_path=%s\ngamepad.deadzone=%.2f\n",v->cfg.smooth_presentation?"true":"false",v->cfg.gamepad_enabled?"true":"false",v->cfg.gamepad_device,v->cfg.deadzone);
    for(int i=0;i<AVP_JS_MAX_AXES;i++)fprintf(f,"gamepad.axis_deadzone_%d=%.2f\n",i,v->cfg.axis_deadzone[i]);
    fprintf(f,"jaguar.dpad_x_axis=%d\njaguar.dpad_y_axis=%d\njaguar.dpad_x_invert=%s\njaguar.dpad_y_invert=%s\n",v->cfg.jag_dpad_x_axis,v->cfg.jag_dpad_y_axis,v->cfg.jag_dpad_x_invert?"true":"false",v->cfg.jag_dpad_y_invert?"true":"false");
    fprintf(f,"jaguar.up_button=%d\njaguar.down_button=%d\njaguar.left_button=%d\njaguar.right_button=%d\n",v->cfg.jag_up_button,v->cfg.jag_down_button,v->cfg.jag_left_button,v->cfg.jag_right_button);
    fprintf(f,"jaguar.a_button=%d\njaguar.b_button=%d\njaguar.c_button=%d\njaguar.pause_button=%d\njaguar.option_button=%d\n",v->cfg.jag_a_button,v->cfg.jag_b_button,v->cfg.jag_c_button,v->cfg.jag_pause_button,v->cfg.jag_option_button);
    for(int i=0;i<10;i++)fprintf(f,"jaguar.keypad_%d_button=%d\n",i,v->cfg.jag_keypad_button[i]);
    fprintf(f,"jaguar.keypad_star_button=%d\njaguar.keypad_hash_button=%d\n",v->cfg.jag_keypad_button[10],v->cfg.jag_keypad_button[11]);
    fprintf(f,"gamepad.pause_button=%d\ngamepad.option_button=%d\n",v->cfg.pc_pause_button,v->cfg.pc_option_button);
    for(int i=0;i<AVP_QUICK_TURN_SLOTS;i++)fprintf(f,"keyboard.quick_turn_%d=0x%lx\ngamepad.quick_turn_button_%d=%d\n",i,v->cfg.quick_turn_keys[i],i,v->cfg.quick_turn_buttons[i]);
    fprintf(f,"gamepad.move_axis=%d\ngamepad.move_invert=%s\ngamepad.strafe_axis=%d\ngamepad.strafe_invert=%s\ngamepad.turn_axis=%d\ngamepad.turn_invert=%s\ngamepad.fire_button=%d\ngamepad.use_button=%d\ngamepad.noclip_button=%d\ngamepad.map_button=%d\n",v->cfg.move_axis,v->cfg.move_invert?"true":"false",v->cfg.strafe_axis,v->cfg.strafe_invert?"true":"false",v->cfg.turn_axis,v->cfg.turn_invert?"true":"false",v->cfg.fire_button,v->cfg.use_button,v->cfg.noclip_button,v->cfg.map_button);
    fprintf(f,"keyboard.forward=0x%lx\nkeyboard.back=0x%lx\nkeyboard.turn_left=0x%lx\nkeyboard.turn_right=0x%lx\nkeyboard.strafe_left=0x%lx\nkeyboard.strafe_right=0x%lx\nkeyboard.fire=0x%lx\nkeyboard.use=0x%lx\nkeyboard.map=0x%lx\nkeyboard.noclip=0x%lx\n",v->cfg.key_forward,v->cfg.key_back,v->cfg.key_turn_left,v->cfg.key_turn_right,v->cfg.key_strafe_left,v->cfg.key_strafe_right,v->cfg.key_fire,v->cfg.key_use,v->cfg.key_map,v->cfg.key_noclip);
    fprintf(f,"# Menu key values are X11 KeySym numbers (0xFF52=Up, 0xFF0D=Return).\n");
    fprintf(f,"menu.key_up=0x%lx\nmenu.key_down=0x%lx\nmenu.key_left=0x%lx\nmenu.key_right=0x%lx\nmenu.key_accept=0x%lx\nmenu.key_back=0x%lx\nmenu.key_open=0x%lx\n",v->cfg.menu_key_up,v->cfg.menu_key_down,v->cfg.menu_key_left,v->cfg.menu_key_right,v->cfg.menu_key_accept,v->cfg.menu_key_back,v->cfg.menu_key_open);
    fprintf(f,"menu.button_open=%d\n",v->cfg.menu_button_open);
    fprintf(f,"menu.button_up=%d\nmenu.button_down=%d\nmenu.button_left=%d\nmenu.button_right=%d\nmenu.button_accept=%d\nmenu.button_back=%d\n",
        v->cfg.menu_button_up,v->cfg.menu_button_down,v->cfg.menu_button_left,
        v->cfg.menu_button_right,v->cfg.menu_button_accept,v->cfg.menu_button_back);
    fprintf(f,"gamepad.forward_button=%d\ngamepad.backward_button=%d\n",v->cfg.forward_button,v->cfg.backward_button);
    fprintf(f,"gamepad.strafe_left_button=%d\ngamepad.strafe_right_button=%d\ngamepad.predator_medkit_button=%d\ngamepad.predator_cloak_button=%d\ngamepad.predator_vision_button=%d\n",v->cfg.strafe_left_button,v->cfg.strafe_right_button,v->cfg.predator_medkit_button,v->cfg.predator_cloak_button,v->cfg.predator_vision_button);
    fprintf(f,"keyboard.pause=0x%lx\nkeyboard.option=0x%lx\nkeyboard.predator_medkit=0x%lx\nkeyboard.predator_cloak=0x%lx\nkeyboard.predator_vision=0x%lx\n",v->cfg.key_pause,v->cfg.key_option,v->cfg.key_predator_medkit,v->cfg.key_predator_cloak,v->cfg.key_predator_vision);
    for(int i=0;i<8;i++)fprintf(f,"action.key_%d=0x%lx\naction.button_%d=%d\n",i,v->cfg.action_key[i],i,v->cfg.action_button[i]);
    fprintf(f,"developer.full_cpu_systems=%s\ndebug_mode_immediate_in_game=%d\n",v->cfg.full_cpu_systems?"true":"false",v->cfg.debug_immediate_in_game?1:0);fclose(f);
}
#ifndef _WIN32
/* Raw Linux joystick events may contain both edges between host polls.
 * Preserve the real press once when the optional buffer is enabled.  XInput
 * supplies state snapshots rather than an event stream, and stays unchanged. */
static void short_joystick_press(AvpPcVideo *v,int button)
{
    u32 bits=0;
    if(!v->cfg.buffer_short_presses||!v->mouse_focused||v->menu_open||v->frontend_input||
       v->capture_mode||avp_computer_active()||
       avp_jaguar_pause_op_active(&v->gpu.pause_op))return;
#define ADD_BUTTON(binding,bit) do{if((binding)==button)bits|=1u<<(bit);}while(0)
    if(v->cfg.controller_mode_jaguar && button!=v->cfg.map_button){
        ADD_BUTTON(v->cfg.jag_up_button,JOY_UP);
        ADD_BUTTON(v->cfg.jag_down_button,JOY_DOWN);
        ADD_BUTTON(v->cfg.jag_left_button,JOY_LEFT);
        ADD_BUTTON(v->cfg.jag_right_button,JOY_RIGHT);
        ADD_BUTTON(v->cfg.jag_a_button,FIRE_A);
        ADD_BUTTON(v->cfg.jag_b_button,FIRE_B);
        ADD_BUTTON(v->cfg.jag_c_button,FIRE_C);
        ADD_BUTTON(v->cfg.jag_pause_button,PAUSE);
        ADD_BUTTON(v->cfg.jag_option_button,OPTION);
        {static const unsigned keypad[12]={KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,
            KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,KEY_STAR,KEY_HASH};
         for(int i=0;i<12;i++)ADD_BUTTON(v->cfg.jag_keypad_button[i],keypad[i]);}
    }else if(!v->cfg.controller_mode_jaguar){
        ADD_BUTTON(v->cfg.fire_button,FIRE_A);
        if(player_type!=PT_ALIEN)ADD_BUTTON(v->cfg.use_button,FIRE_B);
        else if(v->cfg.use_button==button)v->use_button_buffered=1;
        ADD_BUTTON(v->cfg.pc_pause_button,PAUSE);
        ADD_BUTTON(v->cfg.pc_option_button,OPTION);
        if(player_type==PT_ALIEN){
            static const unsigned attack[3]={FIRE_A,FIRE_B,FIRE_C};
            for(int i=0;i<3;i++)ADD_BUTTON(v->cfg.action_button[i+2],attack[i]);
        }
        if(player_type==PT_PREDATOR){
            ADD_BUTTON(v->cfg.predator_medkit_button,KEY_7);
            ADD_BUTTON(v->cfg.predator_cloak_button,OPTION);
            ADD_BUTTON(v->cfg.predator_vision_button,KEY_6);
        }
    }
    ADD_BUTTON(v->cfg.map_button,KEY_8);
    ADD_BUTTON(v->cfg.strafe_left_button,JOY_LEFT);
    ADD_BUTTON(v->cfg.strafe_right_button,JOY_RIGHT);
    ADD_BUTTON(v->cfg.forward_button,JOY_UP);
    ADD_BUTTON(v->cfg.backward_button,JOY_DOWN);
#undef ADD_BUTTON
    v->pad_buffered|=bits;
}
#endif
/* Extra input is never routed into the Jaguar virtual pad or retail logic. */
static int quick_turn_allowed(const AvpPcVideo *v)
{
    /* FocusOut clears queued turns, but live joystick events may still arrive
     * while the window is unfocused. Reject those new host-only actions too. */
    return v && v->mouse_focused && !v->menu_open && !v->frontend_input && !v->capture_mode &&
           !avp_computer_active() && !avp_jaguar_pause_op_active(&v->gpu.pause_op);
}
static void queue_quick_turn(AvpPcVideo *v)
{
    if(quick_turn_allowed(v) && v->quick_turn_pending<4u)v->quick_turn_pending++;
}
static int quick_turn_key_matches(const AvpPcVideo *v,KeySym k)
{
    for(int i=0;i<AVP_QUICK_TURN_SLOTS;i++)
        if(v->cfg.quick_turn_keys[i] && v->cfg.quick_turn_keys[i]==(unsigned long)k)
            return 1;
    return 0;
}
#ifndef _WIN32 /* Linux joystick events; XInput polls via avp_pc_binding_edge */
static int quick_turn_button_matches(const AvpPcVideo *v,int token)
{
    for(int i=0;i<AVP_QUICK_TURN_SLOTS;i++)
        if(v->cfg.quick_turn_buttons[i]==token)return 1;
    return 0;
}
/* Only a newly pressed axis direction counts. A held trigger cannot retrigger. */
static void quick_turn_axis_event(AvpPcVideo *v,int axis,int16_t before,int16_t after)
{
    if((before>-19660 && after<=-19660 &&
        quick_turn_button_matches(v,avp_pc_axis_button_token(axis,0))) ||
       (before<19660 && after>=19660 &&
        quick_turn_button_matches(v,avp_pc_axis_button_token(axis,1))))
        queue_quick_turn(v);
}
#endif
#ifdef _WIN32
/* XInput presents the same axis/button state as the Linux joystick path so
   all existing per-action bindings and axis-as-button tokens are shared. */
static void close_js(AvpPcVideo*v){
    v->js_fd=-1;
    v->js_path[0]=0;
    v->js_name[0]=0;
    v->js_axes_count=v->js_buttons_count=0;
    memset(v->js_axes,0,sizeof(v->js_axes));
    memset(v->js_prev_axes,0,sizeof(v->js_prev_axes));
    memset(v->js_buttons,0,sizeof(v->js_buttons));
    memset(v->js_prev_buttons,0,sizeof(v->js_prev_buttons));
}
static void enumerate_js(AvpPcVideo*v){int i;v->device_count=0;for(i=0;i<4;i++){XINPUT_STATE s;char p[64];if(XInputGetState((DWORD)i,&s)!=ERROR_SUCCESS)continue;snprintf(p,sizeof(p),"xinput:%d",i);snprintf(v->devices[v->device_count].path,sizeof(v->devices[0].path),"%s",p);snprintf(v->devices[v->device_count].name,sizeof(v->devices[0].name),"XInput Controller %d",i+1);v->devices[v->device_count].axes=6;v->devices[v->device_count].buttons=16;v->device_count++;}}
static int open_js_path(AvpPcVideo*v,const char*path){int i;XINPUT_STATE state;if(sscanf(path,"xinput:%d",&i)!=1||i<0||i>=4||XInputGetState((DWORD)i,&state)!=ERROR_SUCCESS)return 0;close_js(v);v->js_fd=i+1;v->js_axes_count=6;v->js_buttons_count=16;snprintf(v->js_path,sizeof(v->js_path),"xinput:%d",i);snprintf(v->js_name,sizeof(v->js_name),"XInput Controller %d",i+1);return 1;}
static void scan_js(AvpPcVideo*v){int i;if(!v->cfg.gamepad_enabled){close_js(v);return;}enumerate_js(v);if(v->js_fd>=0){XINPUT_STATE state;if(XInputGetState((DWORD)(v->js_fd-1),&state)==ERROR_SUCCESS&&(!v->cfg.gamepad_device[0]||!strcmp(v->cfg.gamepad_device,v->js_path)))return;close_js(v);}if(v->cfg.gamepad_device[0]&&open_js_path(v,v->cfg.gamepad_device))return;for(i=0;i<v->device_count;i++)if(open_js_path(v,v->devices[i].path))return;}
static void cycle_js(AvpPcVideo*v,int dir){int i,current=-1;enumerate_js(v);if(v->device_count==0){v->cfg.gamepad_device[0]=0;close_js(v);save_settings(v);return;}for(i=0;i<v->device_count;i++)if(!strcmp(v->cfg.gamepad_device,v->devices[i].path)){current=i;break;}if(current<0)current=dir>0?-1:0;current=(current+(dir>0?1:-1)+v->device_count)%v->device_count;snprintf(v->cfg.gamepad_device,sizeof(v->cfg.gamepad_device),"%s",v->devices[current].path);close_js(v);scan_js(v);save_settings(v);}
static void poll_js(AvpPcVideo*v){XINPUT_STATE s;WORD b;static const WORD masks[]={XINPUT_GAMEPAD_A,XINPUT_GAMEPAD_B,XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_Y,XINPUT_GAMEPAD_LEFT_SHOULDER,XINPUT_GAMEPAD_RIGHT_SHOULDER,XINPUT_GAMEPAD_BACK,XINPUT_GAMEPAD_START,XINPUT_GAMEPAD_LEFT_THUMB,XINPUT_GAMEPAD_RIGHT_THUMB,XINPUT_GAMEPAD_DPAD_UP,XINPUT_GAMEPAD_DPAD_DOWN,XINPUT_GAMEPAD_DPAD_LEFT,XINPUT_GAMEPAD_DPAD_RIGHT};int i;if(!v->cfg.gamepad_enabled){close_js(v);return;}if(v->js_fd<0)scan_js(v);if(v->js_fd<0)return;memcpy(v->js_prev_axes,v->js_axes,sizeof(v->js_axes));memcpy(v->js_prev_buttons,v->js_buttons,sizeof(v->js_buttons));if(XInputGetState((DWORD)(v->js_fd-1),&s)!=ERROR_SUCCESS){close_js(v);return;}v->js_axes[0]=s.Gamepad.sThumbLX;v->js_axes[1]=(s.Gamepad.sThumbLY==-32768)?32767:(int16_t)-s.Gamepad.sThumbLY;v->js_axes[2]=s.Gamepad.sThumbRX;v->js_axes[3]=(s.Gamepad.sThumbRY==-32768)?32767:(int16_t)-s.Gamepad.sThumbRY;v->js_axes[4]=(int16_t)(s.Gamepad.bRightTrigger*128);v->js_axes[5]=(int16_t)(s.Gamepad.bLeftTrigger*128);b=s.Gamepad.wButtons;for(i=0;i<14;i++)v->js_buttons[i]=(b&masks[i])!=0;v->js_buttons[14]=s.Gamepad.bLeftTrigger>=160;v->js_buttons[15]=s.Gamepad.bRightTrigger>=160;
    for(int j=0;j<AVP_QUICK_TURN_SLOTS;j++)
        if(avp_pc_binding_edge(v->cfg.quick_turn_buttons[j],v->js_axes,v->js_prev_axes,
            v->js_axes_count,v->js_buttons,v->js_prev_buttons,v->js_buttons_count)){queue_quick_turn(v);break;}
}
#else
static void close_js(AvpPcVideo*v){if(v->js_fd>=0)close(v->js_fd);v->js_fd=-1;v->js_path[0]=0;v->js_name[0]=0;v->js_axes_count=v->js_buttons_count=0;memset(v->js_axes,0,sizeof(v->js_axes));memset(v->js_buttons,0,sizeof(v->js_buttons));}
static void enumerate_js(AvpPcVideo*v){int i;v->device_count=0;for(i=0;i<AVP_JS_MAX_DEVICES;i++){char p[64],name[128]="Linux joystick";int fd;unsigned char axes=0,buttons=0;snprintf(p,sizeof(p),"/dev/input/js%d",i);fd=open(p,O_RDONLY|O_NONBLOCK);if(fd<0)continue;if(ioctl(fd,JSIOCGNAME(sizeof(name)),name)<0)snprintf(name,sizeof(name),"Linux joystick %d",i);(void)ioctl(fd,JSIOCGAXES,&axes);(void)ioctl(fd,JSIOCGBUTTONS,&buttons);close(fd);snprintf(v->devices[v->device_count].path,sizeof(v->devices[v->device_count].path),"%s",p);snprintf(v->devices[v->device_count].name,sizeof(v->devices[v->device_count].name),"%s",name);v->devices[v->device_count].axes=axes;v->devices[v->device_count].buttons=buttons;if(++v->device_count>=AVP_JS_MAX_DEVICES)break;}}
static int open_js_path(AvpPcVideo*v,const char*path){char name[128]="Linux joystick";unsigned char axes=0,buttons=0;int fd=open(path,O_RDONLY|O_NONBLOCK);if(fd<0)return 0;close_js(v);v->js_fd=fd;snprintf(v->js_path,sizeof(v->js_path),"%s",path);if(ioctl(fd,JSIOCGNAME(sizeof(name)),name)<0)snprintf(name,sizeof(name),"Linux joystick");(void)ioctl(fd,JSIOCGAXES,&axes);(void)ioctl(fd,JSIOCGBUTTONS,&buttons);v->js_axes_count=axes>AVP_JS_MAX_AXES?AVP_JS_MAX_AXES:axes;v->js_buttons_count=buttons>AVP_JS_MAX_BUTTONS?AVP_JS_MAX_BUTTONS:buttons;snprintf(v->js_name,sizeof(v->js_name),"%s",name);return 1;}
static void scan_js(AvpPcVideo*v){int i;if(!v->cfg.gamepad_enabled){close_js(v);return;}enumerate_js(v);if(v->js_fd>=0){if(!v->cfg.gamepad_device[0]||!strcmp(v->cfg.gamepad_device,v->js_path))return;close_js(v);}if(v->cfg.gamepad_device[0]){if(open_js_path(v,v->cfg.gamepad_device))return;}for(i=0;i<v->device_count;i++)if(open_js_path(v,v->devices[i].path))return;}
static void cycle_js(AvpPcVideo*v,int dir){int i,current=-1;enumerate_js(v);if(v->device_count==0){v->cfg.gamepad_device[0]=0;close_js(v);save_settings(v);return;}for(i=0;i<v->device_count;i++)if(!strcmp(v->cfg.gamepad_device,v->devices[i].path)){current=i;break;}if(current<0)current=dir>0?-1:0;current=(current+(dir>0?1:-1)+v->device_count)%v->device_count;snprintf(v->cfg.gamepad_device,sizeof(v->cfg.gamepad_device),"%s",v->devices[current].path);close_js(v);scan_js(v);save_settings(v);}
static void poll_js(AvpPcVideo*v){struct js_event e;if(!v->cfg.gamepad_enabled)return;if(v->js_fd<0)scan_js(v);if(v->js_fd<0)return;memcpy(v->js_prev_axes,v->js_axes,sizeof(v->js_axes));memcpy(v->js_prev_buttons,v->js_buttons,sizeof(v->js_buttons));for(;;){ssize_t n=read(v->js_fd,&e,sizeof(e));if(n==(ssize_t)sizeof(e)){unsigned t=e.type&~JS_EVENT_INIT;if(t==JS_EVENT_AXIS&&e.number<AVP_JS_MAX_AXES){
    if(!(e.type&JS_EVENT_INIT))
        quick_turn_axis_event(v,e.number,v->js_axes[e.number],e.value);
    v->js_axes[e.number]=e.value;
}else if(t==JS_EVENT_BUTTON&&e.number<AVP_JS_MAX_BUTTONS){
    if(!(e.type&JS_EVENT_INIT)&&e.value&&!v->js_buttons[e.number])
        short_joystick_press(v,e.number);
    if(!(e.type&JS_EVENT_INIT)&&e.value&&!v->js_buttons[e.number]&&
       quick_turn_button_matches(v,e.number))queue_quick_turn(v);
    v->js_buttons[e.number]=e.value?1:0;
}continue;}if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;if(n<0&&errno==EINTR)continue;if(n==0||(n<0)){close_js(v);break;}}}
#endif
static float axis_value(const AvpPcVideo*v,int idx,int invert){
    if(idx<0||idx>=v->js_axes_count)return 0.0f;
    return avp_pc_axis_deadzone(v->js_axes[idx],v->cfg.axis_deadzone[idx],invert);
}
static int button_down(const AvpPcVideo*v,int idx){return avp_pc_binding_down(idx,v->js_axes,v->js_axes_count,v->js_buttons,v->js_buttons_count);}
static int button_edge(const AvpPcVideo*v,int idx){return avp_pc_binding_edge(idx,v->js_axes,v->js_prev_axes,v->js_axes_count,v->js_buttons,v->js_prev_buttons,v->js_buttons_count);}
static void button_desc(char*out,size_t n,int token){
    if(token==-1)snprintf(out,n,"Unbound");
    else if(token>=0)snprintf(out,n,"Button %d",token);
    else{
        int64_t a=-(int64_t)token-2;
        if(a<0||a/2>=AVP_JS_MAX_AXES)snprintf(out,n,"Invalid binding");
        else snprintf(out,n,"Axis %d %c",(int)(a/2),(a&1)?'+':'-');
    }
}

static void begin_capture(AvpPcVideo*v,int mode,int target){v->capture_mode=mode;v->capture_target=target;memcpy(v->js_prev_axes,v->js_axes,sizeof(v->js_axes));memcpy(v->js_prev_buttons,v->js_buttons,sizeof(v->js_buttons));}
static void prototype_texture_update(AvpPcVideo *v)
{
    char exe[4096],loaded_from[4096];ssize_t size;
    const char *custom=getenv("AVP_PROTOTYPE_TEXTURE_DIR");
    if(!v)return;
    size=readlink("/proc/self/exe",exe,sizeof(exe)-1u);
    if(size<=0||(size_t)size>=sizeof(exe))exe[0]=0;
    else exe[size]=0;
    (void)avp_pc_prototype_textures_apply_search(&v->prototype_textures,&v->gpu,
        custom,exe,"prototype_texture_restoration",
        v->cfg.prototype_texture_restoration,loaded_from,sizeof(loaded_from));
    if(v->cfg.prototype_texture_restoration){
        if(v->prototype_textures.applied)
            fprintf(stderr,"Prototype textures: loaded %u panels from %s\n",
                    v->prototype_textures.applied,loaded_from);
        else
            fprintf(stderr,"Prototype textures: no valid pack found. "
                    "Place prototype_texture_restoration beside the executable "
                    "or set AVP_PROTOTYPE_TEXTURE_DIR.\n");
    }
}
static int cheat_player_adjust_new(AvpPcVideo *v,int id);
static void cheat_queue_player_action(AvpPcVideo *v,unsigned action);
static void menu_adjust(AvpPcVideo*v,int dir){
    if(v->menu_page==MENU_CHEATS_NEW && cheat_player_adjust_new(v,v->menu_index))return;
    switch(v->menu_index){case 0:v->cfg.smooth_presentation=!v->cfg.smooth_presentation;break;case 1:v->cfg.gamepad_enabled=!v->cfg.gamepad_enabled;if(!v->cfg.gamepad_enabled)close_js(v);else scan_js(v);break;case 2:cycle_js(v,dir);return;case 3:v->cfg.deadzone+=(dir>0?0.05f:-0.05f);if(v->cfg.deadzone<0.0f)v->cfg.deadzone=0.0f;if(v->cfg.deadzone>0.95f)v->cfg.deadzone=0.95f;for(int i=0;i<AVP_JS_MAX_AXES;i++)v->cfg.axis_deadzone[i]=v->cfg.deadzone;break;
    case 147:v->cfg.enhanced_analog_controls=!v->cfg.enhanced_analog_controls;break;
    case 148:v->cfg.buffer_short_analog=!v->cfg.buffer_short_analog;avp_pc_analog_buffer_reset(&v->analog_buffer);break;
    case 157:v->cfg.marine_pulse_los_hits=!v->cfg.marine_pulse_los_hits;break;
    case 158:v->cfg.marine_fire_interrupt=!v->cfg.marine_fire_interrupt;break;
    case 174:v->cfg.normalize_diagonal_contact_range=!v->cfg.normalize_diagonal_contact_range;break;
    case 177:v->cfg.enemy_activation_distance=(v->cfg.enemy_activation_distance+(dir>0?1:4))%5;break;
    case 163:v->cfg.remember_automap=!v->cfg.remember_automap;break;
    case 167:v->cfg.extend_draw_distance=avp_host_distance_cycle(v->cfg.extend_draw_distance,dir,AVP_HOST_DRAW_DISTANCE_CHOICES);break;
    case 170:v->cfg.unlimited_shooting_range=!v->cfg.unlimited_shooting_range;break;
    case 168:v->cfg.extend_fog_distance=avp_host_distance_cycle(v->cfg.extend_fog_distance,dir,AVP_HOST_FOG_DISTANCE_CHOICES);break;
    case 169:v->cfg.prototype_texture_restoration=!v->cfg.prototype_texture_restoration;prototype_texture_update(v);break;
    case 160:v->cfg.marine_start_shotgun=!v->cfg.marine_start_shotgun;break;
    case 171:v->cfg.hud_fixed_hue=!v->cfg.hud_fixed_hue;break;
    case 172:if(avp_game_micro_opt_available()){v->cfg.game_micro_optimization=!v->cfg.game_micro_optimization;avp_game_micro_opt_set(v->cfg.game_micro_optimization);}break;
#ifdef AVP_LOCAL_MULTIPLAYER
    case 159:v->local_mp.enabled=!v->local_mp.enabled;
        if(!v->local_mp.enabled)v->local_mp_open=0;
        break;
#endif
    case 166:
        /* Return to the actual Jaguar title. Do not launch another game or
         * route through the F2 multiplayer setup screen. */
        if(!v->frontend_input){
            v->pending_actions|=AVP_PC_ACTION_RESTART_TITLE;
            v->menu_open=0;
            v->pad_buffered=0;
            v->use_button_buffered=0;
            v->quick_turn_pending=0;
        }
        break;
    case 146:v->cfg.buffer_short_presses=!v->cfg.buffer_short_presses;v->pad_buffered=0;v->use_button_buffered=0;break;case 5:v->cfg.move_invert=!v->cfg.move_invert;break;case 7:v->cfg.strafe_invert=!v->cfg.strafe_invert;break;case 9:v->cfg.turn_invert=!v->cfg.turn_invert;break;case 23:v->species_request=(v->species_request+dir+3)%3;v->species_dirty=1;break;case 24:v->cfg.full_cpu_systems=!v->cfg.full_cpu_systems;break;case 25:v->cfg.controller_mode_jaguar=!v->cfg.controller_mode_jaguar;break;case 48:v->pending_actions|=AVP_PC_ACTION_RESTORE_HEALTH;break;case 49:v->pending_actions|=AVP_PC_ACTION_RESTORE_AMMO;break;case 50:v->infinite_health=!v->infinite_health;break;case 51:v->infinite_ammo=!v->infinite_ammo;break;case 52:v->pending_actions|=AVP_PC_ACTION_GIVE_WEAPONS;break;case 53:v->pending_actions|=AVP_PC_ACTION_QUICKSAVE;break;case 54:v->pending_actions|=AVP_PC_ACTION_QUICKLOAD;break;case 55:v->menu_open=0;break;case 56:v->cfg.high_resolution_world=!v->cfg.high_resolution_world;v->presenting_world_hires=0;break;
    case 123:v->kill_type_choice=(v->kill_type_choice+dir+8)%8;break;
    case 124:v->pending_actions|=AVP_PC_ACTION_KILL_SELECTED;break;
    case 125:v->pending_actions|=AVP_PC_ACTION_KILL_ALL;break;
    case 175:cheat_queue_player_action(v,AVP_PC_ACTION_CLEAR_ENEMIES);break;
    case 176:cheat_queue_player_action(v,AVP_PC_ACTION_CLEAR_HAZARDS);break;
    case 126:v->facehug_attach_disabled=!v->facehug_attach_disabled;break;
    case 127:v->attacks_disabled=!v->attacks_disabled;break;
    case 128:v->cfg.facehug_pain_sounds_enabled=!v->cfg.facehug_pain_sounds_enabled;avp_player_set_facehug_pain_sounds(v->cfg.facehug_pain_sounds_enabled);break;
    case 129:v->cfg.facehug_shake_pain_sounds_enabled=!v->cfg.facehug_shake_pain_sounds_enabled;avp_mazescrn_set_facehug_shake_pain_sounds(v->cfg.facehug_shake_pain_sounds_enabled);break;
    case 57:v->tracker_cheat=!v->tracker_cheat;break;
    case 58:v->pending_actions|=AVP_PC_ACTION_SECURITY_UP;break;
    case 59:v->pending_actions|=AVP_PC_ACTION_SECURITY_DOWN;break;case 76:v->freeze_enemies=!v->freeze_enemies;break;case 77:v->cfg.tracker_sounds_enabled=!v->cfg.tracker_sounds_enabled;break;
    case 110:v->cfg.player_pain_sounds_enabled=!v->cfg.player_pain_sounds_enabled;avp_player_set_pain_sounds(v->cfg.player_pain_sounds_enabled);break;
    case 78:v->cfg.analog_movement=!v->cfg.analog_movement;break;
    case 79:v->cfg.turn_sensitivity+=(dir>0?0.1f:-0.1f);if(v->cfg.turn_sensitivity<0.25f)v->cfg.turn_sensitivity=0.25f;if(v->cfg.turn_sensitivity>5.0f)v->cfg.turn_sensitivity=5.0f;break;
    case 109:v->noclip=!v->noclip;break;
#ifdef AVP_LOCAL_MULTIPLAYER
    case 173:v->local_mp_controls.player[0].cheat_reveal_map=
        !v->local_mp_controls.player[0].cheat_reveal_map;break;
#endif
    case 95:v->cfg.mouse_enabled=!v->cfg.mouse_enabled;break;
    case 96:v->cfg.mouse_sensitivity+=0.1f*(dir>0?1:-1);if(v->cfg.mouse_sensitivity<0.1f)v->cfg.mouse_sensitivity=0.1f;if(v->cfg.mouse_sensitivity>5.0f)v->cfg.mouse_sensitivity=5.0f;break;
    case 97:v->cfg.mouse_acceleration+=0.1f*(dir>0?1:-1);if(v->cfg.mouse_acceleration<0.0f)v->cfg.mouse_acceleration=0.0f;if(v->cfg.mouse_acceleration>3.0f)v->cfg.mouse_acceleration=3.0f;break;
    case 80:v->cfg.turn_speed+=(dir>0?0.1f:-0.1f);if(v->cfg.turn_speed<0.25f)v->cfg.turn_speed=0.25f;if(v->cfg.turn_speed>5.0f)v->cfg.turn_speed=5.0f;break;
    default:
       if(v->menu_index>=130&&v->menu_index<146){int i=v->menu_index-130;
           v->cfg.axis_deadzone[i]+=(dir>0?0.05f:-0.05f);
           if(v->cfg.axis_deadzone[i]<0.0f)v->cfg.axis_deadzone[i]=0.0f;
           if(v->cfg.axis_deadzone[i]>0.95f)v->cfg.axis_deadzone[i]=0.95f;
       }break;}save_settings(v);}

/* IDs are stable for old settings and capture handlers; only the menu tree changes. */
static const int axis_items[]={1,2,3,4,5,6,7,8,9,78,79,80,
    130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145};
static const int pc_gamepad_items[]={10,11,12,47,68,69,70,71,72,73,74,75,83,84,88,89,90,91,92,93,94,153,154,155,156
#ifdef AVP_LOCAL_MULTIPLAYER
    ,164,165 /* P1 co-op map zoom: same settings as Local Players */
#endif
};
static const int jag_gamepad_items[]={26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46
#ifdef AVP_LOCAL_MULTIPLAYER
    ,164,165
#endif
};
static const int frontend_gamepad_items[]={117,118,119,120,121,122,162};
static const int pc_keyboard_items[]={13,14,15,16,17,18,19,20,21,22,60,61,62,63,64,65,66,67,81,82,85,86,87,149,150,151,152};
static const int frontend_keyboard_items[]={111,112,113,114,115,116,161};
static const int cheat_items[]={48,49,50,51,52,57,58,59,76,109,123,124,125,175,176,126,127,173};
/* Only enhancements live here. Stable IDs preserve config and binding behavior. */
static const int enhancement_items[]={0,172,56,167,177,168,169,170,146,147,148,157,158,163,174};
static const int misc_items[]={23,24,53,54,55,77,110,128,129,160,171,166
#ifdef AVP_LOCAL_MULTIPLAYER
,159
#endif
};
#define COUNT(x) ((int)(sizeof(x)/sizeof((x)[0])))
/* Video display controls affect only the host window, never Jaguar ticks. */
static const unsigned video_sizes[][2]={{640,456},{800,600},{960,720},{1024,768},{1280,720},{1366,768},{1600,900},{1920,1080},{2560,1440},{3440,1440},{3840,2160}};
static const unsigned video_size_count=(unsigned)(sizeof(video_sizes)/sizeof(video_sizes[0]));
static unsigned video_size_next(const AvpPcSettings *c,int dir)
{
    unsigned i;
    for(i=0;i<video_size_count;i++)if(video_sizes[i][0]==c->video_window_width && video_sizes[i][1]==c->video_window_height)break;
    if(i==video_size_count)i=0;
    return dir>0?(i+1u)%video_size_count:(i+video_size_count-1u)%video_size_count;
}
static unsigned video_refresh_options(AvpPcVideo *v,unsigned *rates,unsigned cap)
{
    unsigned count=1; /* 0 = leave monitor on desktop refresh */
    if(!cap)return 0;
    rates[0]=0;
#ifdef _WIN32
    {unsigned native[32],n=avp_win32_display_rates(v->cfg.video_window_width,v->cfg.video_window_height,native,32);
     for(unsigned i=0;i<n&&count<cap;i++)if(native[i]>=30u&&native[i]<=360u){
        int duplicate=0;for(unsigned j=0;j<count;j++)if(rates[j]==native[i])duplicate=1;
        if(!duplicate)rates[count++]=native[i];
     }}
#elif defined(AVP_HAVE_XRANDR)
    {XRRScreenConfiguration *sc=XRRGetScreenInfo(v->dpy,RootWindow(v->dpy,v->screen));
     if(sc){int n=0;SizeID size=XRRConfigCurrentConfiguration(sc,NULL);
         short *src=XRRConfigRates(sc,size,&n);
         for(int i=0;i<n&&count<cap;i++)if(src[i]>=30&&src[i]<=360){
            int duplicate=0;for(unsigned j=0;j<count;j++)if(rates[j]==(unsigned)src[i])duplicate=1;
            if(!duplicate)rates[count++]=(unsigned)src[i];
         }
         XRRFreeScreenConfigInfo(sc);
     }}
#else
    (void)v;
#endif
    return count;
}
#ifndef _WIN32
static void video_x11_fullscreen(AvpPcVideo *v,int enable)
{
    Atom wm=XInternAtom(v->dpy,"_NET_WM_STATE",False);
    Atom fs=XInternAtom(v->dpy,"_NET_WM_STATE_FULLSCREEN",False);
    XEvent e;memset(&e,0,sizeof(e));e.xclient.type=ClientMessage;e.xclient.window=v->win;
    e.xclient.message_type=wm;e.xclient.format=32;e.xclient.data.l[0]=enable?1:0;
    e.xclient.data.l[1]=(long)fs;e.xclient.data.l[3]=1;
    (void)XSendEvent(v->dpy,RootWindow(v->dpy,v->screen),False,SubstructureNotifyMask|SubstructureRedirectMask,&e);
}
static void video_x11_decorations(AvpPcVideo *v,int enabled)
{
    /* Standard Motif hint supported by Mutter, KWin, XFWM and most X11 WMs. */
    struct {unsigned long flags,functions,decorations;long input_mode;unsigned long status;} hint;
    Atom atom=XInternAtom(v->dpy,"_MOTIF_WM_HINTS",False);
    memset(&hint,0,sizeof(hint));hint.flags=2;hint.decorations=enabled?1u:0u;
    XChangeProperty(v->dpy,v->win,atom,atom,32,PropModeReplace,(const unsigned char *)&hint,5);
}
#ifdef AVP_HAVE_XRANDR
static void video_x11_restore_refresh(AvpPcVideo *v)
{
    if(v->video_rr_changed){
        XRRScreenConfiguration *sc=XRRGetScreenInfo(v->dpy,RootWindow(v->dpy,v->screen));
        if(sc){(void)XRRSetScreenConfigAndRate(v->dpy,sc,RootWindow(v->dpy,v->screen),v->video_saved_rr_size,v->video_saved_rr_rotation,v->video_saved_rr_rate,CurrentTime);XRRFreeScreenConfigInfo(sc);}
        v->video_rr_changed=0;
    }
}
static void video_x11_apply_refresh(AvpPcVideo *v)
{
    XRRScreenConfiguration *sc;
    Rotation rotation;
    SizeID size;
    int n=0;short *rates;
    if(!v->cfg.video_refresh)return;
    sc=XRRGetScreenInfo(v->dpy,RootWindow(v->dpy,v->screen));if(!sc)return;
    size=XRRConfigCurrentConfiguration(sc,&rotation);
    rates=XRRConfigRates(sc,size,&n);
    for(int i=0;i<n;i++)if(rates[i]==(short)v->cfg.video_refresh){
        short old_rate=XRRConfigCurrentRate(sc);
        if(old_rate!=rates[i] && XRRSetScreenConfigAndRate(v->dpy,sc,RootWindow(v->dpy,v->screen),size,rotation,rates[i],CurrentTime)==Success){
            v->video_saved_rr_size=size;v->video_saved_rr_rotation=rotation;
            v->video_saved_rr_rate=old_rate;v->video_rr_changed=1;
        }
        break;
    }
    XRRFreeScreenConfigInfo(sc);
}
#endif
#endif
static void video_apply_display(AvpPcVideo *v)
{
    unsigned mode=v->cfg.video_window_mode;
    if(!v->dpy||!v->win)return;
    /* Important: release capture before moving to a different monitor mode. */
    if(v->mouse_grabbed)XUngrabPointer(v->dpy,CurrentTime);
    v->mouse_grabbed=0;
#ifdef _WIN32
    avp_win32_display_apply(v->win,mode,v->cfg.video_window_width,v->cfg.video_window_height,v->cfg.video_refresh);
#else
#ifdef AVP_HAVE_XRANDR
    video_x11_restore_refresh(v);
#endif
    if(mode==0){
        video_x11_fullscreen(v,0);video_x11_decorations(v,1);
        XResizeWindow(v->dpy,v->win,v->cfg.video_window_width,v->cfg.video_window_height);
    }else if(mode==1){
        video_x11_fullscreen(v,0);video_x11_decorations(v,0);
        XMoveResizeWindow(v->dpy,v->win,0,0,(unsigned)DisplayWidth(v->dpy,v->screen),(unsigned)DisplayHeight(v->dpy,v->screen));
    }else{
        video_x11_decorations(v,1);video_x11_fullscreen(v,1);
#ifdef AVP_HAVE_XRANDR
        video_x11_apply_refresh(v);
#endif
    }
    XFlush(v->dpy);
#endif
    v->video_switch_pending=1;
}
static void map_maker_key(AvpPcVideo *v,KeySym k);
static int map_maker_paste(AvpMapMaker *m,unsigned x,unsigned y,unsigned field);
static int map_maker_paste_target(AvpMapMaker *m);
static void map_maker_play_test(AvpPcVideo *v);
static void map_maker_object_edit(AvpMapMaker *m,int place);
/* Host-only mix settings; used after audio startup and on each F12 change. */
void avp_pc_video_audio_apply_settings(const AvpPcVideo *v)
{
    if(!v)return;
    avp_pc_audio_set_mix(v->cfg.audio_music_volume,v->cfg.audio_sfx_volume,
        v->cfg.audio_ambience_volume,v->cfg.audio_voice_volume,
        v->cfg.audio_hissing_volume,
        v->cfg.audio_music_on,v->cfg.audio_ambience_on,v->cfg.audio_hissing_on);
    avp_pc_audio_set_speaker_mode(v->cfg.audio_speaker_mode,v->cfg.audio_subwoofers);
}
static void menu_audio_adjust(AvpPcVideo *v,int row,int dir)
{
    unsigned *vol=NULL;
    if(!v||!dir||row==2)return; /* status row is read-only */
    if(row==0)v->cfg.audio_speaker_mode=(v->cfg.audio_speaker_mode+
        (dir>0?1u:AVP_AUDIO_LAYOUT_COUNT-1u))%AVP_AUDIO_LAYOUT_COUNT;
    else if(row==1)v->cfg.audio_subwoofers=(v->cfg.audio_subwoofers+(dir>0?1u:2u))%3u;
    else if(row==3)vol=&v->cfg.audio_music_volume;
    else if(row==4)vol=&v->cfg.audio_sfx_volume;
    else if(row==5)vol=&v->cfg.audio_ambience_volume;
    else if(row==6)vol=&v->cfg.audio_hissing_volume;
    else if(row==7)vol=&v->cfg.audio_voice_volume;
    else if(row==8)v->cfg.audio_music_on=!v->cfg.audio_music_on;
    else if(row==9)v->cfg.audio_ambience_on=!v->cfg.audio_ambience_on;
    else if(row==10)v->cfg.audio_hissing_on=!v->cfg.audio_hissing_on;
    if(vol){
        if(dir>0&&*vol<100u)(*vol)++;
        else if(dir<0&&*vol>0u)(*vol)--;
    }
    avp_pc_video_audio_apply_settings(v);
    save_settings(v);
}
static int menu_count(const AvpPcVideo*v){switch(v->menu_page){
    case MENU_ROOT:return 8;case MENU_DEBUG:return 1;case MENU_AUDIO:return 11;case MENU_MAP_MAKER:return 0;
    case MENU_MM_BINDINGS:return 3;
    case MENU_MM_KEYS:case MENU_MM_MOUSE:case MENU_MM_PAD:return AVP_MM_ACTIONS;case MENU_VIDEO:return
#ifdef AVP_LOCAL_MULTIPLAYER
        11;
#else
        9;
#endif
    case MENU_CONTROLS:return
#ifdef AVP_LOCAL_MULTIPLAYER
        v->local_mp.enabled?5:4;
#else
        4;
#endif
    case MENU_GAMEPAD:return 3;case MENU_KEYS:return v->cfg.controller_mode_jaguar?1:2;
    case MENU_MOUSE:return 2;
    case MENU_GAMEPAD_AXES:return COUNT(axis_items);
    case MENU_GAMEPAD_PC:return COUNT(pc_gamepad_items);
    case MENU_GAMEPAD_JAGUAR:return COUNT(jag_gamepad_items);
    case MENU_GAMEPAD_FRONTEND:return COUNT(frontend_gamepad_items);
    case MENU_KEYS_PC:return COUNT(pc_keyboard_items);
    case MENU_KEYS_FRONTEND:return COUNT(frontend_keyboard_items);
    case MENU_MOUSE_SETTINGS:return 3;
    case MENU_MOUSE_BINDINGS:return AVP_MOUSE_ACTIONS;
    case MENU_CHEATS:return 3;
    case MENU_CHEATS_ORIGINAL:return 3;
    case MENU_CHEATS_NEW:return COUNT(cheat_items);
    case MENU_CHEATS_FUJI:return 9;
    case MENU_CHEATS_BANANA:return 11;
    case MENU_CHEATS_GPS:return 4;
    case MENU_CHEATS_LEVEL:return 3;
    case MENU_MISC:return COUNT(misc_items);
    case MENU_ENHANCEMENTS:return COUNT(enhancement_items);
#ifdef AVP_LOCAL_MULTIPLAYER
    case MENU_MP_CONTROLS:return AVP_MP_CONTROL_BIND_ROW+(int)AVP_MP_INPUT_ACTIONS+5;
#endif
    default:return 0;}}
static int menu_id(const AvpPcVideo*v,int row){switch(v->menu_page){
    case MENU_CONTROLS:return row==0?25:-1;
    case MENU_GAMEPAD_AXES:return axis_items[row];case MENU_GAMEPAD_PC:return pc_gamepad_items[row];
    case MENU_GAMEPAD_JAGUAR:return jag_gamepad_items[row];case MENU_GAMEPAD_FRONTEND:return frontend_gamepad_items[row];
    case MENU_KEYS_PC:return pc_keyboard_items[row];case MENU_KEYS_FRONTEND:return frontend_keyboard_items[row];
    case MENU_MOUSE_SETTINGS:return 95+row;case MENU_MOUSE_BINDINGS:return 98+row;
    case MENU_CHEATS_NEW:return cheat_items[row];case MENU_MISC:return misc_items[row];
    case MENU_ENHANCEMENTS:return enhancement_items[row];
    default:return -1;}}
static int resize_surface(AvpPcVideo*v,unsigned win_w,unsigned win_h);
static void menu_video_adjust(AvpPcVideo *v,int row,int dir)
{
    if(!v || !dir)return;
    if(row==0){
        v->cfg.video_aspect=(v->cfg.video_aspect+(dir>0?1u:4u))%5u;
        (void)resize_surface(v,v->win_w,v->win_h);
    }else if(row==1){
        v->cfg.video_window_mode=(v->cfg.video_window_mode+(dir>0?1u:2u))%3u;
        video_apply_display(v);
    }else if(row==2){
        unsigned next=video_size_next(&v->cfg,dir);
        v->cfg.video_window_width=video_sizes[next][0];v->cfg.video_window_height=video_sizes[next][1];
        if(v->cfg.video_window_mode==0)video_apply_display(v);
    }else if(row==3){
        unsigned rates[48],n=video_refresh_options(v,rates,48),i=0;
        for(unsigned j=0;j<n;j++)if(rates[j]==v->cfg.video_refresh){i=j;break;}
        if(n>0)v->cfg.video_refresh=rates[(i+n+(dir>0?1u:n-1u))%n];
        if(v->cfg.video_window_mode==2)video_apply_display(v);
    }else if(row==4){video_apply_display(v);}
#ifdef AVP_LOCAL_MULTIPLAYER
    else if(row==5){
        v->local_mp.layout=(v->local_mp.layout+1u)%2u;
        v->local_mp_runtime_setup.layout=v->local_mp.layout;
    }else if(row==6){
        v->local_mp.three_layout=(v->local_mp.three_layout+1u)%2u;
        v->local_mp_runtime_setup.three_layout=v->local_mp.three_layout;
    }else if(row==7){
        v->cfg.presentation_hz=avp_presentation_rate_neighbor(v->cfg.presentation_hz,dir);
    }else if(row==8){
        v->cfg.show_fps=!v->cfg.show_fps;
        avp_pc_fps_meter_reset(&v->fps_meter);
    }else if(row==9 && avp_present_available(v))v->cfg.video_vsync=!v->cfg.video_vsync;
    else if(row==10)v->cfg.disable_fog=!v->cfg.disable_fog;
#else
    else if(row==5)v->cfg.presentation_hz=avp_presentation_rate_neighbor(v->cfg.presentation_hz,dir);
    else if(row==6){
        v->cfg.show_fps=!v->cfg.show_fps;
        avp_pc_fps_meter_reset(&v->fps_meter);
    }else if(row==7 && avp_present_available(v))v->cfg.video_vsync=!v->cfg.video_vsync;
    else if(row==8)v->cfg.disable_fog=!v->cfg.disable_fog;
#endif
    save_settings(v);
}
/* F12 Cheats > Activate For chooses a human viewport, not the input device.
 * Keep P1 as the default even when multiplayer is not compiled. */
static void cheat_queue_player_action(AvpPcVideo *v,unsigned action)
{
    if(!v || !action)return;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->cheat_target>0u && v->cheat_target<AVP_LOCAL_MP_MAX_PLAYERS){
        v->local_mp_cheat_actions[v->cheat_target]|=action;
        return;
    }
#endif
    if(v->cheat_target==0u)v->pending_actions|=action;
}
/* Existing New cheats use the P1 settings for P1, or the existing P2-P4
 * per-player profile. Global/world commands intentionally remain global. */
static int cheat_player_adjust_new(AvpPcVideo *v,int id)
{
#ifdef AVP_LOCAL_MULTIPLAYER
    AvpMpInputProfile *p;
    if(!v || v->cheat_target==0u || v->cheat_target>=AVP_LOCAL_MP_MAX_PLAYERS)return 0;
    p=&v->local_mp_controls.player[v->cheat_target];
    switch(id){
    case 48:cheat_queue_player_action(v,AVP_PC_ACTION_RESTORE_HEALTH);break;
    case 49:cheat_queue_player_action(v,AVP_PC_ACTION_RESTORE_AMMO);break;
    case 50:p->cheat_infinite_health=!p->cheat_infinite_health;break;
    case 51:p->cheat_infinite_ammo=!p->cheat_infinite_ammo;break;
    case 52:cheat_queue_player_action(v,AVP_PC_ACTION_GIVE_WEAPONS);break;
    case 57:p->cheat_tracker=!p->cheat_tracker;break;
    case 58:cheat_queue_player_action(v,AVP_PC_ACTION_SECURITY_UP);break;
    case 59:cheat_queue_player_action(v,AVP_PC_ACTION_SECURITY_DOWN);break;
    case 109:p->cheat_noclip=!p->cheat_noclip;break;
    case 126:p->cheat_no_facehug=!p->cheat_no_facehug;break;
    case 173:p->cheat_reveal_map=!p->cheat_reveal_map;break;
    case 127:p->cheat_no_enemy_attack=!p->cheat_no_enemy_attack;break;
    default:return 0; /* Shared world actions retain their legacy behavior. */
    }
    save_settings(v);
    return 1;
#else
    (void)v;(void)id;return 0;
#endif
}
/* Original action menu rows retain their own enable gates. Enabling Banana
 * does not silently enable God Mode or grant gear; those are explicit actions. */
static void original_cheat_adjust(AvpPcVideo *v,int row,int dir)
{
    unsigned action=0u,p=v->cheat_target;
    const int banana=v->menu_page==MENU_CHEATS_BANANA;
    const int fuji=v->menu_page==MENU_CHEATS_FUJI;
    if(p>=4u)return;
    if(v->menu_page==MENU_CHEATS_GPS){
        if(row==0)v->orig_position[p]=!v->orig_position[p];
        return;
    }
    if(v->menu_page==MENU_CHEATS_LEVEL){
        if(v->orig_banana[p] && row>0){
            action=row==1?AVP_PC_ACTION_ORIG_LEVEL_NEXT:AVP_PC_ACTION_ORIG_LEVEL_PREVIOUS;
            v->menu_open=0; /* Original level transfer uses gameplay tick. */
        }
        cheat_queue_player_action(v,action);
        return;
    }
    if(!banana && !fuji)return;
    if(row==0){
        if(banana){v->orig_banana[p]=!v->orig_banana[p];
            if(!v->orig_banana[p])v->orig_god[p]=0;
        }else v->orig_fuji[p]=!v->orig_fuji[p];
        return;
    }
    if(!(banana?v->orig_banana[p]:v->orig_fuji[p]))return;
    switch(row){
    case 1:action=AVP_PC_ACTION_SECURITY_UP;break;
    case 2:action=AVP_PC_ACTION_SECURITY_DOWN;break;
    case 3:action=AVP_PC_ACTION_ORIG_TRACKER_TOGGLE;break;
    case 4:case 5:case 6:case 7:
        action=AVP_PC_ACTION_ORIG_WEAPON1<<(row-4);break;
    case 8:action=AVP_PC_ACTION_RESTORE_AMMO;break;
    case 9:if(banana)v->orig_god[p]=!v->orig_god[p];break;
    default:break;
    }
    (void)dir;
    cheat_queue_player_action(v,action);
}
/* Original cartridge button sequences are available alongside F12 switches.
 * One physical P1 Jaguar pad always acts on P1 (never the F12 "Activate For"
 * selection); F12 remains an independent way to control P2..P4. The native
 * DoPause still handles the retail pause UI and its own state untouched. */
static void original_input_sample(AvpPcVideo *v,unsigned player,u32 pad)
{
    unsigned flags,actions=0u;
    if(!v||player>=4u)return;
    if(v->menu_open||v->frontend_input||avp_computer_active()||
       !v->mouse_focused||v->presenting_frontend){
        avp_pc_original_input_reset(&v->orig_input[player]);
        return;
    }
    flags=avp_pc_original_input_step(&v->orig_input[player],pad,
                                    v->orig_fuji[player],v->orig_banana[player]);
    if(flags&AVP_PC_ORIG_FUJI){
        v->orig_fuji[player]=1;
        v->orig_banana[player]=0;
        v->orig_god[player]=0;
    }
    if(flags&AVP_PC_ORIG_BANANA){
        v->orig_banana[player]=1;
        v->orig_fuji[player]=0;
    }
    if(flags&AVP_PC_ORIG_GPS)v->orig_position[player]=!v->orig_position[player];
    if((flags&AVP_PC_ORIG_GOD)&&v->orig_banana[player])
        v->orig_god[player]=!v->orig_god[player];
    if(flags&AVP_PC_ORIG_AMMO)actions|=AVP_PC_ACTION_RESTORE_AMMO;
    if(flags&AVP_PC_ORIG_TRACKER)actions|=AVP_PC_ACTION_ORIG_TRACKER_TOGGLE;
    if(flags&AVP_PC_ORIG_SECURITY_UP)actions|=AVP_PC_ACTION_SECURITY_UP;
    if(flags&AVP_PC_ORIG_SECURITY_DOWN)actions|=AVP_PC_ACTION_SECURITY_DOWN;
    if(flags&AVP_PC_ORIG_WEAPON1)actions|=AVP_PC_ACTION_ORIG_WEAPON1;
    if(flags&AVP_PC_ORIG_WEAPON2)actions|=AVP_PC_ACTION_ORIG_WEAPON2;
    if(flags&AVP_PC_ORIG_WEAPON3)actions|=AVP_PC_ACTION_ORIG_WEAPON3;
    if(flags&AVP_PC_ORIG_WEAPON4)actions|=AVP_PC_ACTION_ORIG_WEAPON4;
    if(flags&AVP_PC_ORIG_LEVEL_NEXT)actions|=AVP_PC_ACTION_ORIG_LEVEL_NEXT;
    if(flags&AVP_PC_ORIG_LEVEL_PREV)actions|=AVP_PC_ACTION_ORIG_LEVEL_PREVIOUS;
    if(actions){
        const unsigned old=v->cheat_target;
        v->cheat_target=player;
        cheat_queue_player_action(v,actions);
        v->cheat_target=old;
    }
}
/* A single inline selector shared by Original and New cheats.  The F12
 * Left/Right controls (keyboard and gamepad) cycle the target immediately,
 * including wraparound.  This is session-only, as in R53. */
static void menu_cheats_target_adjust(AvpPcVideo *v,int dir)
{
    if(!v || !dir)return;
    v->cheat_target=(v->cheat_target+
        (dir>0?1u:AVP_LOCAL_MP_MAX_PLAYERS-1u))%AVP_LOCAL_MP_MAX_PLAYERS;
}
static void menu_choose(AvpPcVideo*v){
    int id=menu_id(v,v->menu_cursor);v->menu_index=id;
    if(v->menu_page==MENU_ROOT){
        static const int pages[]={MENU_CONTROLS,MENU_VIDEO,MENU_AUDIO,MENU_ENHANCEMENTS,MENU_CHEATS,MENU_DEBUG,MENU_MISC,MENU_MAP_MAKER};
        v->menu_page=pages[v->menu_cursor];v->menu_cursor=0;
        if(v->menu_page==MENU_MAP_MAKER){
            unsigned level=(cur_level>=1&&cur_level<=15)?(unsigned)cur_level:1u;
            if(!v->map_maker.editor_started){
                (void)avp_mm_select(&v->map_maker,level);
                (void)avp_mm_blank(&v->map_maker);
                v->map_maker.editor_started=1;
            }
        }
        return;}
    if(v->menu_page==MENU_DEBUG){
        if(v->menu_cursor==0 && !v->frontend_input)
            v->pending_actions|=AVP_PC_ACTION_REFRESH_LEVEL;
        return;
    }
    if(v->menu_page==MENU_CHEATS){
        /* The third row is an inline Left/Right setting, not a submenu.
         * Enter must not open a player picker or change the target. */
        if(v->menu_cursor==2)return;
        v->menu_page=v->menu_cursor==0?MENU_CHEATS_ORIGINAL:MENU_CHEATS_NEW;
        v->menu_cursor=0;return;
    }
    if(v->menu_page==MENU_CHEATS_ORIGINAL){
        static const int pages[]={MENU_CHEATS_FUJI,MENU_CHEATS_BANANA,MENU_CHEATS_GPS};
        v->menu_page=pages[v->menu_cursor];v->menu_cursor=0;return;
    }
    if(v->menu_page==MENU_CHEATS_FUJI||v->menu_page==MENU_CHEATS_BANANA||
       v->menu_page==MENU_CHEATS_GPS){
        if(v->menu_page==MENU_CHEATS_BANANA && v->menu_cursor==10){
            v->menu_page=MENU_CHEATS_LEVEL;v->menu_cursor=0;return;
        }
        original_cheat_adjust(v,v->menu_cursor,1);return;
    }
    if(v->menu_page==MENU_CHEATS_LEVEL){
        original_cheat_adjust(v,v->menu_cursor,1);return;
    }
    if(v->menu_page==MENU_MM_BINDINGS){
        static const int pages[]={MENU_MM_KEYS,MENU_MM_MOUSE,MENU_MM_PAD};
        v->menu_page=pages[v->menu_cursor];v->menu_cursor=0;return;
    }
    if(v->menu_page==MENU_MM_KEYS||v->menu_page==MENU_MM_MOUSE||v->menu_page==MENU_MM_PAD){
        v->capture_mode=v->menu_page==MENU_MM_KEYS?9:(v->menu_page==MENU_MM_MOUSE?10:11);
        v->capture_target=v->menu_cursor;return;
    }
    if(v->menu_page==MENU_AUDIO){menu_audio_adjust(v,v->menu_cursor,1);return;}
    if(v->menu_page==MENU_VIDEO){
        /* Same Video items are handled by the arrow keys; Enter cycles. */
        /* Forward-declared below to avoid changing the original menu items. */
        menu_video_adjust(v,v->menu_cursor,1);return;
    }
    if(v->menu_page==MENU_CONTROLS){if(v->menu_cursor==0){menu_adjust(v,1);return;}
#ifdef AVP_LOCAL_MULTIPLAYER
        if(v->menu_cursor==4 && v->local_mp.enabled){v->menu_page=MENU_MP_CONTROLS;v->menu_cursor=0;v->capture_mode=0;return;}
#endif
        v->menu_page=v->menu_cursor==1?MENU_GAMEPAD:(v->menu_cursor==2?MENU_KEYS:MENU_MOUSE);v->menu_cursor=0;return;}
    if(v->menu_page==MENU_GAMEPAD){v->menu_page=v->menu_cursor==0?MENU_GAMEPAD_AXES:
        (v->menu_cursor==2?MENU_GAMEPAD_FRONTEND:(v->cfg.controller_mode_jaguar?MENU_GAMEPAD_JAGUAR:MENU_GAMEPAD_PC));v->menu_cursor=0;return;}
    if(v->menu_page==MENU_KEYS){v->menu_page=(v->cfg.controller_mode_jaguar||v->menu_cursor==1)?MENU_KEYS_FRONTEND:MENU_KEYS_PC;v->menu_cursor=0;return;}
    if(v->menu_page==MENU_MOUSE){v->menu_page=v->menu_cursor==0?MENU_MOUSE_SETTINGS:MENU_MOUSE_BINDINGS;v->menu_cursor=0;return;}
    if(id>=98&&id<98+AVP_MOUSE_ACTIONS){v->capture_mode=4;v->capture_target=id;return;}
    if(id==4||id==6||id==8)begin_capture(v,2,id);
    else if((id>=10&&id<=12)||(id>=26&&id<=47)||(id>=68&&id<76)||id==83||id==84||(id>=88&&id<=94)||(id>=117&&id<=122)||id==162||(id>=153&&id<=156)||id==164||id==165)begin_capture(v,1,id);
    else if((id>=13&&id<=22)||(id>=60&&id<68)||id==81||id==82||(id>=85&&id<=87)||(id>=111&&id<=116)||id==161||(id>=149&&id<=152))begin_capture(v,3,id);
    else menu_adjust(v,1);
}
static void menu_back(AvpPcVideo*v){if(v->capture_mode){v->capture_mode=0;return;}
    switch(v->menu_page){
    case MENU_ROOT:v->menu_open=0;break;
    case MENU_CHEATS_ORIGINAL:case MENU_CHEATS_NEW:v->menu_page=MENU_CHEATS;break;
    case MENU_CHEATS_FUJI:case MENU_CHEATS_BANANA:case MENU_CHEATS_GPS:
        v->menu_page=MENU_CHEATS_ORIGINAL;break;
    case MENU_CHEATS_LEVEL:v->menu_page=MENU_CHEATS_BANANA;break;
    case MENU_MM_KEYS:case MENU_MM_MOUSE:case MENU_MM_PAD:v->menu_page=MENU_MM_BINDINGS;break;
    case MENU_MM_BINDINGS:v->menu_page=MENU_MAP_MAKER;break;
    case MENU_MAP_MAKER:if(v->map_maker.palette_open)v->map_maker.palette_open=0;
        else v->menu_page=MENU_ROOT;
        break;
    case MENU_GAMEPAD_AXES:case MENU_GAMEPAD_PC:case MENU_GAMEPAD_JAGUAR:case MENU_GAMEPAD_FRONTEND:v->menu_page=MENU_GAMEPAD;break;
    case MENU_KEYS_PC:case MENU_KEYS_FRONTEND:v->menu_page=MENU_KEYS;break;
    case MENU_MOUSE_SETTINGS:case MENU_MOUSE_BINDINGS:v->menu_page=MENU_MOUSE;break;
    case MENU_GAMEPAD:case MENU_KEYS:case MENU_MOUSE:v->menu_page=MENU_CONTROLS;break;
#ifdef AVP_LOCAL_MULTIPLAYER
    case MENU_MP_CONTROLS:v->menu_page=MENU_CONTROLS;break;
#endif
    default:v->menu_page=MENU_ROOT;break;}
    v->menu_cursor=0;
}
/* F12 Backspace is menu navigation, never an unbind request.  When a
 * binding is being captured, the first Backspace cancels capture; a
 * subsequent press returns to the parent category.  Outside F12 this
 * key is left entirely to the existing game input bindings. */
static int f12_backspace(AvpPcVideo *v, KeySym k)
{
    if(!v->menu_open || k!=XK_BackSpace)return 0;
    if(v->menu_page==MENU_MAP_MAKER &&
       (v->map_maker.name_edit_open||v->map_maker.browser_open||v->map_maker.context_open)){
        map_maker_key(v,k);return 1;
    }
    menu_back(v);
    return 1;
}
static const char *menu_caption(int page){switch(page){
    case MENU_ROOT:return "AVP Options";case MENU_AUDIO:return "Audio";case MENU_VIDEO:return "Video";case MENU_CONTROLS:return "Controls";
    case MENU_MAP_MAKER:return "Map Maker";
    case MENU_GAMEPAD:return "Gamepad / Joystick Categories";case MENU_GAMEPAD_AXES:return "Axes and Per-Axis Deadzones";
    case MENU_GAMEPAD_PC:return "PC Action Joystick Bindings";case MENU_GAMEPAD_JAGUAR:return "Jaguar Virtual Pad Bindings";
    case MENU_GAMEPAD_FRONTEND:return "Frontend Joystick Bindings";
    case MENU_KEYS:return "Keyboard Categories";case MENU_KEYS_PC:return "PC Action Key Bindings";
    case MENU_KEYS_FRONTEND:return "Frontend Key Bindings";
    case MENU_MOUSE:return "Mouse Categories";case MENU_MOUSE_SETTINGS:return "Mouse Settings";
    case MENU_MOUSE_BINDINGS:return "Mouse Button Bindings";
    case MENU_CHEATS:return "Cheats";
    case MENU_CHEATS_ORIGINAL:return "Original";
    case MENU_CHEATS_NEW:return "New";
    case MENU_CHEATS_FUJI:return "Hold That Fuji";
    case MENU_CHEATS_BANANA:return "Banana Stops";
    case MENU_CHEATS_GPS:return "Global Positioning";
    case MENU_CHEATS_LEVEL:return "Banana Stops: Level Select";
    case MENU_ENHANCEMENTS:return "Enhancements";
    case MENU_DEBUG:return "Debug";
#ifdef AVP_LOCAL_MULTIPLAYER
    case MENU_MP_CONTROLS:return "Local Multiplayer: Player Controls";
#endif
    default:return "Miscellaneous";}}
static int mouse_action_down(const AvpPcVideo*v,int action){int b=v->cfg.mouse_button[action];return v->mouse_grabbed&&b>0&&b<16&&v->mouse_down[b];}
static void mouse_grab_update(AvpPcVideo*v){
    int want=v->cfg.mouse_enabled&&!v->menu_open&&!v->frontend_input&&v->mouse_focused;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_open)want=0;
#endif
    if(want==v->mouse_grabbed)return;
    if(want){
        if(XGrabPointer(v->dpy,v->win,True,PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
            GrabModeAsync,GrabModeAsync,v->win,None,CurrentTime)!=GrabSuccess)return;
#ifndef _WIN32
        if(v->invisible_cursor)XDefineCursor(v->dpy,v->win,v->invisible_cursor);
#endif
        v->mouse_grabbed=1;v->mouse_recenter=1;v->mouse_last_x=(int)v->win_w/2;v->mouse_motion_seen=0;
        XWarpPointer(v->dpy,None,v->win,0,0,0,0,(int)v->win_w/2,(int)v->win_h/2);
    }else{
        XUngrabPointer(v->dpy,CurrentTime);
#ifndef _WIN32
        XUndefineCursor(v->dpy,v->win);
#endif
        v->mouse_grabbed=0;v->mouse_recenter=0;v->mouse_motion_seen=0;v->mouse_delta=0.0f;memset(v->mouse_down,0,sizeof(v->mouse_down));
    }
    XFlush(v->dpy);
}

#ifdef AVP_HAVE_XSHM
static int shm_attach_failed;
static int shm_error_handler(Display*d,XErrorEvent*e){(void)d;(void)e;shm_attach_failed=1;return 0;}
/* Create the window-sized present image in a SysV shared segment.  Returns 0
 * (caller uses the ordinary XPutImage path) when the server is remote, the
 * extension is missing, or the attach is refused. */
static int present_create_shm(AvpPcVideo*v,unsigned pw,unsigned ph,uint32_t**pix_out,XImage**img_out)
{
    XImage*img;int (*old)(Display*,XErrorEvent*);
    if(v->shm_unavailable||!XShmQueryExtension(v->dpy))return 0;
    img=XShmCreateImage(v->dpy,DefaultVisual(v->dpy,v->screen),DefaultDepth(v->dpy,v->screen),
                        ZPixmap,NULL,&v->shm,pw,ph);
    if(!img)return 0;
    if(img->bits_per_pixel!=32||img->bytes_per_line!=(int)(pw*4u)){XDestroyImage(img);v->shm_unavailable=1;return 0;}
    v->shm.shmid=shmget(IPC_PRIVATE,(size_t)img->bytes_per_line*ph,IPC_CREAT|0600);
    if(v->shm.shmid<0){XDestroyImage(img);v->shm_unavailable=1;return 0;}
    v->shm.shmaddr=img->data=(char*)shmat(v->shm.shmid,NULL,0);
    if(v->shm.shmaddr==(char*)-1){shmctl(v->shm.shmid,IPC_RMID,NULL);img->data=NULL;XDestroyImage(img);v->shm_unavailable=1;return 0;}
    v->shm.readOnly=False;
    XSync(v->dpy,False);shm_attach_failed=0;old=XSetErrorHandler(shm_error_handler);
    if(!XShmAttach(v->dpy,&v->shm))shm_attach_failed=1;
    XSync(v->dpy,False);XSetErrorHandler(old);
    shmctl(v->shm.shmid,IPC_RMID,NULL); /* freed automatically once both sides detach */
    if(shm_attach_failed){shmdt(v->shm.shmaddr);img->data=NULL;XDestroyImage(img);v->shm_unavailable=1;return 0;}
    memset(img->data,0,(size_t)img->bytes_per_line*ph);
    v->shm_active=1;*pix_out=(uint32_t*)img->data;*img_out=img;return 1;
}
static void present_destroy_image(AvpPcVideo*v)
{
    if(!v->present_img){free(v->present_pixels);v->present_pixels=NULL;return;}
    if(v->shm_active){
        XShmDetach(v->dpy,&v->shm);XSync(v->dpy,False);
        shmdt(v->shm.shmaddr);v->present_img->data=NULL;XDestroyImage(v->present_img);
        v->shm_active=0;
    }else{v->present_img->data=NULL;XDestroyImage(v->present_img);free(v->present_pixels);}
    v->present_img=NULL;v->present_pixels=NULL;
}
#else
static void present_destroy_image(AvpPcVideo*v)
{
    if(v->present_img){v->present_img->data=NULL;XDestroyImage(v->present_img);}
    free(v->present_pixels);v->present_img=NULL;v->present_pixels=NULL;
}
#endif
static int resize_surface(AvpPcVideo*v,unsigned win_w,unsigned win_h)
{
    uint32_t*pix;XImage*img;unsigned pw,ph;
    if(win_w<320u)win_w=320u;
    if(win_h<228u)win_h=228u;
    v->win_w=win_w;v->win_h=win_h;

    /* Render the game at the Jaguar's native 320x228 maze raster.  M2.1/M2.2
       increased the raycaster workload with the host window size, which both
       distorted HUD scaling and could starve the fixed-step gameplay loop on
       large windows.  Native render + presentation scaling matches the retail
       raster and keeps gameplay timing independent of resize. */
    if(!v->img||!v->fb.pixels){
        if(!avp_jaguar_framebuffer_init(&v->fb,320u,228u))return 0;
        img=XCreateImage(v->dpy,DefaultVisual(v->dpy,v->screen),DefaultDepth(v->dpy,v->screen),ZPixmap,0,(char*)v->fb.pixels,v->fb.width,v->fb.height,32,0);
        if(!img){avp_jaguar_framebuffer_destroy(&v->fb);return 0;}
        v->img=img;
    }

    {static const unsigned ar_w[5]={4u,16u,16u,21u,32u};
     static const unsigned ar_h[5]={3u,9u,10u,9u,9u};
     unsigned choice=v->cfg.video_aspect<5u?v->cfg.video_aspect:0u;
     const unsigned aw=ar_w[choice],ah=ar_h[choice];
     pw=win_w;ph=(unsigned)(((uint64_t)pw*ah+aw/2u)/aw);
     if(ph>win_h){ph=win_h;pw=(unsigned)(((uint64_t)ph*aw+ah/2u)/ah);}
    }
    if(pw==v->present_w&&ph==v->present_h&&v->present_img&&v->present_pixels&&v->backbuf)return 1;
    present_destroy_image(v);
#ifdef AVP_HAVE_XSHM
    if(!present_create_shm(v,pw,ph,&pix,&img))
#endif
    {
        pix=calloc((size_t)pw*ph,sizeof(uint32_t));if(!pix)return 0;
        img=XCreateImage(v->dpy,DefaultVisual(v->dpy,v->screen),DefaultDepth(v->dpy,v->screen),ZPixmap,0,(char*)pix,pw,ph,32,0);
        if(!img){free(pix);return 0;}
    }
    v->present_pixels=pix;v->present_img=img;v->present_w=pw;v->present_h=ph;
    avp_jaguar_framebuffer_destroy(&v->world_fb);
    v->presenting_world_hires=0;
    if(v->backbuf) XFreePixmap(v->dpy,v->backbuf);
    v->backbuf=XCreatePixmap(v->dpy,v->win,win_w,win_h,(unsigned)DefaultDepth(v->dpy,v->screen));
    return v->backbuf!=0;
}

static void scale_present(AvpPcVideo *v)
{
    if(v&&v->presenting_frontend&&v->frontend_fb.pixels&&v->present_pixels&&
       v->present_w&&v->present_h){
        /* NTSC SetScreen(320,240): show exactly the 240 frontend lines from
         * VDB (BigPEmu: title row 0 is the first displayed line, 6 lines
         * above the gameplay VDB), same square pixels as gameplay; any
         * spare window width is black. */
        unsigned h=v->present_h,w=(unsigned)(((unsigned long long)h*320u+120u)/240u);
        if(w>v->present_w){w=v->present_w;h=(unsigned)(((unsigned long long)w*240u+160u)/320u);}
        const unsigned x0=(v->present_w-w)/2u,y0=(v->present_h-h)/2u;
        memset(v->present_pixels,0,(size_t)v->present_w*v->present_h*sizeof(uint32_t));
        for(unsigned y=0;y<h;y++){
            const unsigned sy=14u+(unsigned)(((unsigned long long)y*240u)/h);
            const uint32_t *src=v->frontend_fb.pixels+(size_t)sy*v->frontend_fb.width;
            uint32_t *dst=v->present_pixels+(size_t)(y0+y)*v->present_w+x0;
            for(unsigned x=0;x<w;x++)dst[x]=src[((unsigned long long)x*320u)/w];
        }
        return;
    }
    if(!v||!v->present_pixels||!v->fb.pixels||!v->present_w||!v->present_h)return;
    const AvpJaguarFramebuffer *srcfb=(v->presenting_world_hires&&v->world_fb.pixels)?&v->world_fb:&v->fb;
    avp_pc_scale_nearest32(srcfb->pixels,srcfb->width,srcfb->height,
                           v->present_pixels,v->present_w,v->present_h);
}

AvpPcVideo *avp_pc_video_create(unsigned w,unsigned h,const char *title){AvpPcVideo*v=calloc(1,sizeof(*v));XSizeHints hints;if(!v)return NULL;avp_mm_init(&v->map_maker);avp_gpu_state_init(&v->gpu);v->vblank_epoch_ns=avp_pc_now_ns();v->js_fd=-1;
#ifdef AVP_LOCAL_MULTIPLAYER
    for(unsigned i=0;i<AVP_MP_INPUT_PLAYERS;i++){
        v->local_mp_readers[i].fd=-1;v->local_mp_pad_readers[i].fd=-1;
    }
    v->local_mp_hud_owner=-1;
#endif
    v->mouse_focused=1;v->previous_poll_pad=0xffffffffu;load_settings(v);
    /* Pre-R21 settings stored Tab as Mode; migrate to dedicated keypad Enter.
     * Tab is now Copy in the 3D editor. */
    if(v->cfg.mm_key[MM_MODE]==XK_Tab)v->cfg.mm_key[MM_MODE]=XK_KP_Enter;
    avp_game_micro_opt_set(v->cfg.game_micro_optimization);avp_player_set_pain_sounds(v->cfg.player_pain_sounds_enabled);
    avp_player_set_facehug_pain_sounds(v->cfg.facehug_pain_sounds_enabled);
    avp_mazescrn_set_facehug_shake_pain_sounds(v->cfg.facehug_shake_pain_sounds_enabled);
    save_settings(v);v->dpy=XOpenDisplay(NULL);if(!v->dpy){free(v);return NULL;}XSetErrorHandler(pc_x_error);XSetIOErrorHandler(pc_x_io_error);
#ifndef _WIN32
    /* Prevent synthetic KeyRelease/KeyPress pairs on held keys (X11 repeat),
     * so Quick Turn remains edge-triggered even when a key is held down. */
    {Bool supported=False;(void)XkbSetDetectableAutoRepeat(v->dpy,True,&supported);}
#endif
    #ifndef _WIN32
    v->idle_inhibit=avp_pc_idle_inhibit_create(v->dpy);
#endif
    v->screen=DefaultScreen(v->dpy);w=v->cfg.video_window_width;h=v->cfg.video_window_height;v->win=XCreateSimpleWindow(v->dpy,RootWindow(v->dpy,v->screen),0,0,w,h,0,BlackPixel(v->dpy,v->screen),BlackPixel(v->dpy,v->screen));if(!v->win){avp_pc_video_destroy(v);return NULL;}XStoreName(v->dpy,v->win,title);XSelectInput(v->dpy,v->win,ExposureMask|KeyPressMask|KeyReleaseMask|StructureNotifyMask|FocusChangeMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask);
#ifndef _WIN32
    {static const char empty[]={0};Pixmap blank=XCreateBitmapFromData(v->dpy,v->win,empty,1,1);if(blank){XColor c;memset(&c,0,sizeof(c));v->invisible_cursor=XCreatePixmapCursor(v->dpy,blank,blank,&c,&c,0,0);XFreePixmap(v->dpy,blank);}}
#endif
    memset(&hints,0,sizeof(hints));hints.flags=PMinSize;hints.min_width=320;hints.min_height=228;XSetWMNormalHints(v->dpy,v->win,&hints);v->wm_protocols=XInternAtom(v->dpy,"WM_PROTOCOLS",False);v->wm_delete=XInternAtom(v->dpy,"WM_DELETE_WINDOW",False);XSetWMProtocols(v->dpy,v->win,&v->wm_delete,1);
#ifndef _WIN32
    avp_present_init(v);
#endif
    v->gc=XCreateGC(v->dpy,v->win,0,NULL);if(!resize_surface(v,w,h)){avp_pc_video_destroy(v);return NULL;}scan_js(v);XMapWindow(v->dpy,v->win);XSync(v->dpy,False);if(v->cfg.video_window_mode)video_apply_display(v);return v;}
#ifdef AVP_LOCAL_MULTIPLAYER
/* All borrowed textures, sprites and HUD brushes come from the original ROM
 * decoder, but each viewport has its own mutable drawing, interpolation and
 * one-bit automap buffers. NEVER call avp_gpu_state_destroy on a borrower: it
 * does not own the source's assets. This also runs before asset reload. */
int avp_pc_video_local_mp_walktest_mode(AvpPcVideo *v,int enable)
{
    if(!v)return 0;
    v->local_mp_walktest=!!enable;
    v->local_mp_render_alpha=1.0;
    if(!enable){
        avp_pc_video_local_mp_terminal_end(v);
        memset(&v->local_mp_runtime_setup,0,sizeof(v->local_mp_runtime_setup));
    }
    return 1;
}
const AvpMpInputConfig *avp_pc_video_local_mp_profiles(const AvpPcVideo *v)
{return v?&v->local_mp_controls:NULL;}
const AvpLocalMpSetup *avp_pc_video_local_mp_setup(const AvpPcVideo *v)
{return v?&v->local_mp:NULL;}
void avp_pc_video_local_mp_runtime_setup(AvpPcVideo *v,const AvpLocalMpSetup *setup)
{if(v&&setup)v->local_mp_runtime_setup=*setup;}
void avp_pc_video_local_mp_render_alpha(AvpPcVideo *v,double alpha)
{if(v)v->local_mp_render_alpha=alpha<0.0?0.0:alpha>1.0?1.0:alpha;}
static void local_mp_view_release(AvpPcVideo *v)
{
    unsigned p;
    if(!v)return;
    v->local_mp_hud_owner=-1;
    for(p=0;p<AVP_LOCAL_MP_MAX_PLAYERS;p++) {
        AvpAvpGpuState *s=v->local_mp_views[p].state;
        if(s) {
            free(s->hud_native_tiles);
            avp_gpu_maze2_occlusion_destroy(&s->maze_occlusion);
            free(s);
            v->local_mp_views[p].state=NULL;
        }
        avp_jaguar_framebuffer_destroy(&v->local_mp_views[p].frame);
    }
}
static int local_mp_view_ensure(AvpPcVideo *v,unsigned player)
{
    AvpAvpGpuState *s;
    if(!v||player>=AVP_LOCAL_MP_MAX_PLAYERS||!(v->local_mp.enabled||v->local_mp_walktest)||
       v->presenting_frontend)return 0;
    unsigned wanted_w=320u,wanted_h=228u;
    if(v->present_w && v->present_h){
        AvpLocalMpRect rect[AVP_LOCAL_MP_MAX_PLAYERS];
        if(avp_local_mp_viewports(v->local_mp_runtime_setup.enabled?
                &v->local_mp_runtime_setup:&v->local_mp,
                v->present_w,v->present_h,rect)>player &&
           rect[player].width && rect[player].height){
            /* Keep original 228-line rendering for vertical accuracy and
             * let a physically wider partition receive MORE ray columns.
             * High-resolution world mode instead uses the exact partition. */
            if(v->cfg.high_resolution_world){
                wanted_w=rect[player].width;wanted_h=rect[player].height;
            }else{
                wanted_w=avp_host_wide_native_width(rect[player].width,rect[player].height);
            }
        }
    }
    if(v->local_mp_views[player].state){
        AvpJaguarFramebuffer *frame=&v->local_mp_views[player].frame;
        if(frame->width!=wanted_w||frame->height!=wanted_h){
            avp_jaguar_framebuffer_destroy(frame);
            if(!avp_jaguar_framebuffer_init(frame,wanted_w,wanted_h))return 0;
        }
        return 1;
    }
    s=(AvpAvpGpuState *)malloc(sizeof(*s));
    if(!s)return 0;
    /* Borrow only. A renderer can allocate private HUD scratch and wall
     * occlusion, so detach them from the owner before its first draw. */
    *s=v->gpu;
    s->hud_native_tiles=NULL;
    memset(&s->maze_occlusion,0,sizeof(s->maze_occlusion));
    s->hud_msg_valid=0;
    memset(s->hud_msg,0,sizeof(s->hud_msg));
    memset(s->hud_msg_x,0,sizeof(s->hud_msg_x));
    s->computer_active=0;
    s->have_prev_sim=0;
    s->prev_maze_size=0;
    s->render_alpha=1.0;
    s->map_smooth_camera=0;
    s->host_continuous_heading=0;
    s->mp_waveform_enabled=0;
    s->mp_waveform_species=0;
    s->mp_waveform_previous=0;
    s->predator_scope_valid=0;
    if(!avp_jaguar_framebuffer_init(&v->local_mp_views[player].frame,wanted_w,wanted_h)){
        free(s);return 0;
    }
    /* A new co-op viewer must not inherit the legacy P1 map raster. */
    avp_gpu_automap_reset(s);
    v->local_mp_views[player].state=s;
    return 1;
}

int avp_pc_video_local_mp_set_other_players(AvpPcVideo *v,unsigned viewer,
    const AvpGpuOtherPlayer *actors,unsigned count)
{
    if(!v || !local_mp_view_ensure(v,viewer))return 0;
    return avp_gpu_set_other_players(v->local_mp_views[viewer].state,actors,count);
}

/* Requires the calling native backend's world+actor scope to be bound; that
 * supplies original sprite, HUD, player and current-level globals. Writes a
 * genuinely rasterized ROM-asset/CPU-state frame, never a copied P1 picture.
 * The caller retains the returned pixels until the next draw in this slot. */
/* Per-viewer raster belongs to that viewer's species. A co-op slot may
 * change species between sessions; never re-use a Predator HUD scope as the
 * Marine/Alien's PC analyzer, or vice versa. */
static void local_mp_waveform_select_species(AvpAvpGpuState *scope,unsigned species)
{
    if(!scope)return;
    if(species>2u)species=0u;
    if(scope->mp_waveform_species!=species){
        scope->mp_waveform_species=(unsigned char)species;
        scope->mp_waveform_enabled=(unsigned char)(species==2u);
        scope->mp_waveform_previous=0;
        scope->predator_scope_valid=0;
        memset(scope->predator_scope,0,sizeof(scope->predator_scope));
    }
}

int avp_pc_video_local_mp_render_actor(AvpPcVideo *v,unsigned player,
    const u8 *maze,u16 width,u16 height,u32 xpos,u32 ypos,u32 angle,
    unsigned level,u64 vblank,AvpLocalMpSourceFrame *out)
{
    AvpAvpGpuState *state;
    AvpJaguarFramebuffer *fb;
    if(!out||!maze||!width||width>64u||!height||height>64u||
       level<1u||level>15u||!local_mp_view_ensure(v,player))return 0;
    state=v->local_mp_views[player].state;
    local_mp_waveform_select_species(state,(unsigned)player_type>>2);
    fb=&v->local_mp_views[player].frame;
    /* Each viewport uses its OWN precision setting, not P1's. */
    state->host_continuous_heading=v->cfg.smooth_presentation&&
        (player==0 ? avp_pc_video_enhanced_analog_enabled(v) :
         (v->local_mp_controls.player[player].gamepad_enabled &&
          v->local_mp_controls.player[player].gamepad_analog_movement &&
          v->local_mp_controls.player[player].gamepad_enhanced_analog_controls));
    state->host_widescreen_fov=avp_host_has_extra_columns(fb->width,fb->height);
    state->host_extend_draw_distance=v->cfg.extend_draw_distance;
    state->host_extend_fog_distance=v->cfg.extend_fog_distance;
    state->host_disable_fog=v->cfg.disable_fog;
    state->hud_fixed_hue=(unsigned char)!!v->cfg.hud_fixed_hue;
    avp_gpu_set_render_alpha(state,v->local_mp_render_alpha);
    avp_gpu_render_maze(state,fb,maze,width,height,xpos,ypos,angle,
                        1,level,vblank,v->cfg.smooth_presentation,
                        v->cfg.debug_immediate_in_game);
    if(!fb->pixels)return 0;
    out->pixels=fb->pixels;
    out->width=fb->width;out->height=fb->height;out->pitch=fb->width;
    return 1;
}

int avp_pc_video_local_mp_waveform_step(AvpPcVideo *v,unsigned player,s32 left,s32 right)
{
    AvpAvpGpuState *s;
    (void)left;
    (void)right;
    if(!v || !local_mp_view_ensure(v,player))return 0;
    s=v->local_mp_views[player].state;
    local_mp_waveform_select_species(s,(unsigned)player_type>>2);
    /* Marine and Alien make sounds, but do not have a Predator waveform
     * panel. Predator's original HP2 event owns its native analyzer. */
    return 1;
}
int avp_pc_video_local_mp_map_teammates(AvpPcVideo *v,unsigned player,
    const AvpGpuMapTeammate *teammates,
    const AvpGpuMapTeammate *presentation,unsigned count,unsigned zoom_steps)
{
    if(!v || !local_mp_view_ensure(v,player))return 0;
    AvpAvpGpuState *state=v->local_mp_views[player].state;
    if(!avp_gpu_set_map_teammates(state,teammates,count))return 0;
    /* Visual positions are separate; do not modify the real 10-Hz
     * contacts used by the original Marine motion scanner. */
    if(presentation && !avp_gpu_set_map_teammates_draw(state,presentation,count))return 0;
    /* Keep the live roster for the Marine motion tracker; hide dots only
     * in the Jaguar automap overlay, in both multiplayer game modes. */
    avp_gpu_show_map_teammates(state,(int)v->local_mp.show_other_players_on_automap);
    avp_gpu_set_map_zoom(state,zoom_steps);
    return 1;
}
int avp_pc_video_local_mp_automap_observe(AvpPcVideo *v,unsigned player,
    const u8 *maze,u16 width,u16 height,u32 xpos,u32 ypos,u32 angle,unsigned level)
{
    if(!v||!maze||!width||!height||!local_mp_view_ensure(v,player))return 0;
    avp_gpu_automap_history_enter(&v->local_mp_automap_history[player],
        v->local_mp_views[player].state,level,
        player==0?v->cfg.remember_automap:v->local_mp_controls.player[player].remember_automap);
    avp_gpu_automap_observe(v->local_mp_views[player].state,maze,width,height,
                            xpos,ypos,angle,level);
    if(v->local_mp.show_whole_map || v->local_mp_controls.player[player].cheat_reveal_map)
        avp_gpu_automap_reveal_all(v->local_mp_views[player].state,maze,width,height,level);
    return 1;
}

int avp_pc_video_local_mp_snapshot_actor(AvpPcVideo *v,unsigned player,
    const u8 *maze,u16 width,u16 height)
{
    AvpAvpGpuState *state;
    if(!maze||!width||width>64u||!height||height>64u||
       !local_mp_view_ensure(v,player))return 0;
    state=v->local_mp_views[player].state;
    avp_gpu_snapshot_sim(state,maze,width,height);
    return 1;
}
#endif

static void map_editor_release(AvpPcVideo *v)
{
    if(v->map_editor_gpu){
        free(v->map_editor_gpu->hud_native_tiles);
        avp_gpu_maze2_occlusion_destroy(&v->map_editor_gpu->maze_occlusion);
        free(v->map_editor_gpu);v->map_editor_gpu=NULL;
    }
    avp_jaguar_framebuffer_destroy(&v->map_editor_fb);
}
static int map_editor_ensure(AvpPcVideo *v)
{
    AvpAvpGpuState *s;
    if(!v)return 0;
    if(v->map_editor_gpu){
        unsigned width=(unsigned)(((uint64_t)v->present_w*456u+v->present_h/2u)/(v->present_h?v->present_h:1u));
        if(width<320u)width=320u;
        if(width>1280u)width=1280u;
        if(v->map_editor_fb.width!=width){
            avp_jaguar_framebuffer_destroy(&v->map_editor_fb);
            if(!avp_jaguar_framebuffer_init(&v->map_editor_fb,width,456u))return 0;
        }
        v->map_editor_gpu->host_widescreen_fov=avp_host_has_extra_columns(width,456u);
        return 1;
    }
    s=malloc(sizeof(*s));if(!s)return 0;
    *s=v->gpu;
    s->hud_native_tiles=NULL;
    memset(&s->maze_occlusion,0,sizeof(s->maze_occlusion));
    s->have_prev_sim=0;s->prev_maze_size=0;s->computer_active=0;
    s->host_continuous_heading=1;s->host_widescreen_fov=0;
    s->map_editor_world_only=1;s->render_alpha=1.0;
    {unsigned width=(unsigned)(((uint64_t)v->present_w*456u+v->present_h/2u)/(v->present_h?v->present_h:1u));
     if(width<320u)width=320u;
     if(width>1280u)width=1280u;
     s->host_widescreen_fov=avp_host_has_extra_columns(width,456u);
     if(!avp_jaguar_framebuffer_init(&v->map_editor_fb,width,456u)){free(s);return 0;}}
    v->map_editor_gpu=s;return 1;
}
void avp_pc_video_destroy(AvpPcVideo*v){if(!v)return;save_settings(v);
    map_editor_release(v);
    avp_mm_destroy(&v->map_maker);
    avp_gpu_automap_history_destroy(&v->automap_history);
#ifdef AVP_LOCAL_MULTIPLAYER
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_PLAYERS;p++)
        avp_gpu_automap_history_destroy(&v->local_mp_automap_history[p]);
#endif
#ifdef AVP_LOCAL_MULTIPLAYER
    local_mp_view_release(v);
    for(unsigned i=0;i<AVP_MP_INPUT_PLAYERS;i++){
        avp_mp_mouse_reader_close(&v->local_mp_readers[i]);
        avp_mp_pad_reader_close(&v->local_mp_pad_readers[i]);
    }
#endif
if(v->mouse_grabbed&&v->dpy){XUngrabPointer(v->dpy,CurrentTime);v->mouse_grabbed=0;}
#ifndef _WIN32
    if(v->invisible_cursor&&v->dpy)XFreeCursor(v->dpy,v->invisible_cursor);
    avp_pc_idle_inhibit_destroy(v->idle_inhibit);
    v->idle_inhibit=NULL;
#endif
    close_js(v);
#ifdef _WIN32
    if(v->win)avp_win32_display_restore();
#elif defined(AVP_HAVE_XRANDR)
    if(v->dpy)video_x11_restore_refresh(v);
#endif
#ifdef AVP_LOCAL_MULTIPLAYER
    free(v->local_mp_terminal_background);
#endif
    free(v->gameplay_fade_snapshot);avp_pc_prototype_textures_destroy(&v->prototype_textures);avp_gpu_state_destroy(&v->gpu);if(v->img){v->img->data=NULL;XDestroyImage(v->img);}avp_jaguar_framebuffer_destroy(&v->fb);avp_jaguar_framebuffer_destroy(&v->world_fb);avp_jaguar_framebuffer_destroy(&v->frontend_fb);present_destroy_image(v);
#ifndef _WIN32
    avp_present_shutdown(v);
#endif
    if(v->dpy){if(v->backbuf)XFreePixmap(v->dpy,v->backbuf);if(v->gc)XFreeGC(v->dpy,v->gc);if(v->win)XDestroyWindow(v->dpy,v->win);XCloseDisplay(v->dpy);}free(v);}
int avp_pc_video_load_retail_textures(AvpPcVideo *v,const AvpRomImage *rom)
{
#ifdef AVP_LOCAL_MULTIPLAYER
    local_mp_view_release(v); /* Borrowed assets may be replaced by ROM loader. */
#endif
    if(!v)return 0;
    map_editor_release(v); /* Borrowed ROM assets are about to be reloaded. */
    avp_pc_prototype_textures_destroy(&v->prototype_textures);
    {int count=avp_gpu_assets_load_retail(&v->gpu,rom);
     avp_pc_prototype_textures_capture(&v->prototype_textures,&v->gpu);
     prototype_texture_update(v);
     return count;}
}

static void setkey(AvpPcVideo*v,KeyCode code,KeySym sym,int down){
    if(code){v->key[code]=(unsigned char)(down?1:0);
             v->pressed_sym[code]=down?sym:NoSymbol;}
}
static int key(const AvpPcVideo*v,KeySym sym){
    if(!sym)return 0;
    for(unsigned i=1;i<256u;i++)
        if(v->key[i]&&v->pressed_sym[i]==sym)return 1;
    return 0;
}
#ifndef _WIN32
/* Only ask the X server after keyboard events; normal high-FPS render polls
 * do not incur an XQueryKeymap round trip. This recovers a held W/A/S/D or
 * arrow key after a false release from an X11 repeat implementation. */
static void reconcile_physical_keyboard(AvpPcVideo *v)
{
    char physical[32];
    if(!v||!v->dpy||!v->mouse_focused)return;
    XQueryKeymap(v->dpy,physical);
    for(unsigned code=8;code<256u;code++){
        const int down=avp_pc_x11_physical_down(physical,code);
        const unsigned long sym=(down&&!v->key[code])?
            (unsigned long)XkbKeycodeToKeysym(v->dpy,(KeyCode)code,0,0):0u;
        unsigned long stored=(unsigned long)v->pressed_sym[code];
        avp_pc_x11_reconcile_key(&v->key[code],&stored,down,sym);
        v->pressed_sym[code]=(KeySym)stored;
    }
}
#endif
/* R264: X11 can deliver a complete press/release pair in one poll.  The
 * end-of-poll state is then released, so the previous poll-only buffer misses
 * the press.  Preserve real press events, only for the opt-in enhancement. */
static int short_event_buffer_allowed(const AvpPcVideo *v)
{
    return v->cfg.buffer_short_presses&&v->mouse_focused&&!v->menu_open&&!v->frontend_input&&
           !v->capture_mode&&!avp_computer_active()&&
           !avp_jaguar_pause_op_active(&v->gpu.pause_op);
}
static u32 short_key_press_bits(const AvpPcVideo *v,KeySym k)
{
    u32 bits=0;
#define ADD_KEY(sym,bit) do{if((sym)!=0 && k==(KeySym)(sym))bits|=(1u<<(bit));}while(0)
    if(v->cfg.controller_mode_jaguar){
        if(k==XK_Up||k==XK_w)bits|=1u<<JOY_UP;
        if(k==XK_Down||k==XK_s)bits|=1u<<JOY_DOWN;
        if(k==XK_Left||k==XK_a)bits|=1u<<JOY_LEFT;
        if(k==XK_Right||k==XK_d)bits|=1u<<JOY_RIGHT;
        if(k==XK_Return||k==XK_space||k==XK_z)bits|=1u<<FIRE_A;
        if(k==XK_x||k==XK_Shift_L||k==XK_Shift_R)bits|=1u<<FIRE_B;
        if(k==XK_c)bits|=1u<<FIRE_C;
    }else{
        ADD_KEY(v->cfg.key_forward,JOY_UP);
        ADD_KEY(v->cfg.key_back,JOY_DOWN);
        ADD_KEY(v->cfg.key_strafe_left,JOY_LEFT);
        ADD_KEY(v->cfg.key_strafe_right,JOY_RIGHT);
        if(v->cfg.key_turn_left && k==(KeySym)v->cfg.key_turn_left)
            bits|=(1u<<JOY_LEFT)|(1u<<(player_type==PT_ALIEN?OPTION:FIRE_C));
        if(v->cfg.key_turn_right && k==(KeySym)v->cfg.key_turn_right)
            bits|=(1u<<JOY_RIGHT)|(1u<<(player_type==PT_ALIEN?OPTION:FIRE_C));
        ADD_KEY(v->cfg.key_fire,FIRE_A);
        if(player_type!=PT_ALIEN)ADD_KEY(v->cfg.key_use,FIRE_B);
        if(player_type==PT_ALIEN){
            static const unsigned attack_bits[3]={FIRE_A,FIRE_B,FIRE_C};
            for(int i=0;i<3;i++)ADD_KEY(v->cfg.action_key[2+i],attack_bits[i]);
        }
        if(player_type==PT_PREDATOR){
            ADD_KEY(v->cfg.key_predator_medkit,KEY_7);
            ADD_KEY(v->cfg.key_predator_cloak,OPTION);
            ADD_KEY(v->cfg.key_predator_vision,KEY_6);
        }
    }
    ADD_KEY(v->cfg.key_pause,PAUSE);
    ADD_KEY(v->cfg.key_option,OPTION);
    ADD_KEY(v->cfg.key_map,KEY_8);
    if(k>=XK_0&&k<=XK_9){
        static const unsigned keypad_bits[10]={KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,
                                                KEY_5,KEY_6,KEY_7,KEY_8,KEY_9};
        bits|=1u<<keypad_bits[k-XK_0];
    }
    if(k==XK_asterisk)bits|=1u<<KEY_STAR;
    if(k==XK_numbersign)bits|=1u<<KEY_HASH;
#undef ADD_KEY
    return bits;
}
static void short_key_press(AvpPcVideo *v,KeySym k)
{
    if(!short_event_buffer_allowed(v))return;
    /* Opposite taps inside one 10-Hz tick are sequential commands, not a
     * physically impossible simultaneous Jaguar D-pad press. Prefer the last
     * press, and let a genuinely held left+right pair neutralize below. */
    if(!v->cfg.controller_mode_jaguar){
        if(v->cfg.key_turn_left && k==(KeySym)v->cfg.key_turn_left)
            v->pad_buffered&=~(1u<<JOY_RIGHT);
        else if(v->cfg.key_turn_right && k==(KeySym)v->cfg.key_turn_right)
            v->pad_buffered&=~(1u<<JOY_LEFT);
    }else{
        if(k==XK_Left||k==XK_a)v->pad_buffered&=~(1u<<JOY_RIGHT);
        else if(k==XK_Right||k==XK_d)v->pad_buffered&=~(1u<<JOY_LEFT);
    }
    v->pad_buffered|=short_key_press_bits(v,k);
    if(!v->cfg.controller_mode_jaguar && player_type==PT_ALIEN &&
       v->cfg.key_use && k==(KeySym)v->cfg.key_use)
        v->use_button_buffered=1;
}
static void short_mouse_press(AvpPcVideo *v,unsigned button)
{
    if(!short_event_buffer_allowed(v)||!v->mouse_grabbed)return;
    if(v->cfg.mouse_button[0]==(int)button)v->pad_buffered|=1u<<FIRE_A;
    if(v->cfg.mouse_button[1]==(int)button){
        if(player_type==PT_ALIEN && !v->cfg.controller_mode_jaguar)
            v->use_button_buffered=1;
        else if(player_type!=PT_ALIEN)v->pad_buffered|=1u<<FIRE_B;
    }
    if(v->cfg.mouse_button[2]==(int)button)v->pad_buffered|=1u<<KEY_8;
    if(player_type==PT_PREDATOR){
        if(v->cfg.mouse_button[8]==(int)button)v->pad_buffered|=1u<<KEY_7;
        if(v->cfg.mouse_button[9]==(int)button)v->pad_buffered|=1u<<OPTION;
        if(v->cfg.mouse_button[10]==(int)button)v->pad_buffered|=1u<<KEY_6;
    }
}
static void apply_capture_key(AvpPcVideo*v,KeySym k){unsigned long*dst=NULL;switch(v->capture_target){case 13:dst=&v->cfg.key_forward;break;case 14:dst=&v->cfg.key_back;break;case 15:dst=&v->cfg.key_turn_left;break;case 16:dst=&v->cfg.key_turn_right;break;case 17:dst=&v->cfg.key_strafe_left;break;case 18:dst=&v->cfg.key_strafe_right;break;case 19:dst=&v->cfg.key_fire;break;case 20:dst=&v->cfg.key_use;break;case 21:dst=&v->cfg.key_map;break;case 22:dst=&v->cfg.key_noclip;break;case 81:dst=&v->cfg.key_pause;break;case 82:dst=&v->cfg.key_option;break;case 111:dst=&v->cfg.menu_key_up;break;case 112:dst=&v->cfg.menu_key_down;break;
    case 113:dst=&v->cfg.menu_key_left;break;case 114:dst=&v->cfg.menu_key_right;break;
    case 115:dst=&v->cfg.menu_key_accept;break;case 116:dst=&v->cfg.menu_key_back;break;
    case 161:dst=&v->cfg.menu_key_open;break;
    case 85:dst=&v->cfg.key_predator_medkit;break;case 86:dst=&v->cfg.key_predator_cloak;break;case 87:dst=&v->cfg.key_predator_vision;break;default:if(v->capture_target>=149&&v->capture_target<=152)dst=&v->cfg.quick_turn_keys[v->capture_target-149];else if(v->capture_target>=60&&v->capture_target<68)dst=&v->cfg.action_key[v->capture_target-60];break;}if(dst && !(v->capture_target==21&&k>=XK_1&&k<=XK_4))
    *dst=(unsigned long)k;
v->capture_mode=0;save_settings(v);}
static void set_jag_button_binding(AvpPcVideo*v,int target,int button){if(target==26)v->cfg.jag_up_button=button;else if(target==27)v->cfg.jag_down_button=button;else if(target==28)v->cfg.jag_left_button=button;else if(target==29)v->cfg.jag_right_button=button;else if(target==30)v->cfg.jag_a_button=button;else if(target==31)v->cfg.jag_b_button=button;else if(target==32)v->cfg.jag_c_button=button;else if(target==33)v->cfg.jag_pause_button=button;else if(target==34)v->cfg.jag_option_button=button;else if(target>=35&&target<=44)v->cfg.jag_keypad_button[target-35]=button;else if(target==45)v->cfg.jag_keypad_button[10]=button;else if(target==46)v->cfg.jag_keypad_button[11]=button;}
static int jag_binding_value(const AvpPcVideo*v,int target){if(target==26)return v->cfg.jag_up_button;if(target==27)return v->cfg.jag_down_button;if(target==28)return v->cfg.jag_left_button;if(target==29)return v->cfg.jag_right_button;if(target==30)return v->cfg.jag_a_button;if(target==31)return v->cfg.jag_b_button;if(target==32)return v->cfg.jag_c_button;if(target==33)return v->cfg.jag_pause_button;if(target==34)return v->cfg.jag_option_button;if(target>=35&&target<=44)return v->cfg.jag_keypad_button[target-35];if(target==45)return v->cfg.jag_keypad_button[10];if(target==46)return v->cfg.jag_keypad_button[11];return -1;}
static void bind_button_token(AvpPcVideo*v,int token)
{
    int target=v->capture_target;
    if(target==10)v->cfg.fire_button=token;
    else if(target==11)v->cfg.use_button=token;
    else if(target==12)v->cfg.noclip_button=token;
    else if(target==47){
        int *jag[]={&v->cfg.jag_up_button,&v->cfg.jag_down_button,
                    &v->cfg.jag_left_button,&v->cfg.jag_right_button,
                    &v->cfg.jag_a_button,&v->cfg.jag_b_button,&v->cfg.jag_c_button,
                    &v->cfg.jag_pause_button,&v->cfg.jag_option_button};
        v->cfg.map_button=token;
        for(unsigned j=0;j<sizeof(jag)/sizeof(jag[0]);j++)if(*jag[j]==token)*jag[j]=-1;
        for(int j=0;j<12;j++)if(v->cfg.jag_keypad_button[j]==token)v->cfg.jag_keypad_button[j]=-1;
    }else if(target>=68&&target<76)v->cfg.action_button[target-68]=token;
    else if(target==83)v->cfg.strafe_left_button=token;
    else if(target==84)v->cfg.strafe_right_button=token;
    else if(target==93)v->cfg.forward_button=token;
    else if(target==94)v->cfg.backward_button=token;
    else if(target==88)v->cfg.predator_medkit_button=token;
    else if(target==89)v->cfg.predator_cloak_button=token;
    else if(target==90)v->cfg.predator_vision_button=token;
    else if(target==91)v->cfg.pc_pause_button=token;
    else if(target==92)v->cfg.pc_option_button=token;
    else if(target>=153&&target<=156)v->cfg.quick_turn_buttons[target-153]=token;
#ifdef AVP_LOCAL_MULTIPLAYER
    else if(target==164||target==165)v->local_mp_controls.player[0].gamepad_button[
        target==164?AVP_MP_MAP_ZOOM_IN:AVP_MP_MAP_ZOOM_OUT]=token;
#endif
    else if(target>=26&&target<=46)set_jag_button_binding(v,target,token);
    else if(target==117)v->cfg.menu_button_up=token;
    else if(target==118)v->cfg.menu_button_down=token;
    else if(target==119)v->cfg.menu_button_left=token;
    else if(target==120)v->cfg.menu_button_right=token;
    else if(target==121)v->cfg.menu_button_accept=token;
    else if(target==122)v->cfg.menu_button_back=token;
    else if(target==162)v->cfg.menu_button_open=token;
    v->capture_mode=0;save_settings(v);
}
/* Delete unbinds the selected assignment without hijacking Backspace's
 * F12 back-navigation role.  Applies during capture and on binding rows. */
static void clear_binding(AvpPcVideo*v,int target)
{
    int saved_mode=v->capture_mode,saved_target=v->capture_target;
    v->capture_target=target;
    if(target>=98&&target<98+AVP_MOUSE_ACTIONS)v->cfg.mouse_button[target-98]=-1;
    else if(target==4)v->cfg.move_axis=-1;
    else if(target==6)v->cfg.strafe_axis=-1;
    else if(target==8)v->cfg.turn_axis=-1;
    else if((target>=13&&target<=22)||(target>=60&&target<=67)||target==81||target==82||(target>=85&&target<=87)||
            (target>=111&&target<=116)||target==161||(target>=149&&target<=152))apply_capture_key(v,0);
    else if((target>=10&&target<=12)||(target>=26&&target<=47)||(target>=68&&target<=75)||(target>=83&&target<=84)||(target>=88&&target<=94)||(target>=117&&target<=122)||target==162||(target>=153&&target<=156)||target==164||target==165)bind_button_token(v,-1);
    v->capture_mode=0;
    v->capture_target=saved_target;
    (void)saved_mode;
    save_settings(v);
}
/* Delete clears only a binding belonging to the active F12 menu.  The
 * capture target takes precedence over the highlighted row; a menu category
 * or unbindable option never changes its configuration. */
static int f12_delete_binding(AvpPcVideo *v)
{
    int target;
    if(!v->menu_open)return 0;
    /* The Map Maker canvas owns Delete: it erases the selected cell in 2D
     * and removes an authored object in 3D; it is not a F12 binding row. */
    if(v->menu_page==MENU_MAP_MAKER)return 0;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->menu_page==MENU_MP_CONTROLS){
        if((v->local_mp_control_player!=0 ||
           v->menu_cursor>=AVP_MP_CONTROL_BIND_ROW+AVP_MP_MAP_ZOOM_IN) &&
           v->menu_cursor>=AVP_MP_CONTROL_BIND_ROW && v->menu_cursor<AVP_MP_CONTROL_BIND_ROW+(int)AVP_MP_INPUT_ACTIONS){
            unsigned a=(unsigned)(v->menu_cursor-AVP_MP_CONTROL_BIND_ROW);AvpMpInputProfile *p=&v->local_mp_controls.player[v->local_mp_control_player];
            if(v->local_mp_control_kind==0)p->key[a]=0;
            else if(v->local_mp_control_kind==1)p->mouse_button[a]=-1;
            else p->gamepad_button[a]=-1;
            v->capture_mode=0;save_settings(v);
        }
        return 1;
    }
#endif
    target=v->capture_mode?v->capture_target:menu_id(v,v->menu_cursor);
    if(target>=0)clear_binding(v,target);
    return 1;
}
#ifdef AVP_LOCAL_MULTIPLAYER
static void mp_controls_adjust(AvpPcVideo *v,int row,int dir,int accept);
#endif
static void capture_gamepad(AvpPcVideo*v)
{
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->capture_mode==7 && v->menu_open && v->menu_page==MENU_MP_CONTROLS){
        unsigned p=v->local_mp_control_player;
        const char *path=v->local_mp_controls.player[p].gamepad_path;
        AvpMpPadReader *r=&v->local_mp_pad_readers[p];
        AvpMpControllerSample sample={0};
        uint32_t edges;
        if(p==0u){
            /* P1's zoom binding belongs to the same joystick the original
             * single-player controls already own; no duplicate device. */
            for(int b=0;b<v->js_buttons_count;b++)
                if(v->js_buttons[b] && !v->js_prev_buttons[b]){
                    v->local_mp_controls.player[0].gamepad_button[v->capture_target]=b;
                    v->capture_mode=0;save_settings(v);return;
                }
            for(int a=0;a<v->js_axes_count;a++){
                int cur=v->js_axes[a],prev=v->js_prev_axes[a];
                if((cur>=23000||cur<=-23000) && prev>-12000&&prev<12000){
                    v->local_mp_controls.player[0].gamepad_button[v->capture_target]=
                        avp_pc_axis_button_token(a,cur>0);
                    v->capture_mode=0;save_settings(v);return;
                }
            }
            return;
        }
        if(!path[0])return;
        if(strcmp(r->path,path)){
            if(r->path[0])avp_mp_pad_reader_close(r);
            if(!avp_mp_pad_reader_open(r,path))return;
        }
        if(avp_mp_pad_reader_poll(r,&sample)<0)return;
        edges=sample.buttons&~v->local_mp_capture_prev_buttons;
        v->local_mp_capture_prev_buttons=sample.buttons;
        if(edges){
            unsigned b;
            for(b=0;b<32u;b++)if(edges&(1u<<b))break;
            if(b<32u){
                v->local_mp_controls.player[p].gamepad_button[v->capture_target]=(int)b;
                v->capture_mode=0;
                save_settings(v);
            }
        }
        /* Player 1 already supports an axis direction as a button. Match
         * that exact binding format for P2-P4 and only capture a NEW axis
         * movement, not an axis held when Enter was pressed. */
        for(unsigned a=0;a<sample.axis_count && a<AVP_MP_COMMAND_AXES;a++){
            int16_t current=sample.axes[a],previous=v->local_mp_capture_prev_axes[a];
            if((current>=23000 || current<=-23000) &&
               previous>-12000 && previous<12000){
                v->local_mp_controls.player[p].gamepad_button[v->capture_target]=
                    avp_pc_axis_button_token((int)a,current>0);
                v->capture_mode=0;
                save_settings(v);
                return;
            }
        }
        return;
    }
#endif
    if(!v->capture_mode)return;
    if(v->capture_mode==2){
        for(int i=0;i<v->js_axes_count;i++){
            int a=v->js_axes[i];
            if(abs(a)>20000&&abs(v->js_prev_axes[i])<12000){
                if(v->capture_target==4)v->cfg.move_axis=i;
                else if(v->capture_target==6)v->cfg.strafe_axis=i;
                else if(v->capture_target==8)v->cfg.turn_axis=i;
                v->capture_mode=0;save_settings(v);return;
            }
        }
    }else if(v->capture_mode==1){
        for(int i=0;i<v->js_buttons_count;i++)
            if(v->js_buttons[i]&&!v->js_prev_buttons[i]){bind_button_token(v,i);return;}
        for(int i=0;i<v->js_axes_count;i++){
            int a=v->js_axes[i],prev=v->js_prev_axes[i];
            if(abs(a)>23000&&abs(prev)<12000){
                bind_button_token(v,avp_pc_axis_button_token(i,a>0));return;
            }
        }
    }
}
/* Focus loss cancels queued *short* host inputs. The existing FocusOut
 * handler already released held keyboard keys and the captured mouse; the
 * optional press/analog buffers must not replay those old inputs on the
 * first simulation tick after focus has gone. Do not alter Jaguar pad logic. */
static void map_maker_enter_finish(AvpPcVideo *v);
static void clear_focus_input(AvpPcVideo *v)
{
#ifdef AVP_LOCAL_MULTIPLAYER
    avp_mp_commands_clear(&v->local_mp_commands);
#endif
    v->mouse_focused=0;
    memset(v->key,0,sizeof(v->key));
    memset(v->pressed_sym,0,sizeof(v->pressed_sym));
    v->pad_buffered=0;
    v->mm_drag_paint=0;
    v->mm_select_drag_active=0;
    v->mm_select_3d_pending=0;v->mm_select_3d_drag_active=0;
    v->mm_select_drag_last_ns=0;
    map_maker_enter_finish(v);
    avp_mm_history_commit(&v->map_maker);
    avp_mm_wall_drag_end(&v->map_maker);
    v->map_maker.wall_hover_valid=0;
    v->quick_turn_pending=0;
    v->use_button_buffered=0;
    v->use_button_held=0;
    v->previous_poll_pad=0xffffffffu;
    avp_pc_analog_buffer_reset(&v->analog_buffer);
    mouse_grab_update(v);
}
#ifdef AVP_LOCAL_MULTIPLAYER
int avp_pc_video_local_mp_bots_may_think(const AvpPcVideo *v)
{
    return v && (v->local_mp.enabled || v->local_mp_walktest) &&
        !v->frontend_input && !v->menu_open && !v->local_mp_open &&
        !v->local_mp_paused;
}
/* Host-frame look reads the same per-player controller reader as the
 * 10-Hz command sampler. Nonblocking; no independent joystick ownership,
 * keyboard side effects, or duplication of P1's legacy device. */
int avp_pc_video_local_mp_live_turn(AvpPcVideo *v,unsigned player,float *axis)
{
    const AvpMpInputProfile *p;
    AvpMpPadReader *r;
    AvpMpControllerSample sample={0};
    int index;
    if(axis)*axis=0.0f;
    if(!v||!axis||player==0||player>=AVP_MP_INPUT_PLAYERS ||
       !v->local_mp_walktest || v->frontend_input || v->menu_open ||
       v->local_mp_open || v->local_mp_paused || !v->mouse_focused ||
       avp_computer_active())return 0;
    p=&v->local_mp_controls.player[player];
    if(!p->gamepad_enabled||!p->gamepad_analog_movement||
       !p->gamepad_enhanced_analog_controls||!p->gamepad_path[0])return 0;
    index=p->gamepad_axis[2];
    if(index<0||(unsigned)index>=AVP_MP_COMMAND_AXES)return 0;
    r=&v->local_mp_pad_readers[player];
    if(strcmp(r->path,p->gamepad_path)){
        if(r->path[0])avp_mp_pad_reader_close(r);
        if(!avp_mp_pad_reader_open(r,p->gamepad_path))return 0;
    }
    if(avp_mp_pad_reader_poll(r,&sample)<0)return 0;
    v->local_mp_commands.gamepads[player]=sample;
    if((unsigned)index>=sample.axis_count)return 0;
    *axis=avp_pc_axis_deadzone(sample.axes[index],p->gamepad_deadzone,
                              p->gamepad_axis_invert[2]);
    return 1;
}
int avp_pc_video_local_mp_take_commands(AvpPcVideo *v,unsigned players,
    AvpMpPlayerCommand out[AVP_LOCAL_MP_MAX_PLAYERS])
{
    unsigned p,i;
    if(!v||!out||players<1||players>AVP_LOCAL_MP_MAX_PLAYERS||
       !(v->local_mp.enabled||v->local_mp_walktest)||v->frontend_input||v->menu_open||
       v->local_mp_open||!v->mouse_focused) {
        if(v) {
            unsigned i;
            avp_mp_commands_clear(&v->local_mp_commands);
            /* Drop queued raw motion on pause, focus loss or menu entry.
             * Reopen a fresh event stream after gameplay resumes. */
            for(i=0;i<AVP_MP_INPUT_PLAYERS;i++)
                if(v->local_mp_readers[i].fd>=0)
                    avp_mp_mouse_reader_close(&v->local_mp_readers[i]);
            for(i=0;i<AVP_MP_INPUT_PLAYERS;i++)
                if(v->local_mp_pad_readers[i].path[0])
                    avp_mp_pad_reader_close(&v->local_mp_pad_readers[i]);
        }
        return 0;
    }
    /* The normal X11/Win32 pressed-key table is still the one source of
     * held-key truth; snapshots here do not synthesize keyboard repeats. */
    for(i=0;i<AVP_MP_COMMAND_KEY_SLOTS;i++)
        v->local_mp_commands.keysyms[i]=v->key[i]?
            (unsigned long)v->pressed_sym[i]:0ul;
    for(p=0;p<players;p++) {
        const char *assigned=v->local_mp_controls.player[p].mouse_path;
        AvpMpMouseReader *r=&v->local_mp_readers[p];
        if(!assigned[0] || !v->local_mp_controls.player[p].mouse_enabled){
            if(r->fd>=0)avp_mp_mouse_reader_close(r);
            memset(&v->local_mp_commands.mice[p],0,sizeof(v->local_mp_commands.mice[p]));
            continue;
        }
        if(strcmp(r->path,assigned)){
            if(r->fd>=0)avp_mp_mouse_reader_close(r);
            memset(&v->local_mp_commands.mice[p],0,sizeof(v->local_mp_commands.mice[p]));
            if(!avp_mp_mouse_reader_open(r,assigned))return 0;
        }
        if(avp_mp_mouse_reader_poll(r,&v->local_mp_controls,
                                  v->local_mp_commands.mice)<0){
            avp_mp_mouse_reader_close(r);
            memset(&v->local_mp_commands.mice[p],0,sizeof(v->local_mp_commands.mice[p]));
            return 0;
        }
    }
    /* P1 retains its original single-player controller. Independently poll
     * only explicitly assigned P2-4 devices, including Windows XInput slots.
     * A secondary assignment matching P1 is refused rather than doubled. */
    for(p=1;p<players;p++){
        const char *assigned=v->local_mp_controls.player[p].gamepad_path;
        AvpMpPadReader *r=&v->local_mp_pad_readers[p];
        if(!assigned[0] || !v->local_mp_controls.player[p].gamepad_enabled){
            if(r->path[0])avp_mp_pad_reader_close(r);
            memset(&v->local_mp_commands.gamepads[p],0,
                   sizeof(v->local_mp_commands.gamepads[p]));
            continue;
        }
        if((v->js_fd>=0 && !strcmp(v->js_path,assigned)) ||
           (v->cfg.gamepad_device[0] && !strcmp(v->cfg.gamepad_device,assigned)))
            return 0;
        if(strcmp(r->path,assigned)){
            if(r->path[0])avp_mp_pad_reader_close(r);
            memset(&v->local_mp_commands.gamepads[p],0,
                   sizeof(v->local_mp_commands.gamepads[p]));
            if(!avp_mp_pad_reader_open(r,assigned))return 0;
        }
        if(avp_mp_pad_reader_poll(r,&v->local_mp_commands.gamepads[p])<0)
            return 0;
    }
    if(!avp_mp_commands_take(&v->local_mp_controls,&v->local_mp_commands,
                             players,out))return 0;
    /* P1's gamepad is already polled by the legacy host. Never open it a
     * second time: read the two new co-op zoom bindings from its live state. */
    if(v->cfg.gamepad_enabled && v->js_fd>=0){
        const AvpMpInputProfile *p1=&v->local_mp_controls.player[0];
        for(unsigned a=AVP_MP_MAP_ZOOM_IN;a<=AVP_MP_MAP_ZOOM_OUT;a++)
            if(button_down(v,p1->gamepad_button[a])){
                out[0].actions|=1u<<a;
                out[0].digital_actions|=1u<<a;
            }
    }
    return 1;
}
/* S116 preserves the finished multiplayer composite before running the
 * synchronous terminal. Never let the single-player presenter scale over it. */
int avp_pc_video_local_mp_terminal_begin(AvpPcVideo *v,unsigned owner,unsigned players)
{
    size_t count;
    uint32_t *picture;
    if(!v||!v->local_mp_walktest||!v->present_pixels||
       !v->present_w||!v->present_h||owner>=players||players<2u||players>4u||
       v->local_mp_terminal_visible||
       v->present_h>SIZE_MAX/sizeof(uint32_t)/v->present_w)return 0;
    count=(size_t)v->present_w*v->present_h;
    picture=(uint32_t*)malloc(count*sizeof(uint32_t));
    if(!picture)return 0;
    memcpy(picture,v->present_pixels,count*sizeof(uint32_t));
    v->local_mp_terminal_background=picture;
    v->local_mp_terminal_width=v->present_w;
    v->local_mp_terminal_height=v->present_h;
    v->local_mp_terminal_owner=owner;
    v->local_mp_terminal_visible=1;
    return 1;
}
void avp_pc_video_local_mp_terminal_end(AvpPcVideo *v)
{
    if(!v)return;
    if(v->local_mp_terminal_visible&&v->present_pixels&&
       v->local_mp_terminal_background&&v->present_w&&v->present_h){
        avp_pc_scale_nearest32(v->local_mp_terminal_background,
            v->local_mp_terminal_width,v->local_mp_terminal_height,
            v->present_pixels,v->present_w,v->present_h);
    }
    free(v->local_mp_terminal_background);
    v->local_mp_terminal_background=NULL;
    v->local_mp_terminal_width=v->local_mp_terminal_height=0;
    v->local_mp_terminal_owner=0;
    v->local_mp_terminal_visible=0;
}
/* Original COMPUTER.S is synchronous. Its VSync/readpad callbacks sample
 * only the actor that activated this terminal. Other players are frozen. */
int avp_pc_video_local_mp_terminal_poll(AvpPcVideo *v,unsigned owner,
                                       unsigned players,u32 *pad,int *quit)
{
    AvpMpPlayerCommand cmd[AVP_LOCAL_MP_MAX_PLAYERS]={{0}};
    if(!v||!pad||!quit||owner>=players || players>AVP_LOCAL_MP_MAX_PLAYERS)
        return 0;
    if(owner==0u){
        avp_pc_video_set_frontend_input(v,1);
        avp_pc_video_poll(v,pad,quit);
        avp_pc_video_set_frontend_input(v,0);
        return 1;
    }
    /* The original host P1 pad is deliberately ignored for P2-P4. */
    avp_pc_video_poll(v,pad,quit);
    *pad=UINT32_MAX;
    if(*quit)return 1;
    if(avp_pc_video_local_mp_take_commands(v,players,cmd))
        *pad=avp_mp_terminal_actions_pad(cmd[owner].actions);
    /* Escape is C/EXIT here, not Quit Game, including with an unassigned pad. */
    if(key(v,XK_Escape))*pad&=~(1u<<FIRE_C);
    return 1;
}
unsigned avp_pc_video_take_local_mp_cheat_actions(AvpPcVideo *v,unsigned player)
{
    unsigned flags;
    if(!v||player>=AVP_LOCAL_MP_MAX_PLAYERS)return 0u;
    flags=v->local_mp_cheat_actions[player];
    v->local_mp_cheat_actions[player]=0;
    return flags;
}
int avp_pc_video_local_mp_menu_open(const AvpPcVideo *v)
{
    return v && v->local_mp_open && (v->local_mp.enabled || v->local_mp_walktest) &&
        (v->local_mp_walktest ||
         (v->frontend_input && v->gpu.frontend_mode==AVP_MJP_TITLE));
}
int avp_pc_video_title_mp_menu_open(const AvpPcVideo *v)
{
    /* The frontend_input flag is only set during host event polling.
     * MJP checks the idle timer after that polling has finished. */
    return v && v->local_mp_open && (v->local_mp.enabled || v->local_mp_walktest) &&
           v->gpu.frontend_mode==AVP_MJP_TITLE;
}
int avp_pc_video_local_mp_launch_pending(const AvpPcVideo *v)
{
    return v && v->local_mp_launch_pending;
}
int avp_pc_video_take_local_mp_launch(AvpPcVideo *v,AvpLocalMpSetup *out)
{
    if(!out || !avp_pc_video_local_mp_launch_pending(v))return 0;
    /* A running co-op session may have had F12 > Local Multiplayer
     * disabled. Starting another session from F2 explicitly opts back in. */
    if(!v->local_mp.enabled){v->local_mp.enabled=1;save_settings(v);}
    *out=v->local_mp;
    avp_local_mp_validate(out);
    v->local_mp_launch_pending=0;
    return 1;
}
#endif
/* Bracket only explicit edits, not every host refresh/hover frame. */
#define AVP_MM_RECORD(m, action) do {\
    avp_mm_history_begin(m); (void)(action); avp_mm_history_commit(m); \
} while(0)
/* Original guarded paste resolves level-specific wall/flat texture tables. */
static int map_maker_paste(AvpMapMaker *m,unsigned x,unsigned y,unsigned field);
static int map_maker_3d_paste(AvpMapMaker *m,unsigned x,unsigned y,
                              unsigned field,void *unused)
{
    (void)unused;
    return map_maker_paste(m,x,y,field);
}
static void map_maker_enter_3d_target(AvpPcVideo *v)
{
    AvpMapMaker *m;
    if(!v||!v->mm_enter_paint_active)return;
    m=&v->map_maker;
    if(!m->mode_3d||m->palette_open||m->context_open||m->browser_open||
       m->name_edit_open||!m->has_copied_texture)return;
    (void)avp_mm_paint_3d_target_once(m,v->mm_enter_painted,
                sizeof v->mm_enter_painted,map_maker_3d_paste,NULL);
}
/* Hold Return to paint as the cell cursor moves (arrows or mouse).
 * One undo transaction covers the entire key hold. Revisited cells are
 * skipped, preventing duplicate object placement or accidental cycling. */
static void map_maker_enter_finish(AvpPcVideo *v)
{
    if(!v||!v->mm_enter_paint_active)return;
    v->mm_enter_paint_active=0;
    avp_mm_history_commit(&v->map_maker);
}
static void map_maker_enter_cell(AvpPcVideo *v,unsigned x,unsigned y,unsigned side)
{
    AvpMapMaker *m=&v->map_maker;
    unsigned bit;
    if(!v->mm_enter_paint_active||m->mode_3d||x>=m->w||y>=m->h)return;
    bit=y*AVP_MM_DIM+x;
    if(v->mm_enter_painted[bit/8u]&(1u<<(bit%8u)))return;
    v->mm_enter_painted[bit/8u]|=(unsigned char)(1u<<(bit%8u));
    (void)avp_mm_paint_2d_cell(m,x,y,side);
}
static void map_maker_enter_selection(AvpPcVideo *v)
{
    AvpMapMaker *m=&v->map_maker;
    unsigned cx=m->x,cy=m->y,side=m->field;
    unsigned ax=m->selection_2d_range?m->select_anchor_x:cx;
    unsigned ay=m->selection_2d_range?m->select_anchor_y:cy;
    unsigned x0=ax<cx?ax:cx,x1=ax>cx?ax:cx;
    unsigned y0=ay<cy?ay:cy,y1=ay>cy?ay:cy;
    for(unsigned y=y0;y<=y1;y++)for(unsigned x=x0;x<=x1;x++)
        map_maker_enter_cell(v,x,y,side);
    m->x=cx;m->y=cy;m->field=side;
}
static void map_maker_enter_line(AvpPcVideo *v,unsigned x,unsigned y,unsigned side)
{
    int x0,y0,x1=(int)x,y1=(int)y,dx,dy,sx,sy,err;
    if(!v->mm_enter_paint_active)return;
    x0=v->mm_enter_last_valid?(int)v->mm_enter_last_x:x1;
    y0=v->mm_enter_last_valid?(int)v->mm_enter_last_y:y1;
    dx=abs(x1-x0);dy=abs(y1-y0);
    sx=x0<x1?1:-1;sy=y0<y1?1:-1;err=dx-dy;
    for(;;){
        map_maker_enter_cell(v,(unsigned)x0,(unsigned)y0,side);
        if(x0==x1&&y0==y1)break;
        {int e=err*2;
         if(e>-dy){err-=dy;x0+=sx;}
         if(e<dx){err+=dx;y0+=sy;}}
    }
    v->mm_enter_last_valid=1;v->mm_enter_last_x=x;v->mm_enter_last_y=y;
}
static void map_maker_enter_start(AvpPcVideo *v)
{
    if(v->mm_enter_paint_active)return;
    memset(v->mm_enter_painted,0,sizeof v->mm_enter_painted);
    v->mm_enter_last_valid=0;
    avp_mm_history_begin(&v->map_maker);
    v->mm_enter_paint_active=1;
    if(v->map_maker.mode_3d){
        (void)avp_mm_pick(&v->map_maker,
                          v->map_maker.cursor_u,v->map_maker.cursor_v);
        if(!v->map_maker.has_copied_texture)
            snprintf(v->map_maker.message,sizeof v->map_maker.message,
                     "Tab: copy texture, then hold Enter to paint faces.");
        map_maker_enter_3d_target(v);
    }else map_maker_enter_selection(v);
    /* The initial keyboard-selected tile is not the mouse's previous
     * position. First pointer motion starts a fresh segment. */
    v->mm_enter_last_valid=0;
}
/* Delete affects only the current editing layer. Space clears selection and
 * prevents Delete from silently acting on the last selected cell. */
static void map_maker_delete_selection(AvpMapMaker *m)
{
    if(!m)return;
    if(m->mode_3d){
        if(avp_mm_selected_object_count(m)){
            AVP_MM_RECORD(m,avp_mm_delete_selected_objects(m));
        }else AVP_MM_RECORD(m,map_maker_object_edit(m,0));
        return;
    }
    if(!m->selection_2d_valid){
        snprintf(m->message,sizeof m->message,"Nothing selected. Click an item first.");
        return;
    }
    AVP_MM_RECORD(m,avp_mm_delete_2d_selection(m));
}
/* 2D pointer target, consistent for hover preview and actual brush strokes.
 * Edge clicks snap to the nearest quarter-tile border; clicks in the center
 * use the current ,/. direction instead of guessing a different side. */
static int map_maker_wall_pointer_target(AvpPcVideo *v,int px,int py,
                                         unsigned *x,unsigned *y,unsigned *side)
{
    AvpMapMaker *m=&v->map_maker;
    int vx=((int)v->win_w-(int)v->present_w)/2;
    int vy=((int)v->win_h-(int)v->present_h)/2;
    unsigned nearest_edge;
    if(!avp_mm_overhead_hit(m,v->present_w,v->present_h,px-vx,py-vy,x,y))return 0;
    nearest_edge=avp_mm_overhead_edge(m,v->present_w,v->present_h,px-vx,py-vy);
    *side=nearest_edge<4u?nearest_edge:(m->field<4u?m->field:1u);
    return 1;
}
static int map_maker_wall_pointer(AvpPcVideo *v,int px,int py,int erase)
{
    AvpMapMaker *m=&v->map_maker;
    unsigned x,y,side;
    if(!m->wall_tool||m->mode_3d||m->palette_open||m->context_open||
       m->browser_open||m->name_edit_open)return 0;
    if(!map_maker_wall_pointer_target(v,px,py,&x,&y,&side))return 0;
    if(m->wall_drag_lock)side=m->wall_drag_side;
    m->wall_hover_valid=1;
    m->wall_hover_x=x;m->wall_hover_y=y;m->wall_hover_side=side;
    if(erase && m->selection_2d_valid && m->x==x && m->y==y && m->field==side){
        m->selection_2d_valid=0;
        m->wall_hover_valid=0;
        snprintf(m->message,sizeof m->message,"Wall deselected. No texture changed.");
        return 0;
    }
    (void)avp_mm_wall_stroke(m,x,y,side,erase);
    if(!erase)m->selection_2d_valid=1;
    return 1; /* dragging continues even across already-occupied edges */
}
/* X11 coordinates are window-relative, but the Jaguar image occupies
 * present_pixels centered inside that window. Using window dimensions in
 * the 3D ray and then drawing in present dimensions offset the crosshair. */
static int map_maker_pointer_uv(const AvpPcVideo *v,int px,int py,double *u,double *w)
{
    int vx,vy,x,y;
    if(!v||!u||!w||!v->present_w||!v->present_h)return 0;
    vx=((int)v->win_w-(int)v->present_w)/2;
    vy=((int)v->win_h-(int)v->present_h)/2;
    x=px-vx;y=py-vy;
    if(x<0||y<0||(unsigned)x>=v->present_w||(unsigned)y>=v->present_h)return 0;
    *u=((double)x+0.5)/(double)v->present_w;
    *w=((double)y+0.5)/(double)v->present_h;
    return 1;
}
static int map_maker_pick_pointer(AvpPcVideo *v,int px,int py)
{
    double u,w;
    return map_maker_pointer_uv(v,px,py,&u,&w)&&avp_mm_pick(&v->map_maker,u,w);
}
/* Add each visible editor surface/object passed over by Shift+LMB.
 * Object markers are independently pickable because staged AMP objects are
 * not native 3D sprites until Play Test. Intermediate ray samples prevent a
 * fast mouse movement from skipping thin wall faces. Never edits geometry. */
static void map_maker_3d_drag_sample(AvpPcVideo *v,int px,int py)
{
    AvpMapMaker *m=&v->map_maker;
    int vx=((int)v->win_w-(int)v->present_w)/2;
    int vy=((int)v->win_h-(int)v->present_h)/2;
    int oi=avp_mm_3d_object_at(m,v->present_w,v->present_h,px-vx,py-vy);
    if(oi>=0){(void)avp_mm_select_object(m,(unsigned)oi,1,0);return;}
    if(map_maker_pick_pointer(v,px,py))
        (void)avp_mm_select_target_drag(m);
}
static void map_maker_3d_drag_line(AvpPcVideo *v,int from_x,int from_y,
                                   int to_x,int to_y)
{
    int dx=to_x-from_x,dy=to_y-from_y;
    int maxdist=abs(dx)>abs(dy)?abs(dx):abs(dy);
    int steps=maxdist/4+1;
    if(steps>256)steps=256; /* bound event work regardless of cursor warps */
    for(int i=0;i<=steps;i++){
        int x=from_x+(int)((int64_t)dx*i/steps);
        int y=from_y+(int)((int64_t)dy*i/steps);
        map_maker_3d_drag_sample(v,x,y);
    }
    (void)map_maker_pick_pointer(v,to_x,to_y); /* restore current hover */
}
static int map_maker_3d_object_under_pointer(AvpPcVideo *v,int px,int py)
{
    int vx=((int)v->win_w-(int)v->present_w)/2;
    int vy=((int)v->win_h-(int)v->present_h)/2;
    return avp_mm_3d_object_at(&v->map_maker,v->present_w,v->present_h,
                                px-vx,py-vy);
}
#ifdef AVP_LOCAL_MULTIPLAYER
/* R56 F2 navigation: page changes never touch stable gameplay row IDs. */
static void mp_setup_open_page(AvpPcVideo *v,unsigned page)
{
    unsigned items[AVP_LOCAL_MP_MENU_ROWS];
    unsigned n=avp_local_mp_menu_rows(&v->local_mp,page,items);
    v->local_mp_page=page;
    v->local_mp_cursor=n?items[0]:AVP_LOCAL_MP_START_ROW;
    v->menu_repeat_direction=0;
}
static void mp_setup_back(AvpPcVideo *v)
{
    if(v->local_mp_page!=AVP_LOCAL_MP_MENU_ROOT){
        unsigned parent=v->local_mp_page;
        v->local_mp_page=AVP_LOCAL_MP_MENU_ROOT;
        v->local_mp_cursor=parent==AVP_LOCAL_MP_MENU_BOTS?
            AVP_LOCAL_MP_MENU_BOTS_ROW:parent==AVP_LOCAL_MP_MENU_MODIFIERS?
            AVP_LOCAL_MP_MENU_MODIFIERS_ROW:AVP_LOCAL_MP_MENU_OPTIONS_ROW;
        v->menu_repeat_direction=0;
    }else v->local_mp_open=0;
}
static void mp_setup_accept(AvpPcVideo *v)
{
    if(v->local_mp_page==AVP_LOCAL_MP_MENU_ROOT &&
       v->local_mp_cursor>=AVP_LOCAL_MP_MENU_BOTS_ROW &&
       v->local_mp_cursor<=AVP_LOCAL_MP_MENU_OPTIONS_ROW){
        mp_setup_open_page(v,v->local_mp_cursor-AVP_LOCAL_MP_MENU_BOTS_ROW+1u);
        return;
    }
    if(v->local_mp_cursor==AVP_LOCAL_MP_START_ROW){
        v->local_mp_launch_pending=1;
        v->local_mp_open=0;
        v->local_mp_suppress_input=1;
        memset(v->key,0,sizeof(v->key));
        memset(v->pressed_sym,0,sizeof(v->pressed_sym));
        mouse_grab_update(v);
    }else if(avp_local_mp_adjust(&v->local_mp,v->local_mp_cursor,1)){
        save_settings(v);
    }
}
#endif /* AVP_LOCAL_MULTIPLAYER */
static void mp_menu_repeat_poll(AvpPcVideo *v);
int avp_pc_video_poll(AvpPcVideo*v,u32 *pad,int *quit){
#ifndef _WIN32
    int keyboard_events_seen=0;
#endif
    poll_js(v);capture_gamepad(v);
    if(v->menu_open&&v->capture_mode==11){
        for(int b=0;b<v->js_buttons_count;b++)if(b<AVP_JS_MAX_BUTTONS&&
           v->js_buttons[b]&&!v->js_prev_buttons[b]){
            if(v->capture_target>=0&&v->capture_target<AVP_MM_ACTIONS){
                v->cfg.mm_button[v->capture_target]=b;save_settings(v);
            }v->capture_mode=0;break;
        }
        /* Match the main binding menu: a newly moved axis direction may
         * serve as a button without overriding a held stick on entry. */
        if(v->capture_mode==11){
            for(int a=0;a<v->js_axes_count&&a<AVP_JS_MAX_AXES;a++){
                int cur=v->js_axes[a],prev=v->js_prev_axes[a];
                if((cur>=23000||cur<=-23000)&&prev>-12000&&prev<12000){
                    if(v->capture_target>=0&&v->capture_target<AVP_MM_ACTIONS){
                        v->cfg.mm_button[v->capture_target]=avp_pc_axis_button_token(a,cur>0);
                        save_settings(v);
                    }
                    v->capture_mode=0;break;
                }
            }
        }
    }
    if(v->menu_open&&v->menu_page==MENU_MAP_MAKER&&!v->capture_mode&&!v->map_maker.palette_open){
        AvpMapMaker *m=&v->map_maker;
        if(button_edge(v,v->cfg.mm_button[MM_COPY])){
            if(m->mode_3d)(void)avp_mm_copy_target(m);
            else (void)avp_mm_copy_surface(m,m->x,m->y,m->edit_mode_2d==3u?avp_mm_texture_field(m):(m->overhead_ceiling?4u:5u));
        }
        if(button_edge(v,v->cfg.mm_button[MM_OBJECT_PLACE]))
            AVP_MM_RECORD(m,map_maker_object_edit(m,1));
        if(button_edge(v,v->cfg.mm_button[MM_OBJECT_REMOVE]))
            map_maker_delete_selection(m);
        if(button_edge(v,v->cfg.mm_button[MM_OBJECT_NEXT])){
            unsigned id=avp_mm_object_catalog_id(m->object_def);
            m->object_def=avp_mm_object_catalog_def((id+1u)%avp_mm_object_catalog_count());
        }
        if(button_edge(v,v->cfg.mm_button[MM_OBJECT_GROUP]))m->object_group=(m->object_group+1u)%4u;
        if(button_edge(v,v->cfg.mm_button[MM_PLAY_TEST]))map_maker_play_test(v);
        if(button_edge(v,v->cfg.mm_button[MM_LOOK_UP]))avp_mm_camera_pitch(m,0.08);
        if(button_edge(v,v->cfg.mm_button[MM_LOOK_DOWN]))avp_mm_camera_pitch(m,-0.08);
        if(button_edge(v,v->cfg.mm_button[MM_GHOST_WALLS]))
            avp_mm_toggle_ghost_walls(m);
        if(button_edge(v,v->cfg.mm_button[MM_HIGHLIGHT]))
            avp_mm_toggle_highlight(m);
        if(!m->mode_3d){
            if(button_edge(v,v->cfg.mm_button[MM_ROTATE_CCW]))
                AVP_MM_RECORD(m,(m->wall_tool?avp_mm_wall_turn(m,-1):avp_mm_rotate_wall(m,-1)));
            if(button_edge(v,v->cfg.mm_button[MM_ROTATE_CW]))
                AVP_MM_RECORD(m,(m->wall_tool?avp_mm_wall_turn(m,1):avp_mm_rotate_wall(m,1)));
        }
        if(button_edge(v,v->cfg.mm_button[MM_OVERHEAD_SURFACE])){
            if(!m->mode_3d&&m->edit_mode_2d==3u)avp_mm_cycle_texture_target(m);
            else avp_mm_toggle_overhead_surface(m);
        }
        /* A's generic menu-accept action must not toggle editing modes.
         * Keyboard: keypad Enter; gamepad: the explicitly bound MM_MODE. */
        if(button_edge(v,v->cfg.mm_button[MM_MODE]))
            avp_mm_toggle_3d(m);
        if(button_edge(v,v->cfg.mm_button[MM_PAINT])){
            if(m->mode_3d)AVP_MM_RECORD(m,map_maker_paste_target(m));
            else if(m->edit_mode_2d==3u)
                AVP_MM_RECORD(m,avp_mm_texture_paint_selection(m));
            else if(m->wall_tool && m->field<4u)
                AVP_MM_RECORD(m,avp_mm_wall_place(m,m->x,m->y,m->field));
            else AVP_MM_RECORD(m,avp_mm_place_floor(m));
        }
        if(m->mode_3d){
            if(button_edge(v,v->cfg.mm_button[MM_TEX_PLUS]))AVP_MM_RECORD(m,avp_mm_adjust_target(m,1));
            if(button_edge(v,v->cfg.mm_button[MM_TEX_MINUS]))AVP_MM_RECORD(m,avp_mm_adjust_target(m,-1));
        }
    }
    if(!v->capture_mode && button_edge(v,v->cfg.menu_button_open)){
#ifdef AVP_LOCAL_MULTIPLAYER
        v->local_mp_open=0;
#endif
        v->menu_open=!v->menu_open;v->menu_page=MENU_ROOT;v->menu_cursor=0;
        v->menu_index=0;v->menu_repeat_direction=0;
        v->pad_buffered=0;v->quick_turn_pending=0;
        memset(v->key,0,sizeof(v->key));memset(v->pressed_sym,0,sizeof(v->pressed_sym));
    }
    mouse_grab_update(v);while(XPending(v->dpy)){XEvent e;XNextEvent(v->dpy,&e);
#ifndef _WIN32
    /* Legacy X11 auto-repeat is a synthetic release/press pair with the
     * same keycode and timestamp. If detectable repeat is unavailable, never
     * let that pair release a held turning key for one render frame. */
    if(e.type==KeyRelease && XPending(v->dpy)){
        XEvent next;
        XPeekEvent(v->dpy,&next);
        if(next.type==KeyPress && next.xkey.keycode==e.xkey.keycode &&
           next.xkey.time==e.xkey.time)continue;
    }
#endif
    /* Keep desktop idle inhibition scoped to the mapped, focused game window. */
#ifndef _WIN32
    if(e.type==MapNotify && e.xmap.window==v->win)
        v->idle_window_mapped=1;
    else if(e.type==UnmapNotify && e.xunmap.window==v->win)
        v->idle_window_mapped=0;
    else if(e.type==FocusIn && e.xfocus.window==v->win &&
            e.xfocus.mode!=NotifyGrab && e.xfocus.mode!=NotifyUngrab)
        v->idle_window_focused=1;
    else if(e.type==FocusOut && e.xfocus.window==v->win &&
            e.xfocus.mode!=NotifyGrab && e.xfocus.mode!=NotifyUngrab)
        v->idle_window_focused=0;
#endif
    if(e.type==ClientMessage&&e.xclient.message_type==v->wm_protocols&&(Atom)e.xclient.data.l[0]==v->wm_delete)*quit=1;else if(e.type==DestroyNotify)*quit=1;else if(e.type==ConfigureNotify){if(e.xconfigure.width>0&&e.xconfigure.height>0){
        (void)resize_surface(v,(unsigned)e.xconfigure.width,(unsigned)e.xconfigure.height);
        /* A fullscreen/borderless resize can emit several asynchronous
         * ConfigureNotify events even after switching back to Windowed.
         * Never overwrite the user-selected windowed dimensions with the
         * monitor dimensions during that transition.  Window Size is
         * selected explicitly in Video; ResizeNotify affects only the
         * current rendering surface. */
        v->video_switch_pending=0;
    }}else if(e.type==FocusOut){v->mm_pan_active=0;v->mm_select_drag_active=0;v->mm_select_3d_pending=0;
        v->mm_select_3d_drag_active=0;v->mm_nav_shift_latched=0;clear_focus_input(v);}
    else if(e.type==FocusIn){v->mouse_focused=1;mouse_grab_update(v);}
    else if(e.type==MotionNotify && v->menu_open && v->menu_page==MENU_MAP_MAKER){
        AvpMapMaker *m=&v->map_maker;
        if(!m->context_open&&!m->browser_open&&!m->name_edit_open&&!m->palette_open){
            if(m->mode_3d){
                if(v->mm_select_3d_pending||v->mm_select_3d_drag_active){
                    int px=e.xmotion.x,py=e.xmotion.y;
                    if(!(e.xmotion.state&ShiftMask)||!(e.xmotion.state&Button1Mask)){
                        v->mm_select_3d_pending=0;v->mm_select_3d_drag_active=0;
                    }else{
                        int dx=px-v->mm_select_3d_start_x;
                        int dy=py-v->mm_select_3d_start_y;
                        if(v->mm_select_3d_drag_active || abs(dx)>3 || abs(dy)>3){
                            int fx=v->mm_select_3d_drag_active?v->mm_select_3d_last_x:
                                   v->mm_select_3d_start_x;
                            int fy=v->mm_select_3d_drag_active?v->mm_select_3d_last_y:
                                   v->mm_select_3d_start_y;
                            v->mm_select_3d_pending=0;v->mm_select_3d_drag_active=1;
                            map_maker_3d_drag_line(v,fx,fy,px,py);
                            v->mm_select_3d_last_x=px;v->mm_select_3d_last_y=py;
                        }
                    }
                }
                if(map_maker_pick_pointer(v,e.xmotion.x,e.xmotion.y))
                    map_maker_enter_3d_target(v);
            }else if(v->mm_select_drag_active){
                v->mm_select_drag_px=e.xmotion.x;
                v->mm_select_drag_py=e.xmotion.y;
                /* Shift+LMB always owns the stroke, even in Walls mode.
                 * Misses outside the clipped map leave the selection intact. */
                unsigned xx,yy;
                int vx=((int)v->win_w-(int)v->present_w)/2;
                int vy=((int)v->win_h-(int)v->present_h)/2;
                if(avp_mm_overhead_hit(m,v->present_w,v->present_h,
                                       e.xmotion.x-vx,e.xmotion.y-vy,&xx,&yy))
                    (void)avp_mm_update_2d_drag_selection(m,xx,yy);
            }else if(v->mm_pan_active){
                avp_mm_overhead_pan(m,v->present_w,v->present_h,
                    e.xmotion.x-v->mm_pan_last_x,e.xmotion.y-v->mm_pan_last_y);
                v->mm_pan_last_x=e.xmotion.x;
                v->mm_pan_last_y=e.xmotion.y;
            }else if(v->mm_enter_paint_active){
                unsigned xx,yy;
                int vx=((int)v->win_w-(int)v->present_w)/2;
                int vy=((int)v->win_h-(int)v->present_h)/2;
                if(avp_mm_overhead_hit(m,v->present_w,v->present_h,
                                       e.xmotion.x-vx,e.xmotion.y-vy,&xx,&yy)){
                    unsigned side=m->field<4u?m->field:1u;
                    map_maker_enter_line(v,xx,yy,side);
                    m->x=xx;m->y=yy;m->field=side;m->selection_2d_valid=1;
                    m->selection_2d_range=0;
                }else{
                    /* A pointer excursion outside the map is NOT a stroke
                     * across every cell between its departure and return. */
                    v->mm_enter_last_valid=0;
                }
            }else if(v->mm_drag_paint==2||v->mm_drag_paint==3){
                (void)map_maker_wall_pointer(v,e.xmotion.x,e.xmotion.y,v->mm_drag_paint==3);
            }else if(m->wall_tool){
                unsigned x,y,side;
                if(map_maker_wall_pointer_target(v,e.xmotion.x,e.xmotion.y,&x,&y,&side)){
                    m->wall_hover_valid=1;
                    m->wall_hover_x=x;m->wall_hover_y=y;m->wall_hover_side=side;
                }else m->wall_hover_valid=0;
            }else if(v->mm_drag_paint==1){
                unsigned x,y;
                int vx=((int)v->win_w-(int)v->present_w)/2;
                int vy=((int)v->win_h-(int)v->present_h)/2;
                if(avp_mm_overhead_hit(m,v->present_w,v->present_h,
                                       e.xmotion.x-vx,e.xmotion.y-vy,&x,&y)){
                    m->x=x;m->y=y;
                    (void)map_maker_paste(m,x,y,m->overhead_ceiling?4u:5u);
                }
            }
        }
    }else if(e.type==MotionNotify && v->mouse_grabbed){int cx=(int)v->win_w/2,cy=(int)v->win_h/2;
        if(e.xmotion.x==cx&&e.xmotion.y==cy&&v->mouse_recenter){
            /* Synthetic warp echo, not physical movement. */
            v->mouse_recenter=0;v->mouse_last_x=cx;
        }else{
            /* Sum successive physical motion deltas instead of repeatedly
               counting each absolute displacement from the screen center. */
            v->mouse_delta+=(float)(e.xmotion.x-v->mouse_last_x);
            v->mouse_last_x=e.xmotion.x;v->mouse_motion_seen=1;
        }
    }else if(e.type==ButtonPress||e.type==ButtonRelease){unsigned b=e.xbutton.button;if(b>0&&b<16){
#ifdef AVP_LOCAL_MULTIPLAYER
        if(v->capture_mode==8 && v->menu_open && v->menu_page==MENU_MP_CONTROLS &&
           e.type==ButtonPress){
            v->local_mp_controls.player[v->local_mp_control_player].mouse_button[v->capture_target]=(int)b;
            v->capture_mode=0;save_settings(v);
        }
        else
#endif
        if(v->menu_open&&v->capture_mode==10&&e.type==ButtonPress){
            if(v->capture_target>=0&&v->capture_target<AVP_MM_ACTIONS){
                v->cfg.mm_mouse[v->capture_target]=(int)b;save_settings(v);
            }v->capture_mode=0;
        }else if(v->menu_open&&v->menu_page==MENU_MAP_MAKER){
            AvpMapMaker *m=&v->map_maker;
            if(e.type==ButtonRelease){
                if(b==1u){
                    if(m->mode_3d && v->mm_select_3d_pending &&
                       !m->context_open&&!m->browser_open&&
                       !m->name_edit_open&&!m->palette_open){
                        int oi=map_maker_3d_object_under_pointer(v,
                              v->mm_select_3d_start_x,v->mm_select_3d_start_y);
                        if(oi>=0)(void)avp_mm_select_object(m,(unsigned)oi,1,1);
                        else if(map_maker_pick_pointer(v,v->mm_select_3d_start_x,
                                    v->mm_select_3d_start_y))
                            (void)avp_mm_select_target(m,1);
                    }
                    v->mm_select_3d_pending=0;v->mm_select_3d_drag_active=0;
                    v->mm_select_drag_active=0;v->mm_select_drag_last_ns=0;
                }
                if(b==2u)v->mm_pan_active=0;
                if((b==3u && (v->mm_drag_paint==1||v->mm_drag_paint==3)) ||
                   (b==1u && v->mm_drag_paint==2)){
                    v->mm_drag_paint=0;avp_mm_history_commit(m);avp_mm_wall_drag_end(m);
                }
            }else if(m->context_open||m->browser_open||m->name_edit_open){
                /* Modal Escape actions are keyboard-driven; never paint behind them. */
            }else if(m->palette_open){
                unsigned id=0;
                int vx=((int)v->win_w-(int)v->present_w)/2;
                int vy=((int)v->win_h-(int)v->present_h)/2;
                if(avp_mm_browser_hit(v->present_w,v->present_h,
                         e.xbutton.x-vx,e.xbutton.y-vy,m->palette_id/64u,&id)){
                    m->palette_id=id;
                    if(b==1){
                        avp_mm_history_begin(m);
                        unsigned field=m->mode_3d&&m->selected_count?
                  m->selected[m->selected_count-1u].field:
                  m->mode_3d&&m->target_valid?m->target_field:
                  m->edit_mode_2d==3u?avp_mm_texture_field(m):m->field;
                        if(!m->mode_3d && m->edit_mode_2d==3u){
                            avp_mm_texture_brush_select(m,(u8)id);
                        }else if(m->wall_tool&&!m->mode_3d&&field<4u){
                            avp_mm_wall_brush_select(m,(u8)id);
                            snprintf(m->message,sizeof m->message,"Wall brush ID %u selected. Click to place.",
                                     (unsigned)m->wall_brush);
                        }else{
                            int ok=m->mode_3d?avp_mm_assign_target(m,(u8)id):avp_mm_assign(m,(u8)id);
                            if(ok && field<4u)avp_mm_wall_brush_select(m,(u8)id);
                        }
                        m->palette_open=0;
                        avp_mm_history_commit(m);
                    }
                }
            }else if(m->mode_3d){
                int on_image=map_maker_pick_pointer(v,e.xbutton.x,e.xbutton.y);
                int oi=map_maker_3d_object_under_pointer(v,e.xbutton.x,e.xbutton.y);
                /* Normal click replaces selection. Shift+click toggles;
                 * Shift+drag adds unique faces/objects without painting. */
                if(b==1u){
                    if(e.xbutton.state&ShiftMask){
                        v->mm_select_3d_pending=1;v->mm_select_3d_drag_active=0;
                        v->mm_select_3d_start_x=e.xbutton.x;
                        v->mm_select_3d_start_y=e.xbutton.y;
                        v->mm_select_3d_last_x=e.xbutton.x;
                        v->mm_select_3d_last_y=e.xbutton.y;
                    }else if(oi>=0)(void)avp_mm_select_object(m,(unsigned)oi,0,0);
                    else if(on_image)(void)avp_mm_select_target(m,0);
                }else if(b==3u){
                    if(oi>=0){
                        if(m->selected_objects[oi])
                            (void)avp_mm_select_object(m,(unsigned)oi,1,1);
                    }else if(on_image)(void)avp_mm_deselect_target(m);
                }else if(b==4u||b==5u){
                    if(on_image)AVP_MM_RECORD(m,avp_mm_adjust_target(m,b==4u?1:-1));
                }else if((int)b==v->cfg.mm_mouse[MM_HIGHLIGHT]){
                    avp_mm_toggle_highlight(m);
                }else if((int)b==v->cfg.mm_mouse[MM_COPY]){
                    (void)avp_mm_copy_target(m);
                }else if((int)b==v->cfg.mm_mouse[MM_OBJECT_PLACE])AVP_MM_RECORD(m,map_maker_object_edit(m,1));
                else if((int)b==v->cfg.mm_mouse[MM_OBJECT_REMOVE])AVP_MM_RECORD(m,map_maker_object_edit(m,0));
                else if((int)b==v->cfg.mm_mouse[MM_MODE])avp_mm_toggle_3d(m);
                else if((int)b==v->cfg.mm_mouse[MM_TEX_PLUS])
                    AVP_MM_RECORD(m,avp_mm_adjust_target(m,1));
                else if((int)b==v->cfg.mm_mouse[MM_TEX_MINUS])
                    AVP_MM_RECORD(m,avp_mm_adjust_target(m,-1));
                /* MM_PAINT remains a keyboard/gamepad action, but never
                 * steals mouse left-click from 3D face selection. */
            }else if(b==1u && (e.xbutton.state&ShiftMask)){
                /* Start a selection rectangle, in EVERY 2D edit mode. This
                 * precedes wall drawing, so Shift+drag cannot place walls.
                 * Left and right Shift both set X11's ShiftMask. */
                unsigned xx,yy;
                int vx=((int)v->win_w-(int)v->present_w)/2;
                int vy=((int)v->win_h-(int)v->present_h)/2;
                if(!v->mm_enter_paint_active &&
                   avp_mm_overhead_hit(m,v->present_w,v->present_h,
                                       e.xbutton.x-vx,e.xbutton.y-vy,&xx,&yy)){
                    v->mm_select_drag_active=avp_mm_begin_2d_drag_selection(m,xx,yy);
                    v->mm_select_drag_px=e.xbutton.x;
                    v->mm_select_drag_py=e.xbutton.y;
                    v->mm_select_drag_last_ns=avp_pc_now_ns();
                }
            }else if(b==2u){
                /* MMB belongs to viewport panning, never selection/copy.
                 * Modal and 3D controls above retain their own bindings. */
                if(!v->mm_enter_paint_active&&!v->mm_drag_paint){
                    v->mm_pan_active=1;
                    v->mm_pan_last_x=e.xbutton.x;
                    v->mm_pan_last_y=e.xbutton.y;
                    avp_mm_overhead_pan(m,v->present_w,v->present_h,0,0);
                }
            }else if((int)b==v->cfg.mm_mouse[MM_ROTATE_CCW]){
                AVP_MM_RECORD(m,(m->wall_tool?avp_mm_wall_turn(m,-1):avp_mm_rotate_wall(m,-1)));
            }else if((int)b==v->cfg.mm_mouse[MM_ROTATE_CW]){
                AVP_MM_RECORD(m,(m->wall_tool?avp_mm_wall_turn(m,1):avp_mm_rotate_wall(m,1)));
            }else if(b==4u||b==5u){
                int vx=((int)v->win_w-(int)v->present_w)/2;
                int vy=((int)v->win_h-(int)v->present_h)/2;
                avp_mm_overhead_zoom_at(m,v->present_w,v->present_h,
                    e.xbutton.x-vx,e.xbutton.y-vy,b==4u?1:-1);
            }else if(m->wall_tool && (b==1u||b==3u)){
                avp_mm_history_begin(m);
                if(map_maker_wall_pointer(v,e.xbutton.x,e.xbutton.y,b==3u))
                    v->mm_drag_paint=b==1u?2:3;
                else avp_mm_history_commit(m);
            }else if(b==1u||b==3u){
                unsigned xx,yy;
                int vx=((int)v->win_w-(int)v->present_w)/2;
                int vy=((int)v->win_h-(int)v->present_h)/2;
                if(avp_mm_overhead_hit(m,v->present_w,v->present_h,
                                       e.xbutton.x-vx,e.xbutton.y-vy,&xx,&yy)){
                    if(b==3u && m->selection_2d_valid &&
                       m->x==xx && m->y==yy &&
                       (m->field<4u || m->edit_mode_2d==3u)){
                        avp_mm_clear_selection(m);
                        m->wall_hover_valid=0;
                        snprintf(m->message,sizeof m->message,"Wall deselected.");
                        continue;
                    }
                    m->x=xx;m->y=yy;m->selection_2d_range=0;
                    if(b==1u){
                        m->selection_2d_valid=1;
                        unsigned side=avp_mm_overhead_edge(m,v->present_w,v->present_h,
                                      e.xbutton.x-vx,e.xbutton.y-vy);
                        if(side<4u)m->field=side;
                    }
                    if(b==3u && m->edit_mode_2d==0u){
                        avp_mm_history_begin(m);
                        v->mm_drag_paint=1;
                        (void)map_maker_paste(m,xx,yy,m->overhead_ceiling?4u:5u);
                    }
                }
            }
        }else if(v->capture_mode==4&&e.type==ButtonPress){v->cfg.mouse_button[v->capture_target-98]=(int)b;v->capture_mode=0;save_settings(v);}
        else if(v->mouse_grabbed){if(e.type==ButtonPress&&!v->mouse_down[b])short_mouse_press(v,b);v->mouse_down[b]=(e.type==ButtonPress);if(e.type==ButtonPress&&!v->frontend_input){if(v->cfg.mouse_button[3]==(int)b)v->weapon_delta=-1;else if(v->cfg.mouse_button[4]==(int)b)v->weapon_delta=1;}}
    }}else if(e.type==KeyPress||e.type==KeyRelease){KeySym k=XLookupKeysym(&e.xkey,0);int d=e.type==KeyPress;int was=key(v,k);
#ifndef _WIN32
        keyboard_events_seen=1;
#endif
    if(!d && (k==XK_Shift_L||k==XK_Shift_R) &&
       !key(v,k==XK_Shift_L?XK_Shift_R:XK_Shift_L))
        v->mm_nav_shift_latched=0;
    if(v->menu_open && v->capture_mode==9){
        if(d&&!was){
            if(k!=XK_Escape&&v->capture_target>=0&&v->capture_target<AVP_MM_ACTIONS){
                v->cfg.mm_key[v->capture_target]=(unsigned long)(k==XK_Delete?0:k);
                save_settings(v);
            }
            v->capture_mode=0;
        }
        setkey(v,e.xkey.keycode,k,d);
        continue;
    }
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->menu_open && v->menu_page==MENU_MP_CONTROLS && v->capture_mode==6){
        /* Enter begins capture on KeyPress. Its KeyRelease arrives while
         * capture is active; consuming that release without updating the
         * held-key table leaves Enter stuck and blocks the next binding. */
        if(!d)setkey(v,e.xkey.keycode,k,0);
        if(d&&!was){
            if(k!=XK_Escape && k!=XK_BackSpace && k!=XK_Delete && k!=XK_F12){
                v->local_mp_controls.player[v->local_mp_control_player].key[v->capture_target]=(unsigned long)k;
                save_settings(v);
            } else if(k==XK_Delete){
                v->local_mp_controls.player[v->local_mp_control_player].key[v->capture_target]=0;
                save_settings(v);
            }
            v->capture_mode=0;
        }
        continue;
    }
#endif
    /* Alt+Enter is a host shortcut in both gameplay and menus; never a Jaguar input. */
    if(d&&!was&&(k==XK_Return||k==XK_KP_Enter) && (e.xkey.state & Mod1Mask)){
        v->cfg.video_window_mode=(v->cfg.video_window_mode==2u)?0u:2u;
        video_apply_display(v);save_settings(v);
        setkey(v,e.xkey.keycode,k,0);continue;
    }
    if(d&&!was&&k==XK_Delete&&f12_delete_binding(v)){
        if(avp_jaguar_pause_op_active(&v->gpu.pause_op))avp_pc_video_present_pause(v);
        continue;}
    if(d&&!was&&f12_backspace(v,k)){
        mouse_grab_update(v);
        if(avp_jaguar_pause_op_active(&v->gpu.pause_op))avp_pc_video_present_pause(v);
        continue;
    }
    if(d&&!was&&v->menu_open&&v->menu_page==MENU_MAP_MAKER&&
       (e.xkey.state&ControlMask)&&!v->map_maker.name_edit_open&&
       !v->map_maker.browser_open&&!v->map_maker.context_open&&
       !v->map_maker.palette_open&&
       (k==XK_z||k==XK_Z||k==XK_y||k==XK_Y)){
        avp_mm_history_commit(&v->map_maker);
        if(k==XK_y||k==XK_Y||(e.xkey.state&ShiftMask))
            (void)avp_mm_redo(&v->map_maker);
        else (void)avp_mm_undo(&v->map_maker);
        v->mm_drag_paint=0;v->mm_select_drag_active=0;
        v->mm_select_3d_pending=0;v->mm_select_3d_drag_active=0;v->mm_select_drag_last_ns=0;avp_mm_wall_drag_end(&v->map_maker);
        setkey(v,e.xkey.keycode,k,d);
        continue;
    }
    if(d&&!was&&k==XK_Escape&&v->menu_open&&v->menu_page==MENU_MAP_MAKER){
        AVP_MM_RECORD(&v->map_maker,map_maker_key(v,k));setkey(v,e.xkey.keycode,k,d);continue;
    }
    if((v->capture_mode==3||v->capture_mode==4)&&d&&!was){if(k==XK_Escape)v->capture_mode=0;else if(k!=XK_F12&&v->capture_mode==3)apply_capture_key(v,k);continue;}if(k==XK_Escape&&d&&!was&&!v->frontend_input&&!avp_computer_active()
#ifdef AVP_LOCAL_MULTIPLAYER
       && !v->local_mp_open
#endif
      ){
        if(v->menu_open){menu_back(v);mouse_grab_update(v);}
        else if(v->capture_mode)v->capture_mode=0;
        else *quit=1;
    }
#ifdef AVP_LOCAL_MULTIPLAYER
    if((v->local_mp.enabled || v->local_mp_walktest) &&
       ((v->frontend_input && v->gpu.frontend_mode==AVP_MJP_TITLE) ||
        v->local_mp_walktest)){
        if(k==XK_F2 && d&&!was && !avp_computer_active()){
            v->menu_open=0; /* F2 always wins over the host F12 menu in co-op. */
            v->local_mp_open=!v->local_mp_open;
            if(v->local_mp_open)mp_setup_open_page(v,AVP_LOCAL_MP_MENU_ROOT);
            v->menu_repeat_direction=0;
            v->local_mp_suppress_input=1;
            memset(v->key,0,sizeof(v->key));
            memset(v->pressed_sym,0,sizeof(v->pressed_sym));
            setkey(v,e.xkey.keycode,k,d); /* Hold edge: never repeat-toggle. */
            mouse_grab_update(v);
            continue;
        }
        if(v->local_mp_open && k!=XK_F12 &&
           (!v->cfg.menu_key_open || k!=(KeySym)v->cfg.menu_key_open)){
            if(d&&!was){
                if(k==XK_Escape || k==XK_BackSpace ||
                   k==(KeySym)v->cfg.menu_key_back){
                    /* Match F12: physical Backspace navigates to the
                     * parent F2 page without changing any settings. */
                    mp_setup_back(v);
                    memset(v->key,0,sizeof(v->key));
                    memset(v->pressed_sym,0,sizeof(v->pressed_sym));
                    mouse_grab_update(v);
                }else if(k==XK_Up || k==(KeySym)v->cfg.menu_key_up){
                    v->local_mp_cursor=avp_local_mp_menu_next_row(&v->local_mp,
                        v->local_mp_page,v->local_mp_cursor,-1);
                }else if(k==XK_Down || k==(KeySym)v->cfg.menu_key_down){
                    v->local_mp_cursor=avp_local_mp_menu_next_row(&v->local_mp,
                        v->local_mp_page,v->local_mp_cursor,1);
                }else if(k==XK_Return || k==XK_space ||
                         k==(KeySym)v->cfg.menu_key_accept){
                    mp_setup_accept(v);
                }else if(k==XK_Left || k==XK_Right ||
                         k==(KeySym)v->cfg.menu_key_left ||
                         k==(KeySym)v->cfg.menu_key_right){
                    int direction=(k==XK_Left || k==(KeySym)v->cfg.menu_key_left)?-1:1;
                    if(v->local_mp_cursor<AVP_LOCAL_MP_ROWS &&
                       v->local_mp_cursor!=AVP_LOCAL_MP_START_ROW &&
                       avp_local_mp_adjust(&v->local_mp,v->local_mp_cursor,direction))
                        save_settings(v);
                }
            }
            v->local_mp_suppress_input=1;
            if(v->local_mp_open)setkey(v,e.xkey.keycode,k,d);
            continue;
        }
    }
#endif
    if(d&&!was&&quick_turn_key_matches(v,k))queue_quick_turn(v);
    if(d&&!was&&!avp_computer_active()&&(k==XK_F12 || (v->cfg.menu_key_open && k==(KeySym)v->cfg.menu_key_open))){
#ifdef AVP_LOCAL_MULTIPLAYER
        v->local_mp_open=0;
#endif
        v->capture_mode=0;v->menu_open=!v->menu_open;v->menu_page=MENU_ROOT;v->menu_cursor=0;v->menu_index=0;
        if(v->menu_open)v->species_request=(int)((unsigned)player_type>>2);
        /* F12 can interrupt a mouse stroke before ButtonRelease arrives.
         * Preserve it as one completed undo step. */
        avp_mm_history_commit(&v->map_maker);
        v->mm_drag_paint=0;v->mm_select_drag_active=0;
        v->mm_select_3d_pending=0;v->mm_select_3d_drag_active=0;v->mm_select_drag_last_ns=0;avp_mm_wall_drag_end(&v->map_maker);
        v->pad_buffered=0;v->use_button_buffered=0;v->quick_turn_pending=0;
        memset(v->key,0,sizeof(v->key));memset(v->pressed_sym,0,sizeof(v->pressed_sym));mouse_grab_update(v);
        /* The original pause routine is a synchronous VBlank/input loop.
         * Refresh the PC-only menu at the key event itself rather than
         * assuming the regular gameplay presenter will run while paused. */
        if(avp_jaguar_pause_op_active(&v->gpu.pause_op))avp_pc_video_present_pause(v);
    }else if(!v->menu_open&&!v->frontend_input&&d&&!was&&!avp_computer_active()&&k==(KeySym)v->cfg.action_key[0])v->weapon_delta=-1;
    else if(!v->menu_open&&!v->frontend_input&&d&&!was&&!avp_computer_active()&&k==(KeySym)v->cfg.action_key[1])v->weapon_delta=1;
    else if(d&&!was&&k==XK_F5)v->pending_actions|=AVP_PC_ACTION_QUICKSAVE;
    else if(d&&!was&&k==XK_F9)v->pending_actions|=AVP_PC_ACTION_QUICKLOAD;
    else if(!v->menu_open&&d&&!was&&k==XK_Page_Up)v->stage_delta=1;
    else if(!v->menu_open&&d&&!was&&k==XK_Page_Down)v->stage_delta=-1;
    else if(!v->menu_open&&d&&!was&&k==XK_bracketleft)v->hz_delta=-1;
    else if(!v->menu_open&&d&&!was&&k==XK_bracketright)v->hz_delta=1;
    else if(!v->menu_open&&k==(KeySym)v->cfg.key_noclip&&d&&!was)v->noclip=!v->noclip;
    else if(v->menu_open && v->menu_page==MENU_MAP_MAKER &&
            !v->capture_mode &&
            !v->map_maker.context_open && !v->map_maker.browser_open &&
            !v->map_maker.name_edit_open && !v->map_maker.palette_open &&
            k==XK_Return){
        if(d&&!was)map_maker_enter_start(v);
        else if(!d)map_maker_enter_finish(v);
    }
    else if(v->menu_open&&d&&!was&&!v->capture_mode){
        if(v->menu_page==MENU_MAP_MAKER){
            KeySym editkey=k;
            /* The normal key routing deliberately reads the unshifted
             * keysym for bindings. Save As must instead honor Shift for
             * filenames such as saved_map and capital letters. */
            if(e.xkey.state & ShiftMask){
                if(v->map_maker.name_edit_open){
                    KeySym shifted=XLookupKeysym(&e.xkey,1);
                    if(shifted!=NoSymbol)editkey=shifted;
                }else if(k==XK_comma)editkey=XK_less;
                else if(k==XK_period)editkey=XK_greater;
            }
            if(!v->map_maker.mode_3d &&
               (k==XK_Up||k==XK_Down||k==XK_Left||k==XK_Right) &&
               ((e.xkey.state&ShiftMask)||key(v,XK_Shift_L)||key(v,XK_Shift_R))){
                /* The press-event modifier is authoritative; X11 held-key
                 * reconciliation may temporarily miss Shift while repeating. */
                v->mm_nav_shift_latched=1;
                avp_mm_extend_2d_selection(&v->map_maker,k==XK_Left?-1:k==XK_Right?1:0,
                                            k==XK_Up?-1:k==XK_Down?1:0);
            }else{
                if(v->mm_enter_paint_active && k!=XK_Up && k!=XK_Down &&
                   k!=XK_Left && k!=XK_Right)map_maker_enter_finish(v);
                if(v->mm_enter_paint_active)map_maker_key(v,editkey);
                else AVP_MM_RECORD(&v->map_maker,map_maker_key(v,editkey));
            }
            if(v->mm_enter_paint_active && !v->map_maker.mode_3d &&
               (k==XK_Up||k==XK_Down||k==XK_Left||k==XK_Right))
                {map_maker_enter_selection(v);
                 v->mm_enter_last_valid=0;}
        }
        else if((v->menu_page==MENU_MM_KEYS||v->menu_page==MENU_MM_MOUSE||v->menu_page==MENU_MM_PAD) && k==XK_Delete){
            unsigned row=(unsigned)v->menu_cursor;
            if(row<AVP_MM_ACTIONS){if(v->menu_page==MENU_MM_KEYS)v->cfg.mm_key[row]=0;
                else if(v->menu_page==MENU_MM_MOUSE)v->cfg.mm_mouse[row]=-1;
                else v->cfg.mm_button[row]=-1;
                save_settings(v);}
        }
#ifdef AVP_LOCAL_MULTIPLAYER
        else if(v->menu_page==MENU_MP_CONTROLS){
            if(k==XK_Up)v->menu_cursor=(v->menu_cursor+menu_count(v)-1)%menu_count(v);
            else if(k==XK_Down)v->menu_cursor=(v->menu_cursor+1)%menu_count(v);
            else if(k==XK_Left||k==XK_Right||k==XK_Return||k==XK_space){
                mp_controls_adjust(v,v->menu_cursor,k==XK_Left?-1:1,k==XK_Return||k==XK_space);
                mouse_grab_update(v);
            }
        }
#endif
        else if(k==XK_Up)v->menu_cursor=(v->menu_cursor+menu_count(v)-1)%menu_count(v);
        else if(k==XK_Down)v->menu_cursor=(v->menu_cursor+1)%menu_count(v);
        else if(k==XK_Left||k==XK_Right){if(v->menu_page==MENU_VIDEO)menu_video_adjust(v,v->menu_cursor,k==XK_Left?-1:1);else if(v->menu_page==MENU_AUDIO)menu_audio_adjust(v,v->menu_cursor,k==XK_Left?-1:1);
            else if(v->menu_page==MENU_CHEATS && v->menu_cursor==2)menu_cheats_target_adjust(v,k==XK_Left?-1:1);
            else if(v->menu_page==MENU_CHEATS_FUJI||v->menu_page==MENU_CHEATS_BANANA||v->menu_page==MENU_CHEATS_GPS||v->menu_page==MENU_CHEATS_LEVEL)
                original_cheat_adjust(v,v->menu_cursor,k==XK_Left?-1:1);
            else {int id=menu_id(v,v->menu_cursor);if(id>=0 && !(v->menu_page==MENU_CHEATS_NEW && (id==175||id==176))){v->menu_index=id;menu_adjust(v,k==XK_Left?-1:1);mouse_grab_update(v);}}}
        else if(k==XK_Return||k==XK_space){menu_choose(v);mouse_grab_update(v);}
    }if(d&&!was)short_key_press(v,k);setkey(v,e.xkey.keycode,k,d);
    /* The PC-only controls must remain visibly interactive while the
     * original Jaguar pause loop owns the frame cadence. The F12 opening
     * redraw is above; repaint after submenu/navigation and Escape too. */
    if(d&&!was&&k!=XK_F12&&avp_jaguar_pause_op_active(&v->gpu.pause_op)&&
       (v->menu_open||k==XK_Escape)&&
       (k==XK_Escape||k==XK_Up||k==XK_Down||k==XK_Left||k==XK_Right||
        k==XK_Return||k==XK_space||k==XK_Delete))
        avp_pc_video_present_pause(v);
    }}
#ifndef _WIN32
    /* Process queued map/focus changes together; do not ever inhibit an
     * unfocused/minimized application or leave an inhibitor on exit. */
    avp_pc_idle_inhibit_set_active(v->idle_inhibit,
        v->idle_window_mapped && v->idle_window_focused && !*quit);
    avp_pc_idle_inhibit_tick(v->idle_inhibit);
    if(keyboard_events_seen)reconcile_physical_keyboard(v);
#endif
    mp_menu_repeat_poll(v);
    if(v->mm_enter_paint_active && (!v->menu_open||v->menu_page!=MENU_MAP_MAKER||
        v->map_maker.palette_open||v->map_maker.context_open||
        v->map_maker.browser_open||v->map_maker.name_edit_open || !v->mouse_focused))
        map_maker_enter_finish(v);
    /* Arrow movement is repeatable even on X11 servers with detectable
     * auto-repeat disabled. The first press moves immediately via key event. */
    if(v->menu_open && v->menu_page==MENU_MAP_MAKER && !v->map_maker.mode_3d &&
       !v->map_maker.context_open&&!v->map_maker.palette_open &&
       !v->map_maker.browser_open&&!v->map_maker.name_edit_open&&v->mouse_focused){
        int direction=key(v,XK_Up)?1:key(v,XK_Down)?2:key(v,XK_Left)?3:key(v,XK_Right)?4:0;
        uint64_t now=avp_pc_now_ns();
        if(direction!=v->mm_nav_direction){
            v->mm_nav_direction=direction;
            v->mm_nav_next_ns=now+250000000ull;
        }else if(direction && now>=v->mm_nav_next_ns){
            int dx=direction==3?-1:direction==4?1:0;
            int dy=direction==1?-1:direction==2?1:0;
            if(v->mm_nav_shift_latched||key(v,XK_Shift_L)||key(v,XK_Shift_R))
                avp_mm_extend_2d_selection(&v->map_maker,dx,dy);
            else avp_mm_move(&v->map_maker,dx,dy);
            if(v->mm_enter_paint_active){
                map_maker_enter_selection(v);
                v->mm_enter_last_valid=0;
            }
            v->mm_nav_next_ns=now+85000000ull;
        }
    }else{v->mm_nav_direction=0;v->mm_nav_next_ns=0;}
    /* Range-selection edge scroll must advance even without new MotionNotify
     * events (the pointer may be stationary at the edge). It never paints. */
    if(v->mm_select_drag_active && v->menu_open &&
       v->menu_page==MENU_MAP_MAKER && !v->map_maker.mode_3d &&
       !v->map_maker.context_open && !v->map_maker.palette_open &&
       !v->map_maker.browser_open && !v->map_maker.name_edit_open &&
       v->mouse_focused){
        uint64_t now=avp_pc_now_ns(),old=v->mm_select_drag_last_ns;
        double dt=old&&now>old?(double)(now-old)/1e9:0.0;
        unsigned xx,yy;
        int vx=((int)v->win_w-(int)v->present_w)/2;
        int vy=((int)v->win_h-(int)v->present_h)/2;
        int step;
        double speed=700.0;
        AvpMmOverheadLayout layout;
        if(dt>0.07)dt=0.07;
        /* At moderate zoom, cap speed by tiles/second too: otherwise a
         * tiny cell could fly across the selection faster than intended. */
        if(avp_mm_overhead_layout(&v->map_maker,v->present_w,v->present_h,
                                  &layout) && speed>7.0*layout.cell_px)
            speed=7.0*layout.cell_px;
        step=(int)(dt*speed+0.5);
        v->mm_select_drag_last_ns=now;
        if(avp_mm_overhead_selection_auto_pan(&v->map_maker,
                v->present_w,v->present_h,
                v->mm_select_drag_px-vx,v->mm_select_drag_py-vy,
                step,&xx,&yy))
            (void)avp_mm_update_2d_drag_selection(&v->map_maker,xx,yy);
    }else v->mm_select_drag_last_ns=0;
    if(v->menu_open&&v->menu_page==MENU_MAP_MAKER&&v->map_maker.mode_3d&&
       !v->map_maker.palette_open&&v->mouse_focused){
        uint64_t now=avp_pc_now_ns(),old=v->mm_last_move_ns;
        double dt=old&&now>old?(double)(now-old)/1e9:0.0;
        double forward=0,side=0,turn=0;
        if(dt>0.07)dt=0.07;
        v->mm_last_move_ns=now;
        /* Arrow keys share the SAME delta-time motion path as WASD/QE;
         * never apply one-step movement on KeyPress in 3D. */
        if(key(v,XK_Up))forward+=1;
        if(key(v,XK_Down))forward-=1;
        if(key(v,XK_Left))turn-=1;
        if(key(v,XK_Right))turn+=1;
        if(v->cfg.mm_key[MM_FORWARD]&&key(v,(KeySym)v->cfg.mm_key[MM_FORWARD]))forward+=1;
        if(v->cfg.mm_key[MM_BACK]&&key(v,(KeySym)v->cfg.mm_key[MM_BACK]))forward-=1;
        if(v->cfg.mm_key[MM_LEFT]&&key(v,(KeySym)v->cfg.mm_key[MM_LEFT]))side-=1;
        if(v->cfg.mm_key[MM_RIGHT]&&key(v,(KeySym)v->cfg.mm_key[MM_RIGHT]))side+=1;
        if(v->cfg.mm_key[MM_TURN_LEFT]&&key(v,(KeySym)v->cfg.mm_key[MM_TURN_LEFT]))turn-=1;
        if(v->cfg.mm_key[MM_TURN_RIGHT]&&key(v,(KeySym)v->cfg.mm_key[MM_TURN_RIGHT]))turn+=1;
        if(v->cfg.gamepad_enabled && v->js_fd>=0){
            if(button_down(v,v->cfg.mm_button[MM_FORWARD]))forward+=1;
            if(button_down(v,v->cfg.mm_button[MM_BACK]))forward-=1;
            if(button_down(v,v->cfg.mm_button[MM_LEFT]))side-=1;
            if(button_down(v,v->cfg.mm_button[MM_RIGHT]))side+=1;
            if(button_down(v,v->cfg.mm_button[MM_TURN_LEFT]))turn-=1;
            if(button_down(v,v->cfg.mm_button[MM_TURN_RIGHT]))turn+=1;
            int axis=v->cfg.move_axis;
            if(axis>=0&&axis<v->js_axes_count){double n=(double)v->js_axes[axis]/32767.0;
                if(v->cfg.move_invert)n=-n;
                if(fabs(n)>v->cfg.deadzone)forward-=n;}
            axis=v->cfg.strafe_axis;
            if(axis>=0&&axis<v->js_axes_count){double n=(double)v->js_axes[axis]/32767.0;
                if(v->cfg.strafe_invert)n=-n;
                if(fabs(n)>v->cfg.deadzone)side+=n;}
            axis=v->cfg.turn_axis;
            if(axis>=0&&axis<v->js_axes_count){double n=(double)v->js_axes[axis]/32767.0;
                if(v->cfg.turn_invert)n=-n;
                if(fabs(n)>v->cfg.deadzone)turn+=n;}
        }
        avp_mm_camera_motion(&v->map_maker,forward*dt*2.2,side*dt*2.2,turn*dt*1.8);
        (void)avp_mm_pick(&v->map_maker,v->map_maker.cursor_u,v->map_maker.cursor_v);
        map_maker_enter_3d_target(v);
    }else v->mm_last_move_ns=0u;
    /* S117/R3: single-player uses the very same P1 Map Zoom In/Out
     * bindings already exposed in the F12 multiplayer controls and P1
     * gamepad menu. Previously only the multiplayer presenter consumed
     * them, leaving v->gpu.map_zoom_steps unchanged in single-player.
     * Keep this entirely in the PC host: never change Jaguar pad bits,
     * simulation ticks, the original automap, or any multiplayer view. */
    {
        unsigned held=0u;
        const int allow=v->mouse_focused && !v->frontend_input &&
            !v->menu_open && !v->capture_mode && !avp_computer_active() &&
            !avp_jaguar_pause_op_active(&v->gpu.pause_op)
#ifdef AVP_LOCAL_MULTIPLAYER
            && !v->local_mp_walktest && !v->local_mp_open
#endif
            ;
        if(allow){
#ifdef AVP_LOCAL_MULTIPLAYER
            const AvpMpInputProfile *p1=&v->local_mp_controls.player[0];
            for(unsigned a=AVP_MP_MAP_ZOOM_IN;a<=AVP_MP_MAP_ZOOM_OUT;a++){
                const unsigned bit=1u<<(a-AVP_MP_MAP_ZOOM_IN);
                if((p1->key[a] && key(v,(KeySym)p1->key[a])) ||
                   (v->cfg.gamepad_enabled && v->js_fd>=0 &&
                    button_down(v,p1->gamepad_button[a])))held|=bit;
            }
#else
            /* Standalone single-player builds lack the multiplayer input
             * profile. Provide the standard host +/- keyboard aliases. */
            if(key(v,XK_equal)||key(v,XK_plus)||key(v,XK_KP_Add))held|=1u;
            if(key(v,XK_minus)||key(v,XK_KP_Subtract))held|=2u;
#endif
        }
        {
            const unsigned edge=held&~v->singleplayer_map_zoom_held;
            int zoom=v->gpu.map_zoom_steps;
            if(edge&1u)++zoom;
            if(edge&2u)--zoom;
            if(edge)avp_gpu_set_map_zoom(&v->gpu,zoom);
            v->singleplayer_map_zoom_held=held;
        }
    }
    if(v->mouse_grabbed&&v->mouse_motion_seen){
        /* Warping after *every* mouse event batch created a feedback loop of
         * synthetic motion and X server work during fast aiming. Recenter only
         * when approaching the horizontal confinement edge; normal motion is
         * taken directly as event-to-event displacement. */
        if(avp_pc_mouse_needs_recenter(v->mouse_last_x,(int)v->win_w)){
            int cx=(int)v->win_w/2,cy=(int)v->win_h/2;
            XWarpPointer(v->dpy,None,v->win,0,0,0,0,cx,cy);
            /* Keep last_x until the warp's center echo is observed: queued
             * pre-warp events must retain their real displacement. */
            v->mouse_recenter=1;
        }
        v->mouse_motion_seen=0;
    }
    {u32 p=0xffffffffu;float mv=0.0f,sv=0.0f,tv=0.0f;if(!v->menu_open){
        if(!v->frontend_input&&!avp_computer_active()){
            if(button_edge(v,v->cfg.action_button[0]))v->weapon_delta=-1;
            else if(button_edge(v,v->cfg.action_button[1]))v->weapon_delta=1;
        }
        if(v->frontend_input || v->cfg.controller_mode_jaguar){
            /* Virtual Jaguar pad.  Frontend always uses pure Jaguar semantics so
               PC action chords (notably turn=C+dpad) cannot leak into menus. */
            if(key(v,v->frontend_input?(KeySym)v->cfg.menu_key_up:XK_Up)||key(v,XK_w)||(v->frontend_input&&button_down(v,v->cfg.menu_button_up)))p&=~(1u<<JOY_UP);
            if(key(v,v->frontend_input?(KeySym)v->cfg.menu_key_down:XK_Down)||key(v,XK_s)||(v->frontend_input&&button_down(v,v->cfg.menu_button_down)))p&=~(1u<<JOY_DOWN);
            {int left=key(v,v->frontend_input?(KeySym)v->cfg.menu_key_left:XK_Left)||key(v,XK_a)||(v->frontend_input&&button_down(v,v->cfg.menu_button_left));
             int right=key(v,v->frontend_input?(KeySym)v->cfg.menu_key_right:XK_Right)||key(v,XK_d)||(v->frontend_input&&button_down(v,v->cfg.menu_button_right));
             if(left!=right)p&=~(1u<<(left?JOY_LEFT:JOY_RIGHT));}
            if(key(v,v->frontend_input?(KeySym)v->cfg.menu_key_accept:XK_Return)||key(v,XK_space)||key(v,XK_z)||(v->frontend_input&&button_down(v,v->cfg.menu_button_accept)))p&=~(1u<<FIRE_A);
            if(key(v,XK_x)||key(v,XK_Shift_L)||key(v,XK_Shift_R))p&=~(1u<<FIRE_B);
            if(key(v,XK_c)||key(v,v->frontend_input?(KeySym)v->cfg.menu_key_back:XK_Escape)||(v->frontend_input&&button_down(v,v->cfg.menu_button_back)))p&=~(1u<<FIRE_C);
            if(key(v,(KeySym)v->cfg.key_option)) p&=~(1u<<OPTION);
            if(key(v,(KeySym)v->cfg.key_pause)) p&=~(1u<<PAUSE);
            {static const KeySym ks[10]={XK_0,XK_1,XK_2,XK_3,XK_4,XK_5,XK_6,XK_7,XK_8,XK_9};
             for(int i=0;i<10;i++)if(key(v,ks[i]))p=avp_pc_digit_pad(p,(unsigned)i);}
            if(key(v,XK_asterisk)) p&=~(1u<<KEY_STAR);
            if(key(v,XK_numbersign)) p&=~(1u<<KEY_HASH);
            mv=axis_value(v,v->cfg.jag_dpad_y_axis,v->cfg.jag_dpad_y_invert);sv=axis_value(v,v->cfg.jag_dpad_x_axis,v->cfg.jag_dpad_x_invert);
            /* When analog controls are explicitly enabled, keep the axes out
               of the digital Jaguar D-pad image. Buttons and keyboard still
               work normally, and the original digital mode is unchanged. */
            if(!v->cfg.analog_movement||v->frontend_input){
                if(mv<-0.65f)p&=~(1u<<JOY_UP);else if(mv>0.65f)p&=~(1u<<JOY_DOWN);
                if(sv<-0.65f)p&=~(1u<<JOY_LEFT);else if(sv>0.65f)p&=~(1u<<JOY_RIGHT);
            }
            if(v->cfg.jag_up_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_up_button)) p&=~(1u<<JOY_UP);
            if(v->cfg.jag_down_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_down_button)) p&=~(1u<<JOY_DOWN);
            if(v->cfg.jag_left_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_left_button)) p&=~(1u<<JOY_LEFT);
            if(v->cfg.jag_right_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_right_button)) p&=~(1u<<JOY_RIGHT);
            if(v->cfg.jag_a_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_a_button)) p&=~(1u<<FIRE_A);
            if(v->cfg.jag_b_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_b_button)) p&=~(1u<<FIRE_B);
            if(v->cfg.jag_c_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_c_button)) p&=~(1u<<FIRE_C);
            if(v->cfg.jag_pause_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_pause_button)) p&=~(1u<<PAUSE);
            if(v->cfg.jag_option_button!=v->cfg.map_button&&button_down(v,v->cfg.jag_option_button)) p&=~(1u<<OPTION);
            {static const unsigned bits[12]={KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,KEY_STAR,KEY_HASH};for(int i=0;i<12;i++)if(v->cfg.jag_keypad_button[i]!=v->cfg.map_button&&button_down(v,v->cfg.jag_keypad_button[i]))p&=~(1u<<bits[i]);}
            /* Host-only map shortcut is also available in Jaguar-pad mode.
               An assigned joystick map button must be dedicated: button 2 is
               Jaguar C by default, and must not become Map as a side effect. */
            p=avp_pc_map_pad(p,key(v,(KeySym)v->cfg.key_map));
            p=avp_pc_map_pad(p,button_down(v,v->cfg.map_button));
            if(v->cfg.analog_movement&&!v->frontend_input){
                v->modern_forward=avp_pc_axis_motion_full_travel(axis_value(v,v->cfg.move_axis,v->cfg.move_invert));
                v->modern_strafe=avp_pc_axis_motion_full_travel(axis_value(v,v->cfg.strafe_axis,v->cfg.strafe_invert));
                v->modern_turn=axis_value(v,v->cfg.turn_axis,v->cfg.turn_invert);
            }else v->modern_turn=v->modern_forward=v->modern_strafe=0.0f;
        }else{
            if(key(v,(KeySym)v->cfg.key_forward)) p&=~(1u<<JOY_UP);
            if(key(v,(KeySym)v->cfg.key_back)) p&=~(1u<<JOY_DOWN);
            /* A+D or Q+D must not collapse into two pressed Jaguar directions.
             * A held turn owns the pad chord; independent strafe is applied
             * at the original player acceleration step by the PC host bridge. */
            p=avp_pc_keyboard_action_horizontal_pad(p,
                key(v,(KeySym)v->cfg.key_turn_left),key(v,(KeySym)v->cfg.key_turn_right),
                key(v,(KeySym)v->cfg.key_strafe_left),key(v,(KeySym)v->cfg.key_strafe_right),
                player_type==PT_ALIEN?OPTION:FIRE_C);
            if(key(v,(KeySym)v->cfg.key_fire)) p&=~(1u<<FIRE_A);
            if(player_type!=PT_ALIEN&&key(v,(KeySym)v->cfg.key_use)) p&=~(1u<<FIRE_B);
            p=avp_pc_map_pad(p,key(v,(KeySym)v->cfg.key_map));
            /* Keep the original Jaguar-only controls reachable in PC action
               mode so Pause/Option/keypad gameplay features remain usable. */
            p=avp_pc_pause_option_pad(p,
                key(v,(KeySym)v->cfg.key_pause)||button_down(v,v->cfg.pc_pause_button),
                key(v,(KeySym)v->cfg.key_option)||button_down(v,v->cfg.pc_option_button));
            {static const KeySym ks[10]={XK_0,XK_1,XK_2,XK_3,XK_4,XK_5,XK_6,XK_7,XK_8,XK_9};
             for(int i=0;i<10;i++)if(key(v,ks[i]))p=avp_pc_digit_pad(p,(unsigned)i);}
            if(key(v,XK_asterisk))p&=~(1u<<KEY_STAR);
            if(key(v,XK_numbersign))p&=~(1u<<KEY_HASH);
            mv=axis_value(v,v->cfg.move_axis,v->cfg.move_invert);sv=axis_value(v,v->cfg.strafe_axis,v->cfg.strafe_invert);tv=axis_value(v,v->cfg.turn_axis,v->cfg.turn_invert);v->modern_turn=tv;
            /* Modern turn-stick isn't encoded as Jaguar D-pad bits during
             * normal play.  While facehugged it must count as a physical
             * left/right throw as well, subject to the normal edge latch. */
            if(!v->cfg.analog_movement){
                unsigned turn_key=player_type==PT_ALIEN?OPTION:FIRE_C;
                if(tv<-0.5f){p&=~(1u<<JOY_LEFT);p&=~(1u<<turn_key);}
                else if(tv>0.5f){p&=~(1u<<JOY_RIGHT);p&=~(1u<<turn_key);}
            }else if(hugkill){
                if(tv<-0.65f)p&=~(1u<<JOY_LEFT);
                else if(tv>0.65f)p&=~(1u<<JOY_RIGHT);
            }
            v->modern_forward=v->cfg.analog_movement?avp_pc_axis_motion_full_travel(mv):0.0f;
            v->modern_strafe=v->cfg.analog_movement?avp_pc_axis_motion_full_travel(sv):0.0f;
            if(!v->cfg.analog_movement){if(avp_pc_axis_digital_positive(mv))p&=~(1u<<JOY_UP);else if(avp_pc_axis_digital_negative(mv))p&=~(1u<<JOY_DOWN);if(avp_pc_axis_digital_negative(sv))p&=~(1u<<JOY_LEFT);else if(avp_pc_axis_digital_positive(sv))p&=~(1u<<JOY_RIGHT);}
            if(button_down(v,v->cfg.fire_button))p&=~(1u<<FIRE_A);
            if(player_type!=PT_ALIEN&&button_down(v,v->cfg.use_button))p&=~(1u<<FIRE_B);
            p=avp_pc_map_pad(p,button_down(v,v->cfg.map_button));
            if(button_edge(v,v->cfg.noclip_button))v->noclip=!v->noclip;
        }
        /* Dedicated strafe bindings are gameplay actions, not PC-action-only
         * keys.  In Jaguar-pad mode the ordinary unmodified Left/Right bits
         * already mean sidestep; leave turn modifiers and frontend untouched. */
        if(!v->frontend_input)
            p=avp_pc_strafe_pad(p,button_down(v,v->cfg.strafe_left_button),
                                    button_down(v,v->cfg.strafe_right_button));
        /* Dedicated joystick forward/backward buttons or signed axis directions
         * work in both gameplay controller modes; frontend navigation remains
         * governed solely by the original Jaguar-pad inputs. */
        if(!v->frontend_input)
            p=avp_pc_move_pad(p,button_down(v,v->cfg.forward_button),
                                 button_down(v,v->cfg.backward_button));
        if(!v->frontend_input&&player_type==PT_PREDATOR){
            if((v->cfg.key_predator_medkit&&key(v,(KeySym)v->cfg.key_predator_medkit))||button_down(v,v->cfg.predator_medkit_button))p&=~(1u<<KEY_7);
            if((v->cfg.key_predator_vision&&key(v,(KeySym)v->cfg.key_predator_vision))||button_down(v,v->cfg.predator_vision_button))p&=~(1u<<KEY_6);
            if((v->cfg.key_predator_cloak&&key(v,(KeySym)v->cfg.key_predator_cloak))||button_down(v,v->cfg.predator_cloak_button))p&=~(1u<<OPTION);
        }
        if(!v->frontend_input && v->cfg.mouse_enabled && v->mouse_grabbed){
            if(mouse_action_down(v,0))p&=~(1u<<FIRE_A);
            if(player_type!=PT_ALIEN&&mouse_action_down(v,1))p&=~(1u<<FIRE_B);
            p=avp_pc_map_pad(p,mouse_action_down(v,2));
            if(player_type==PT_PREDATOR){
                if(mouse_action_down(v,8))p&=~(1u<<KEY_7);
                if(mouse_action_down(v,9))p&=~(1u<<OPTION);
                if(mouse_action_down(v,10))p&=~(1u<<KEY_6);
            }
            if(avp_computer_active()){
                if(mouse_action_down(v,5))p&=~(1u<<FIRE_A);
                if(mouse_action_down(v,6))p&=~(1u<<FIRE_B);
                if(mouse_action_down(v,7))p&=~(1u<<FIRE_C);
            }
        }
        /* Direct attack and terminal bindings are aliases for the original
           Jaguar A/B/C actions, not new attack or menu implementations. */
        {int base=v->cfg.controller_mode_jaguar?-1:(avp_computer_active()?5:(player_type==PT_ALIEN?2:-1));
         if(base>=0){
            const unsigned bits[3]={FIRE_A,FIRE_B,FIRE_C};
            for(int i=0;i<3;i++){
                int idx=base+i;
                if((v->cfg.action_key[idx]&&key(v,(KeySym)v->cfg.action_key[idx]))||button_down(v,v->cfg.action_button[idx]))
                    p&=~(1u<<bits[i]);
            }
         }}
        if(!v->frontend_input&&key(v,(KeySym)v->cfg.key_noclip)){} /* toggle remains edge-driven by event path */
    }else v->modern_turn=v->modern_forward=v->modern_strafe=0.0f;
        /* Host polls run faster than the retail game tick. Preserve a brief
         * deadzone-filtered stick impulse per axis only when explicitly enabled.
         * Mouse motion remains separate and is never buffered as analog input. */
        avp_pc_analog_buffer_poll(&v->analog_buffer,
            v->modern_forward,v->modern_strafe,v->modern_turn,
            v->cfg.buffer_short_analog&&v->cfg.analog_movement&&v->cfg.gamepad_enabled&&
            v->js_fd>=0&&v->mouse_focused&&!v->menu_open&&!v->frontend_input&&!avp_computer_active()&&
            !avp_jaguar_pause_op_active(&v->gpu.pause_op));
        avp_pc_buffer_poll(&v->previous_poll_pad,&v->pad_buffered,p,
                           v->cfg.buffer_short_presses&&v->mouse_focused&&!v->menu_open&&!v->frontend_input&&
                           !avp_computer_active()&&!avp_jaguar_pause_op_active(&v->gpu.pause_op));
        v->use_button_held=!v->menu_open&&!v->frontend_input&&!avp_computer_active()&&
             !avp_jaguar_pause_op_active(&v->gpu.pause_op)&&
             !v->cfg.controller_mode_jaguar&&player_type==PT_ALIEN&&
             (key(v,(KeySym)v->cfg.key_use)||button_down(v,v->cfg.use_button)||
              (v->cfg.mouse_enabled&&mouse_action_down(v,1)));
        if(v->cfg.buffer_short_presses&&v->mouse_focused&&v->use_button_held)v->use_button_buffered=1;
        if(!v->cfg.buffer_short_presses||!v->mouse_focused||v->menu_open||v->frontend_input||avp_computer_active()||
           avp_jaguar_pause_op_active(&v->gpu.pause_op))v->use_button_buffered=0;
#ifdef AVP_LOCAL_MULTIPLAYER
        if(v->local_mp_open||v->local_mp_suppress_input)p=0xffffffffu;
        v->local_mp_suppress_input=0; /* Never advance the retail frontend on a consumed event. */
#endif
        /* Returning to the retail title must not treat the F12 accept
         * press (keyboard or pad) as a NEW GAME selection. Wait for a
         * neutral frontend confirmation first; no gameplay input changes. */
        if(v->frontend_input && v->title_return_wait_release){
            const u32 confirm=(1u<<FIRE_A)|(1u<<FIRE_B)|(1u<<FIRE_C);
            if((p&confirm)==confirm)v->title_return_wait_release=0;
            else p|=confirm;
        }
        original_input_sample(v,0u,p);
        *pad=p;}return 1;}
void avp_pc_video_set_frontend_input(AvpPcVideo*v,int active){if(v){v->frontend_input=active?1:0;
    if(active){v->quick_turn_pending=0;v->pad_buffered=0;v->use_button_buffered=0;v->previous_poll_pad=0xffffffffu;v->use_button_held=0;avp_pc_analog_buffer_reset(&v->analog_buffer);v->sim_turn=v->sim_forward=v->sim_strafe=0.0f;}
    mouse_grab_update(v);}}
int avp_pc_video_alien_use_down(const AvpPcVideo *v){return v?v->use_button_held:0;}
int avp_pc_video_alien_use_separate(const AvpPcVideo *v){return v&&!v->cfg.controller_mode_jaguar;}
int avp_pc_video_take_alien_use(AvpPcVideo *v){if(!v)return 0;
    int down=v->use_button_held||v->use_button_buffered;
    v->use_button_buffered=0;return down;
}
int avp_pc_video_buffer_short_presses(const AvpPcVideo *v){return v&&v->cfg.buffer_short_presses;}
int avp_pc_video_buffer_short_analog(const AvpPcVideo *v){return v&&v->cfg.buffer_short_analog;}
u32 avp_pc_video_simulation_pad(AvpPcVideo *v,u32 current){
    if(!v)return current;
    /* Called once per original 10 Hz tick, before both lift interaction and
     * MovePlayer. Use currently-held axes preferentially and consume any
     * sub-tick impulses only once. Never synthesize a release or extra tick. */
    /* Use a tick-scoped snapshot. Mutating the last polled live axes would
     * replay a one-shot pulse during catch-up ticks without another poll. */
    v->sim_forward=v->modern_forward;
    v->sim_strafe=v->modern_strafe;
    v->sim_turn=v->modern_turn;
    avp_pc_analog_buffer_take(&v->analog_buffer,
        &v->sim_forward,&v->sim_strafe,&v->sim_turn,
        v->cfg.buffer_short_analog&&v->cfg.analog_movement&&v->cfg.gamepad_enabled&&
        v->js_fd>=0&&v->mouse_focused&&!v->menu_open&&!v->frontend_input&&!avp_computer_active()&&
        !avp_jaguar_pause_op_active(&v->gpu.pause_op));
    /* If a Q/E press was buffered before the turning key was pressed, its
     * transient pad direction must not override the held turn chord. Live
     * keyboard strafing is carried separately for this simultaneous chord. */
    if(!v->cfg.controller_mode_jaguar&&v->mouse_focused&&!v->frontend_input&&
       !v->menu_open&&!v->capture_mode&&!avp_computer_active()&&
       !avp_jaguar_pause_op_active(&v->gpu.pause_op)){
        const int left=key(v,(KeySym)v->cfg.key_turn_left);
        const int right=key(v,(KeySym)v->cfg.key_turn_right);
        if(left||right){
            if(left&&right){
                v->pad_buffered&=~((1u<<JOY_LEFT)|(1u<<JOY_RIGHT));
            }else if(left){
                v->pad_buffered&=~(1u<<JOY_RIGHT);
            }else{
                v->pad_buffered&=~(1u<<JOY_LEFT);
            }
            if(!left&&key(v,(KeySym)v->cfg.key_strafe_left))
                v->pad_buffered&=~(1u<<JOY_LEFT);
            if(!right&&key(v,(KeySym)v->cfg.key_strafe_right))
                v->pad_buffered&=~(1u<<JOY_RIGHT);
        }
    }
    {
        u32 pad=avp_pc_buffer_take(&v->pad_buffered,current,
             v->cfg.buffer_short_presses&&v->mouse_focused&&!v->menu_open&&!v->frontend_input&&
             !avp_computer_active()&&!avp_jaguar_pause_op_active(&v->gpu.pause_op));
        if(v->mouse_focused&&!v->menu_open&&!v->frontend_input&&!v->capture_mode){
            int left,right;
            if(v->cfg.controller_mode_jaguar){
                left=key(v,XK_Left)||key(v,XK_a);
                right=key(v,XK_Right)||key(v,XK_d);
            }else{
                left=key(v,(KeySym)v->cfg.key_turn_left);
                right=key(v,(KeySym)v->cfg.key_turn_right);
            }
            if(left&&right){
                pad|=(1u<<JOY_LEFT)|(1u<<JOY_RIGHT);
                /* The PC turn modifier is consumed only when no independent
                 * key holds it (Alien OPTION, other species FIRE_C). */
                if(!v->cfg.controller_mode_jaguar &&
                   !key(v,(KeySym)v->cfg.key_option) &&
                   (player_type!=PT_ALIEN || !key(v,(KeySym)v->cfg.action_key[4])))
                    pad|=1u<<(player_type==PT_ALIEN?OPTION:FIRE_C);
            }
        }
        /* Once an original mode is active, Option+keypad commands are
         * consumed by the PC adapter. Do not also execute ordinary weapon
         * selection, MapKeys, Predator vision or the unrelated TEST_LEVELS
         * keys in PLAYER.S. This masks only those held chord buttons on the
         * authored 10-Hz player tick; normal input is bit-for-bit unchanged. */
        if((v->orig_fuji[0]||v->orig_banana[0]) &&
           !(pad&(1u<<OPTION)) && (pad&(1u<<PAUSE))){
            pad|=(1u<<KEY_0)|(1u<<KEY_1)|(1u<<KEY_2)|(1u<<KEY_3)|
                 (1u<<KEY_4)|(1u<<KEY_5)|(1u<<KEY_6)|(1u<<KEY_8)|
                 (1u<<KEY_9);
            if(v->orig_banana[0])pad|=(1u<<FIRE_A)|(1u<<FIRE_B);
        }
        return pad;
    }
}
int avp_pc_video_take_weapon_delta(AvpPcVideo*v){int d=v?v->weapon_delta:0;if(v)v->weapon_delta=0;return d;}
int avp_pc_video_quick_turn_allowed(const AvpPcVideo *v)
{
    return v&&quick_turn_allowed(v);
}
unsigned avp_pc_video_take_quick_turn(AvpPcVideo *v)
{
    unsigned n;
    if(!v)return 0;
    n=v->quick_turn_pending;
    v->quick_turn_pending=0;
    return quick_turn_allowed(v)?n:0;
}
int avp_pc_video_noclip(const AvpPcVideo*v){return v?v->noclip:0;}float avp_pc_video_modern_turn(AvpPcVideo*v){if(!v)return 0.0f;v->mouse_aim_active=0;
    if(v->cfg.mouse_enabled&&v->mouse_grabbed&&!v->menu_open&&!v->frontend_input&&v->mouse_delta!=0.0f){
        float dx=v->mouse_delta;float motion=avp_pc_mouse_turn(dx,v->cfg.mouse_sensitivity,v->cfg.mouse_acceleration);
        v->mouse_delta=0.0f;v->mouse_aim_active=1;return motion;
    }
    /* A grabbed mouse remains a precision aim device while stationary.
     * Do not select mouse impulse math for a concurrently moving stick. */
    v->mouse_aim_active=v->cfg.mouse_enabled&&v->mouse_grabbed&&
        !v->menu_open&&!v->frontend_input&&v->sim_turn==0.0f;
    v->mouse_delta=0.0f;return v->sim_turn;}
int avp_pc_video_analog_enabled(const AvpPcVideo*v){return v&&v->cfg.analog_movement&&!v->frontend_input;}
int avp_pc_video_enhanced_analog_enabled(const AvpPcVideo*v){return avp_pc_video_analog_enabled(v)&&v->cfg.enhanced_analog_controls;}
int avp_pc_video_keyboard_strafe_during_turn(const AvpPcVideo *v)
{
    if(!v||v->cfg.controller_mode_jaguar||!v->mouse_focused||
       v->frontend_input||v->menu_open||v->capture_mode||
       avp_computer_active()||avp_jaguar_pause_op_active(&v->gpu.pause_op))
        return 0;
    return avp_pc_keyboard_chord_strafe(
        key(v,(KeySym)v->cfg.key_turn_left),key(v,(KeySym)v->cfg.key_turn_right),
        key(v,(KeySym)v->cfg.key_strafe_left),key(v,(KeySym)v->cfg.key_strafe_right));
}

int avp_pc_video_keyboard_turn_direction(const AvpPcVideo *v)
{
    int left,right;
    if(!v||v->cfg.controller_mode_jaguar||!v->mouse_focused||
       v->frontend_input||v->menu_open||v->capture_mode||
       avp_computer_active()||avp_jaguar_pause_op_active(&v->gpu.pause_op))
        return 0;
    left=key(v,(KeySym)v->cfg.key_turn_left);
    right=key(v,(KeySym)v->cfg.key_turn_right);
    return right!=left?(right?1:-1):0;
}

int avp_pc_video_live_stick_turn(const AvpPcVideo*v,float *axis)
{
    if(axis)*axis=0.0f;
    if(!v||!axis||!avp_pc_video_enhanced_analog_enabled(v)||
       !v->cfg.gamepad_enabled||v->js_fd<0||v->menu_open||
       v->frontend_input||avp_computer_active()||
       avp_jaguar_pause_op_active(&v->gpu.pause_op)||
       (v->cfg.mouse_enabled&&v->mouse_grabbed&&v->mouse_delta!=0.0f))return 0;
    *axis=v->modern_turn;
    return 1;
}
int avp_pc_video_mouse_aim_active(const AvpPcVideo*v){return v&&v->mouse_aim_active;}
int avp_pc_video_aim_enabled(const AvpPcVideo*v){return v&&!v->frontend_input&&(v->cfg.analog_movement||(v->cfg.mouse_enabled&&v->mouse_grabbed));}
float avp_pc_video_aim_sensitivity(const AvpPcVideo*v){return (v&&v->cfg.mouse_enabled&&v->mouse_grabbed&&v->mouse_aim_active)?1.0f:avp_pc_video_turn_sensitivity(v);}
float avp_pc_video_aim_speed(const AvpPcVideo*v){return (v&&v->cfg.mouse_enabled&&v->mouse_grabbed&&v->mouse_aim_active)?1.0f:avp_pc_video_turn_speed(v);}
void avp_pc_video_analog_motion(const AvpPcVideo*v,float *forward,float *strafe){
    if(forward)*forward=avp_pc_video_analog_enabled(v)?v->sim_forward:0.0f;
    if(strafe)*strafe=avp_pc_video_analog_enabled(v)?v->sim_strafe:0.0f;
}
float avp_pc_video_turn_sensitivity(const AvpPcVideo*v){return v?v->cfg.turn_sensitivity:1.0f;}
float avp_pc_video_turn_speed(const AvpPcVideo*v){return v?v->cfg.turn_speed:1.0f;}int avp_pc_video_take_stage_delta(AvpPcVideo*v){int d=v?v->stage_delta:0;if(v)v->stage_delta=0;return d;}int avp_pc_video_take_species_request(AvpPcVideo*v){
    int requested;
    if(!v||!v->species_dirty)return -1;
    v->species_dirty=0;
    requested=v->species_request;
    return requested;
}int avp_pc_video_take_hz_delta(AvpPcVideo*v){int d=v?v->hz_delta:0;if(v)v->hz_delta=0;return d;}int avp_pc_video_smooth_presentation(const AvpPcVideo*v){return v?v->cfg.smooth_presentation:0;}unsigned avp_pc_video_presentation_hz(const AvpPcVideo*v){return avp_presentation_valid_rate(v?v->cfg.presentation_hz:60u);}int avp_pc_video_menu_open(const AvpPcVideo*v){return v?v->menu_open:0;}int avp_pc_video_full_cpu_systems(const AvpPcVideo*v){return v?v->cfg.full_cpu_systems:0;}int avp_pc_video_marine_start_shotgun(const AvpPcVideo*v){return v?v->cfg.marine_start_shotgun:0;}int avp_pc_video_debug_immediate_in_game(const AvpPcVideo*v){return v?v->cfg.debug_immediate_in_game:0;}unsigned avp_pc_video_take_actions(AvpPcVideo*v){unsigned a=v?v->pending_actions:0u;if(v)v->pending_actions=0u;return a;}
int avp_pc_video_take_refresh_level(AvpPcVideo *v)
{
    if(!v || !(v->pending_actions&AVP_PC_ACTION_REFRESH_LEVEL))return 0;
    v->pending_actions&=~AVP_PC_ACTION_REFRESH_LEVEL;
    return 1;
}
void avp_pc_video_prepare_title_return(AvpPcVideo *v)
{
    if(!v)return;
    v->title_return_wait_release=1;
    v->pad_buffered=0;
    v->use_button_buffered=0;
    v->pending_actions=0;
    v->menu_open=0;
}
int avp_pc_video_take_restart_title(AvpPcVideo *v)
{
    if(!v || !(v->pending_actions&AVP_PC_ACTION_RESTART_TITLE))return 0;
    v->pending_actions&=~AVP_PC_ACTION_RESTART_TITLE;
    return 1;
}int avp_pc_video_infinite_health(const AvpPcVideo*v){return v?v->infinite_health:0;}int avp_pc_video_infinite_ammo(const AvpPcVideo*v){return v?v->infinite_ammo:0;}
int avp_pc_video_freeze_enemies(const AvpPcVideo*v){return v?v->freeze_enemies:0;}
int avp_pc_video_attacks_disabled(const AvpPcVideo*v){return v?v->attacks_disabled:0;}
int avp_pc_video_marine_pulse_los_hits(const AvpPcVideo*v){return v?v->cfg.marine_pulse_los_hits:0;}
int avp_pc_video_unlimited_shooting_range(const AvpPcVideo*v){return v?v->cfg.unlimited_shooting_range:0;}
int avp_pc_video_marine_fire_interrupt(const AvpPcVideo*v){return v?v->cfg.marine_fire_interrupt:0;}
int avp_pc_video_normalize_diagonal_contact_range(const AvpPcVideo*v){return v?v->cfg.normalize_diagonal_contact_range:0;}
unsigned avp_pc_video_enemy_activation_distance(const AvpPcVideo*v)
{return v&&v->cfg.enemy_activation_distance>=0&&v->cfg.enemy_activation_distance<5?(unsigned)v->cfg.enemy_activation_distance:0u;}
int avp_pc_video_facehug_attach_disabled(const AvpPcVideo*v){return v?v->facehug_attach_disabled:0;}
int avp_pc_video_kill_selected_type(const AvpPcVideo*v){
    static const int kinds[8]={AC_ALIEN,AC_ACRAWL,AC_HUMAN,AC_PREDATOR,AC_FACEHUG,AC_EGG,AC_AQUEEN,AC_GEN};
    return kinds[v?(unsigned)v->kill_type_choice%8u:0u];
}
int avp_pc_video_original_god_player(const AvpPcVideo *v,unsigned player)
{return v && player<4u && v->orig_banana[player] && v->orig_god[player];}
int avp_pc_video_original_god(const AvpPcVideo *v)
{return avp_pc_video_original_god_player(v,0u);}
int avp_pc_video_original_positioning_player(const AvpPcVideo *v,unsigned player)
{return v && player<4u && v->orig_position[player];}
int avp_pc_video_original_positioning(const AvpPcVideo *v)
{return avp_pc_video_original_positioning_player(v,0u);}
void avp_pc_video_original_reset(AvpPcVideo *v)
{
    if(!v)return;
    memset(v->orig_fuji,0,sizeof(v->orig_fuji));
    memset(v->orig_banana,0,sizeof(v->orig_banana));
    memset(v->orig_position,0,sizeof(v->orig_position));
    memset(v->orig_god,0,sizeof(v->orig_god));
    for(unsigned p=0;p<4u;p++)avp_pc_original_input_reset(&v->orig_input[p]);
    v->pending_actions &= ~(AVP_PC_ACTION_ORIG_TRACKER_TOGGLE|
        AVP_PC_ACTION_ORIG_WEAPON1|AVP_PC_ACTION_ORIG_WEAPON2|
        AVP_PC_ACTION_ORIG_WEAPON3|AVP_PC_ACTION_ORIG_WEAPON4|
        AVP_PC_ACTION_ORIG_LEVEL_NEXT|AVP_PC_ACTION_ORIG_LEVEL_PREVIOUS);
#ifdef AVP_LOCAL_MULTIPLAYER
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_PLAYERS;p++)
        v->local_mp_cheat_actions[p] &= ~(AVP_PC_ACTION_ORIG_TRACKER_TOGGLE|
            AVP_PC_ACTION_ORIG_WEAPON1|AVP_PC_ACTION_ORIG_WEAPON2|
            AVP_PC_ACTION_ORIG_WEAPON3|AVP_PC_ACTION_ORIG_WEAPON4|
            AVP_PC_ACTION_ORIG_LEVEL_NEXT|AVP_PC_ACTION_ORIG_LEVEL_PREVIOUS);
#endif
}
void avp_pc_video_original_gps_actor(AvpPcVideo *v,unsigned player,
    unsigned level,u32 x,u32 y)
{
    if(v && player<4u){v->orig_gps_x[player]=x;v->orig_gps_y[player]=y;
        v->orig_gps_level[player]=level;}
}
void avp_pc_video_original_actor_status(AvpPcVideo *v,unsigned player,
    unsigned species,unsigned weapons,int tracker)
{
    if(!v || player>=4u)return;
    v->orig_species[player]=species;
    v->orig_weapons[player]=weapons;
    v->orig_tracker[player]=!!tracker;
    v->orig_actor_valid[player]=1;
}
int avp_pc_video_tracker_cheat(const AvpPcVideo*v){return v?v->tracker_cheat:0;}
int avp_pc_video_tracker_sounds_enabled(const AvpPcVideo*v){
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v && v->local_mp_hud_owner>=1 && v->local_mp_hud_owner<4)
        return v->local_mp_controls.player[v->local_mp_hud_owner].audio_motion_tracker;
#endif
    return v?v->cfg.tracker_sounds_enabled:1;
}
int avp_pc_video_player_pain_sounds_enabled(const AvpPcVideo*v){return v?v->cfg.player_pain_sounds_enabled:1;}
int avp_pc_video_facehug_pain_sounds_enabled(const AvpPcVideo*v){return v?v->cfg.facehug_pain_sounds_enabled:1;}
int avp_pc_video_facehug_shake_sounds_enabled(const AvpPcVideo*v){return v?v->cfg.facehug_shake_pain_sounds_enabled:1;}
void avp_pc_video_snapshot_sim(AvpPcVideo*v,const u8*maze,u16 mw,u16 mh){if(v)avp_gpu_snapshot_sim(&v->gpu,maze,mw,mh);}
void avp_pc_video_set_render_alpha(AvpPcVideo*v,double a){if(v)avp_gpu_set_render_alpha(&v->gpu,a);}
static const char*key_name(unsigned long k){const char*n=XKeysymToString((KeySym)k);return n?n:"Unknown";}

#ifdef AVP_LOCAL_MULTIPLAYER
/* Host-only menu for P2-4; never edits the legacy Player 1 settings. */
static void mp_controls_adjust(AvpPcVideo *v,int row,int dir,int accept)
{
    AvpMpInputProfile *p=&v->local_mp_controls.player[v->local_mp_control_player];
    if(row>=AVP_MP_CONTROL_BIND_ROW+(int)AVP_MP_INPUT_ACTIONS){
        int n=row-AVP_MP_CONTROL_BIND_ROW-(int)AVP_MP_INPUT_ACTIONS;
        int *const vals[]={&p->audio_motion_tracker,&p->audio_player_pain,
            &p->audio_facehug_pain,&p->audio_facehug_shake,&p->remember_automap};
        if(n<0||n>=5)return;
        if(v->local_mp_control_player==0){
            switch(n){
            case 0:v->cfg.tracker_sounds_enabled=!v->cfg.tracker_sounds_enabled;break;
            case 1:v->cfg.player_pain_sounds_enabled=!v->cfg.player_pain_sounds_enabled;break;
            case 2:v->cfg.facehug_pain_sounds_enabled=!v->cfg.facehug_pain_sounds_enabled;break;
            case 3:v->cfg.facehug_shake_pain_sounds_enabled=!v->cfg.facehug_shake_pain_sounds_enabled;break;
            case 4:v->cfg.remember_automap=!v->cfg.remember_automap;break;
            }
        }else *vals[n]=!*vals[n];
        save_settings(v);return;
    }
    if(row==0){v->local_mp_control_player=(v->local_mp_control_player+ (dir>0?1u:3u))%4u;return;}
    /* P1's key/action/joystick settings remain controlled by original F12.
     * This page may set only the dedicated P1 co-op physical-mouse ID. */
    if(v->local_mp_control_player==0 && row!=2 &&
       (row<AVP_MP_CONTROL_ANALOG_ROW || (row>=AVP_MP_CONTROL_BIND_ROW &&
        row<AVP_MP_CONTROL_BIND_ROW+AVP_MP_MAP_ZOOM_IN)))return;
    if(row==1){v->local_mp_control_kind=(v->local_mp_control_kind+(dir>0?1u:2u))%3u;return;}
    if(row==2){
        AvpMpPhysicalMouse mice[AVP_MP_DISCOVER_MICE];unsigned n=avp_mp_mice_discover(mice),i;
        int current=-1;
        for(i=0;i<n;i++)if(!strcmp(p->mouse_path,mice[i].path)){current=(int)i;break;}
        if(n==0){(void)avp_mp_input_assign_mouse(&v->local_mp_controls,v->local_mp_control_player,"");return;}
        /* Cycle available unique mice, with an explicit Unassigned choice. */
        for(i=0;i<=n;i++){
            int idx=current+(i+1)*dir;
            while(idx< -1)idx+=(int)n+1;
            idx%=(int)n+1;
            if(idx==-1 || idx==(int)n){(void)avp_mp_input_assign_mouse(&v->local_mp_controls,v->local_mp_control_player,"");break;}
            if(avp_mp_input_assign_mouse(&v->local_mp_controls,v->local_mp_control_player,mice[idx].path))break;
        }
    }else if(row==3){
        const char *paths[AVP_JS_MAX_DEVICES];
        const char *p1_device=NULL;
        unsigned count;
        enumerate_js(v);count=(unsigned)v->device_count;
        for(unsigned i=0;i<count;i++)paths[i]=v->devices[i].path;
        /* P1 uses the original host joystick. When auto-selected, reserve
         * the actual open path (or the first discovered pad before opening).
         * Never expose it as a free P2-P4 device. */
        if(v->cfg.gamepad_device[0])p1_device=v->cfg.gamepad_device;
        else if(v->cfg.gamepad_enabled){
            if(v->js_fd>=0 && v->js_path[0])p1_device=v->js_path;
            else if(count)p1_device=paths[0];
        }
        (void)avp_mp_input_cycle_gamepad(&v->local_mp_controls,
            v->local_mp_control_player,paths,count,p1_device,dir);
    }else if(row==4)p->mouse_enabled=!p->mouse_enabled;
    else if(row==5){p->mouse_sensitivity+=dir*0.1f;}
    else if(row==6){p->mouse_acceleration+=dir*0.1f;}
    else if(row==7)p->gamepad_enabled=!p->gamepad_enabled;
    else if(row==8)p->gamepad_deadzone+=dir*0.05f;
    else if(row>=9&&row<=11){int *a=&p->gamepad_axis[row-9];*a+=dir;if(*a>15)*a=-1;if(*a< -1)*a=15;}
    else if(row>=12&&row<=14)p->gamepad_axis_invert[row-12]=!p->gamepad_axis_invert[row-12];
    else if(row==AVP_MP_CONTROL_ANALOG_ROW){
        if(v->local_mp_control_player==0)v->cfg.analog_movement=!v->cfg.analog_movement;
        else p->gamepad_analog_movement=!p->gamepad_analog_movement;
    }else if(row==AVP_MP_CONTROL_ENHANCED_ROW){
        if(v->local_mp_control_player==0)v->cfg.enhanced_analog_controls=!v->cfg.enhanced_analog_controls;
        else p->gamepad_enhanced_analog_controls=!p->gamepad_enhanced_analog_controls;
    }else if(row==AVP_MP_CONTROL_SENS_ROW){
        if(v->local_mp_control_player==0)v->cfg.turn_sensitivity+=dir*0.1f;
        else p->gamepad_turn_sensitivity+=dir*0.1f;
    }else if(row==AVP_MP_CONTROL_SPEED_ROW){
        if(v->local_mp_control_player==0)v->cfg.turn_speed+=dir*0.1f;
        else p->gamepad_turn_speed+=dir*0.1f;
    }else if(row>=AVP_MP_CONTROL_BIND_ROW&&row<AVP_MP_CONTROL_BIND_ROW+(int)AVP_MP_INPUT_ACTIONS){
        unsigned a=(unsigned)(row-AVP_MP_CONTROL_BIND_ROW);
        if(v->local_mp_control_kind==0){
            if(accept){v->capture_mode=6;v->capture_target=(int)a;return;}
        }else if(v->local_mp_control_kind==1){
            if(accept){v->capture_mode=8;v->capture_target=(int)a;return;}
        }else if(accept){
            AvpMpPadReader *r=&v->local_mp_pad_readers[v->local_mp_control_player];
            AvpMpControllerSample sample={0};
            if(v->local_mp_control_player==0){
                if(!v->cfg.gamepad_enabled || v->js_fd<0)return;
                v->capture_mode=7;v->capture_target=(int)a;return;
            }
            if(!p->gamepad_path[0])return; /* Choose physical device first. */
            if(strcmp(r->path,p->gamepad_path)){
                if(r->path[0])avp_mp_pad_reader_close(r);
                if(!avp_mp_pad_reader_open(r,p->gamepad_path))return;
            }
            if(avp_mp_pad_reader_poll(r,&sample)<0)return;
            v->local_mp_capture_prev_buttons=sample.buttons;
            memcpy(v->local_mp_capture_prev_axes,sample.axes,
                   sizeof(v->local_mp_capture_prev_axes));
            v->capture_mode=7;v->capture_target=(int)a;return;
        }
    }
    if(v->cfg.turn_sensitivity<0.25f)v->cfg.turn_sensitivity=0.25f;
    if(v->cfg.turn_sensitivity>5.0f)v->cfg.turn_sensitivity=5.0f;
    if(v->cfg.turn_speed<0.25f)v->cfg.turn_speed=0.25f;
    if(v->cfg.turn_speed>5.0f)v->cfg.turn_speed=5.0f;
    avp_mp_input_validate(&v->local_mp_controls);save_settings(v);
}
static void draw_mp_controls(AvpPcVideo *v,Drawable target)
{
    const char *const fixed[]={"Configure Player","Binding Device Type","Physical Mouse","Gamepad Device",
        "Mouse Enabled","Mouse Sensitivity","Mouse Acceleration","Gamepad Enabled","Gamepad Deadzone",
        "Forward/Back Axis","Strafe Axis","Turn Axis","Invert Forward Axis","Invert Strafe Axis","Invert Turn Axis",
        "Controller Analog Movement","Enhanced Analog Controls","Controller Turn Sensitivity","Turn Speed"};
    const AvpMpInputProfile *p=&v->local_mp_controls.player[v->local_mp_control_player];
    unsigned ww=v->win_w?v->win_w:640u,wh=v->win_h?v->win_h:480u;
    unsigned boxw=ww>800u?760u:(ww>24u?ww-24u:0u);
    int count=menu_count(v),visible=(int)((wh>72u?wh-72u:18u)/18u),first,row;char line[520],value[256];
    if(visible<3)visible=3;
    if(visible>count)visible=count;
    first=v->menu_cursor-visible/2;if(first<0)first=0;if(first>count-visible)first=count-visible;
    XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
    XFillRectangle(v->dpy,target,v->gc,12,12,boxw,(unsigned)(visible*18+52));
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
    snprintf(line,sizeof(line),"PLAYER %u CONTROLS - %s%s",v->local_mp_control_player+1,
        v->local_mp_control_kind==0?"Keyboard":v->local_mp_control_kind==1?"Mouse":"Gamepad",
        v->capture_mode==6?" - PRESS A KEY (Esc cancels)":
        v->capture_mode==7?" - PRESS GAMEPAD BUTTON / AXIS (Esc cancels)":
        v->capture_mode==8?" - CLICK MOUSE BUTTON (Esc cancels)":"");
    XDrawString(v->dpy,target,v->gc,36,30,line,(int)strlen(line));
    for(row=first;row<first+visible;row++){
        static const char *const extra[]={"Audio: Motion Tracker Sounds",
            "Audio: Player Pain Sounds","Audio: Facehugged Pain Sounds",
            "Audio: Facehug Shake Pain Sounds","Enhancement: Remember Explored Automap"};
        int opt=row-AVP_MP_CONTROL_BIND_ROW-(int)AVP_MP_INPUT_ACTIONS;
        const char *label=row<AVP_MP_CONTROL_BIND_ROW?fixed[row]:
            (opt>=0&&opt<5?extra[opt]:avp_mp_input_action_name((unsigned)(row-AVP_MP_CONTROL_BIND_ROW)));
        value[0]=0;
        if(row==0)snprintf(value,sizeof(value),"Player %u%s",v->local_mp_control_player+1,
            v->local_mp_control_player==0?" (legacy bindings)":"");
        else if(row==1)snprintf(value,sizeof(value),"%s",v->local_mp_control_kind==0?"Keyboard":v->local_mp_control_kind==1?"Mouse":"Gamepad");
        else if(row==2)snprintf(value,sizeof(value),"%s",p->mouse_path[0]?p->mouse_path:"Unassigned (Linux evdev)");
        else if(row==3)snprintf(value,sizeof(value),"%s",p->gamepad_path[0]?p->gamepad_path:"Unassigned");
        else if(row==4)snprintf(value,sizeof(value),"%s",p->mouse_enabled?"On":"Off");
        else if(row==5)snprintf(value,sizeof(value),"%.2f",p->mouse_sensitivity);
        else if(row==6)snprintf(value,sizeof(value),"%.2f",p->mouse_acceleration);
        else if(row==7)snprintf(value,sizeof(value),"%s",p->gamepad_enabled?"On":"Off");
        else if(row==8)snprintf(value,sizeof(value),"%.2f",p->gamepad_deadzone);
        else if(row>=9&&row<=11)snprintf(value,sizeof(value),"%d",p->gamepad_axis[row-9]);
        else if(row>=12&&row<=14)snprintf(value,sizeof(value),"%s",p->gamepad_axis_invert[row-12]?"Yes":"No");
        else if(row==AVP_MP_CONTROL_ANALOG_ROW)snprintf(value,sizeof(value),"%s",
            (v->local_mp_control_player==0?v->cfg.analog_movement:p->gamepad_analog_movement)?"Enabled":"Disabled");
        else if(row==AVP_MP_CONTROL_ENHANCED_ROW)snprintf(value,sizeof(value),"%s",
            (v->local_mp_control_player==0?v->cfg.enhanced_analog_controls:p->gamepad_enhanced_analog_controls)?"Enabled":"Disabled");
        else if(row==AVP_MP_CONTROL_SENS_ROW)snprintf(value,sizeof(value),"%.2fx",
            v->local_mp_control_player==0?v->cfg.turn_sensitivity:p->gamepad_turn_sensitivity);
        else if(row==AVP_MP_CONTROL_SPEED_ROW)snprintf(value,sizeof(value),"%.2fx",
            v->local_mp_control_player==0?v->cfg.turn_speed:p->gamepad_turn_speed);
        else if(opt>=0 && opt<5){
            const int vals[]={p->audio_motion_tracker,p->audio_player_pain,
                p->audio_facehug_pain,p->audio_facehug_shake,p->remember_automap};
            const int legacy[]={v->cfg.tracker_sounds_enabled,v->cfg.player_pain_sounds_enabled,
                v->cfg.facehug_pain_sounds_enabled,v->cfg.facehug_shake_pain_sounds_enabled,
                v->cfg.remember_automap};
            snprintf(value,sizeof(value),"%s",
                (v->local_mp_control_player==0?legacy[opt]:vals[opt])?"Enabled":"Disabled");
        }
        else if(v->local_mp_control_kind==0){unsigned long k=p->key[row-AVP_MP_CONTROL_BIND_ROW];snprintf(value,sizeof(value),"%s",k?key_name(k):"Unbound");}
        else if(v->local_mp_control_kind==1){int b=p->mouse_button[row-AVP_MP_CONTROL_BIND_ROW];snprintf(value,sizeof(value),b<0?"Unbound":"Mouse %d",b);}
        else{int b=p->gamepad_button[row-AVP_MP_CONTROL_BIND_ROW];
            if(b==-1)snprintf(value,sizeof(value),"Unbound");
            else if(b< -1){int n=-b-2;snprintf(value,sizeof(value),"Axis %d %s",n/2,(n&1)?"+":"-");}
            else snprintf(value,sizeof(value),"Button %d",b);
        }
        snprintf(line,sizeof(line),"%c %-23s : %.170s",row==v->menu_cursor?'>':' ',label,value);
        XDrawString(v->dpy,target,v->gc,36,52+(row-first)*18,line,(int)strlen(line));
    }
    {const char *hint="Arrows: Change | Enter: Bind | Delete: Unbind | Backspace: Back";
     XDrawString(v->dpy,target,v->gc,36,52+visible*18+9,hint,(int)strlen(hint));}
}
#endif

/* The editor is not a row menu and deliberately has menu_count()==0.
 * Route held arrows and gamepad navigation to selected-cell movement. */
static void map_maker_navigation_step(AvpMapMaker *m,int direction)
{
    if(direction==1)avp_mm_move(m,0,-1);
    else if(direction==2)avp_mm_move(m,0,1);
    else if(direction==3)avp_mm_move(m,-1,0);
    else if(direction==4)avp_mm_move(m,1,0);
}

/* 350 ms first repeat then 75 ms, matching held arrow-key navigation.
 * Keyboard first presses are handled by existing X11 event code. Controller
 * first presses are handled here and share the same repeat timer. No binding
 * capture, player movement, or Quick Turn events are synthesized. */
static void mp_menu_navigation_step(AvpPcVideo *v,int direction)
{
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_open && (v->frontend_input || v->local_mp_walktest) && !v->menu_open){
        if(direction==1 || direction==2)
            v->local_mp_cursor=avp_local_mp_menu_next_row(&v->local_mp,
                v->local_mp_page,v->local_mp_cursor,direction==1?-1:1);
        else if((direction==3||direction==4) &&
                v->local_mp_cursor<AVP_LOCAL_MP_ROWS &&
                v->local_mp_cursor!=AVP_LOCAL_MP_START_ROW)
            if(avp_local_mp_adjust(&v->local_mp,v->local_mp_cursor,direction==3?-1:1))
                save_settings(v);
        v->local_mp_suppress_input=1;
        return;
    }
#endif
    if(!v->menu_open||v->capture_mode)return;
    /* Map Maker is a canvas, not a row menu.  In particular its menu_count is
     * zero; treating held arrows or gamepad directions as row navigation
     * would divide by zero.  Move the selected map cell instead. */
    if(v->menu_page==MENU_MAP_MAKER){
        AvpMapMaker *m=&v->map_maker;
        if(m->browser_open){
            if(direction==1 && m->browser_cursor>0u)m->browser_cursor--;
            if(direction==2 && m->browser_cursor+1u<m->browser_count)m->browser_cursor++;
            return;
        }
        if(m->palette_open||m->context_open||m->name_edit_open)return;
        if(!m->mode_3d)map_maker_navigation_step(m,direction);
        /* Held arrows in 3D are consumed by the dt-based camera path.
         * Repeat here would add unwanted 350/75 ms steering jumps. */
        return;
    }
    if(direction==1)v->menu_cursor=(v->menu_cursor+menu_count(v)-1)%menu_count(v);
    else if(direction==2)v->menu_cursor=(v->menu_cursor+1)%menu_count(v);
    else if(direction==3||direction==4){
#ifdef AVP_LOCAL_MULTIPLAYER
        if(v->menu_page==MENU_MP_CONTROLS){
            mp_controls_adjust(v,v->menu_cursor,direction==3?-1:1,0);
        }else
#endif
        {if(v->menu_page==MENU_VIDEO)menu_video_adjust(v,v->menu_cursor,direction==3?-1:1);
         else if(v->menu_page==MENU_AUDIO)menu_audio_adjust(v,v->menu_cursor,direction==3?-1:1);
         else if(v->menu_page==MENU_CHEATS && v->menu_cursor==2)menu_cheats_target_adjust(v,direction==3?-1:1);
         else if(v->menu_page==MENU_CHEATS_FUJI||v->menu_page==MENU_CHEATS_BANANA||v->menu_page==MENU_CHEATS_GPS||v->menu_page==MENU_CHEATS_LEVEL)
             original_cheat_adjust(v,v->menu_cursor,direction==3?-1:1);
         else {int id=menu_id(v,v->menu_cursor);
          if(id>=0 && !(v->menu_page==MENU_CHEATS_NEW && (id==175||id==176))){v->menu_index=id;menu_adjust(v,direction==3?-1:1);}}}
        mouse_grab_update(v);
    }
}
static int mp_menu_held_direction(const AvpPcVideo *v,int *is_key)
{
    const int k[]={XK_Up,XK_Down,XK_Left,XK_Right};
    const unsigned long configured[]={v->cfg.menu_key_up,v->cfg.menu_key_down,
                                      v->cfg.menu_key_left,v->cfg.menu_key_right};
    for(int i=0;i<4;i++)if(key(v,(KeySym)k[i])||
        (configured[i]&&key(v,(KeySym)configured[i]))){*is_key=1;return i+1;}
    {const int buttons[]={v->cfg.menu_button_up,v->cfg.menu_button_down,
                          v->cfg.menu_button_left,v->cfg.menu_button_right};
     for(int i=0;i<4;i++)if(button_down(v,buttons[i])){*is_key=0;return i+1;}}
    return 0;
}
static void mp_menu_repeat_poll(AvpPcVideo *v)
{
    int is_key=0,direction;
    uint64_t now;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(!v->menu_open&&!v->local_mp_open){v->menu_repeat_direction=0;return;}
#else
    if(!v->menu_open){v->menu_repeat_direction=0;return;}
#endif
    if(v->capture_mode){v->menu_repeat_direction=0;return;}
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_open && !v->menu_open){
        if(button_edge(v,v->cfg.menu_button_back)){
            mp_setup_back(v);v->local_mp_suppress_input=1;
            v->menu_repeat_direction=0;return;
        }
        if(button_edge(v,v->cfg.menu_button_accept)){
            mp_setup_accept(v);v->local_mp_suppress_input=1;
            v->menu_repeat_direction=0;return;
        }
    }
#endif
    direction=mp_menu_held_direction(v,&is_key);
    /* R33: Keyboard arrows in the Map Maker have their own single repeat
     * driver (mm_nav_direction below), which preserves a Shift-selection's
     * fixed anchor. Letting this generic F12-menu repeat also move the cell
     * called avp_mm_move() without Shift and erased the selection every 75ms.
     * Keep controller D-pad navigation here; it has no keyboard driver. */
    if(v->menu_page==MENU_MAP_MAKER && is_key){
        v->menu_repeat_direction=0;
        return;
    }
    if(!direction){v->menu_repeat_direction=0;return;}
    now=avp_pc_now_ns();
    if(v->menu_repeat_direction!=direction){
        v->menu_repeat_direction=direction;
        v->menu_repeat_next_ns=now+UINT64_C(350000000);
        if(!is_key)mp_menu_navigation_step(v,direction);
        return;
    }
    if(now>=v->menu_repeat_next_ns){
        mp_menu_navigation_step(v,direction);
        v->menu_repeat_next_ns=now+UINT64_C(75000000);
    }
}

/* The editor is a non-destructive PC staging tool. No active maze is rebound. */
void avp_pc_video_map_maker_rom_path(AvpPcVideo *v,const char *path)
{
    if(v)snprintf(v->map_test_rom_path,sizeof v->map_test_rom_path,"%s",path?path:"");
}
void avp_pc_video_map_maker_bind(AvpPcVideo *v,unsigned level,const u8 *decoded,size_t bytes)
{
    if(v)(void)avp_mm_bind(&v->map_maker,level,decoded,bytes);
}
/* All editor-created maps live beside the executable in custom_maps.
 * Only safe bare stems are accepted: no traversal or arbitrary file writes. */
static int map_maker_directory(char *out,size_t cap,int create)
{
    char exe[1024];char *sep;
#ifdef _WIN32
    DWORD n=GetModuleFileNameA(NULL,exe,sizeof exe);
    if(!n||n>=sizeof exe)return 0;
#else
    ssize_t n=readlink("/proc/self/exe",exe,sizeof exe-1u);
    if(n<=0||(size_t)n>=sizeof exe-1u)return 0;
#endif
    exe[n]=0;sep=strrchr(exe,'/');
#ifdef _WIN32
    {char *back=strrchr(exe,'\\');if(back&&(!sep||back>sep))sep=back;}
#endif
    if(!sep)return 0;
    *sep=0;
    if(snprintf(out,cap,"%s/custom_maps",exe)>=(int)cap)return 0;
    if(create){
#ifdef _WIN32
        if(_mkdir(out)!=0&&errno!=EEXIST)return 0;
#else
        if(mkdir(out,0755)!=0&&errno!=EEXIST)return 0;
#endif
    }
    return 1;
}
static int map_maker_stem_valid(const char *stem)
{
    size_t n=strlen(stem);
    if(n<1u||n>80u)return 0;
    for(size_t i=0;i<n;i++)if(!((stem[i]>='a'&&stem[i]<='z')||
        (stem[i]>='A'&&stem[i]<='Z')||(stem[i]>='0'&&stem[i]<='9')||
        stem[i]=='_'||stem[i]=='-'))return 0;
    return 1;
}
static int map_maker_named_path(const char *stem,const char *extension,
                                char *out,size_t cap,int create)
{
    char dir[1024];
    if(!map_maker_stem_valid(stem)||!map_maker_directory(dir,sizeof dir,create))return 0;
    return snprintf(out,cap,"%s/%s.%s",dir,stem,extension)<(int)cap;
}
static int map_maker_save_named(AvpPcVideo *v,const char *stem)
{
    AvpMapMaker *m=&v->map_maker;char map[1200],objects[1200];
    if(!map_maker_named_path(stem,"avpm",map,sizeof map,1)||
       !map_maker_named_path(stem,"avpo",objects,sizeof objects,1))return 0;
    if(!avp_mm_save(m,map)||!avp_mm_objects_save(m,objects))return 0;
    snprintf(m->map_stem,sizeof m->map_stem,"%s",stem);
    snprintf(m->message,sizeof m->message,"Saved custom_maps/%s.avpm",stem);
    return 1;
}
static int map_maker_load_named(AvpPcVideo *v,const char *stem)
{
    AvpMapMaker *m=&v->map_maker;char map[1200],objects[1200];
    unsigned level;unsigned char header[20];FILE *f;
    if(!map_maker_named_path(stem,"avpm",map,sizeof map,0)||
       !map_maker_named_path(stem,"avpo",objects,sizeof objects,0))return 0;
    f=fopen(map,"rb");if(!f)return 0;
    if(fread(header,1,sizeof header,f)!=sizeof header||memcmp(header,"AVPMAP01",8)){
        fclose(f);return 0;
    }
    fclose(f);level=(unsigned)header[8]*256u+header[9];
    if(level<1u||level>AVP_MM_LEVELS)return 0;
    /* Loading is transactional, even when the file references another level. */
    AvpMapMaker saved=*m;
    if(!avp_mm_select(m,level)||!avp_mm_load(m,map)){
        *m=saved;return 0;
    }
    m->object_count=0u;
    /* An absent sidecar is valid for maps without user-placed objects. */
    (void)avp_mm_objects_load(m,objects);
    m->editor_started=1;m->context_open=m->browser_open=m->name_edit_open=0;
    snprintf(m->map_stem,sizeof m->map_stem,"%s",stem);
    snprintf(m->message,sizeof m->message,"Loaded custom_maps/%s.avpm",stem);
    return 1;
}
static int map_maker_file_valid(const char *stem)
{
    char path[1200];unsigned char hdr[20];unsigned level;FILE *f;
    if(!map_maker_named_path(stem,"avpm",path,sizeof path,0))return 0;
    f=fopen(path,"rb");if(!f)return 0;
    int ok=fread(hdr,1,sizeof hdr,f)==sizeof hdr &&
           (!memcmp(hdr,"AVPMAP01",8u) || !memcmp(hdr,"AVPMAP02",8u));
    if(fclose(f)!=0)ok=0;
    if(!ok)return 0;
    level=(unsigned)hdr[8]*256u+(unsigned)hdr[9];
    if(level<1u||level>15u)return 0;
    AvpMapMaker scratch;avp_mm_init(&scratch);scratch.level=level;
    ok=avp_mm_load(&scratch,path);
    avp_mm_destroy(&scratch);
    return ok;
}
static int map_maker_browser_cmp(const void *a,const void *b)
{ return strcmp((const char *)a,(const char *)b); }
static void map_maker_browser_scan(AvpMapMaker *m)
{
    char dir[1024];m->browser_count=m->browser_cursor=0;
    if(!map_maker_directory(dir,sizeof dir,1))return;
#ifndef _WIN32
    DIR *d=opendir(dir);struct dirent *entry;
    if(!d)return;
    while((entry=readdir(d))!=NULL && m->browser_count<128u){
        size_t len=strlen(entry->d_name);
        if(len<=5u||strcmp(entry->d_name+len-5u,".avpm"))continue;
        char stem[96];if(len-5u>=sizeof stem)continue;
        memcpy(stem,entry->d_name,len-5u);stem[len-5u]=0;
        if(map_maker_stem_valid(stem)&&map_maker_file_valid(stem)){
            snprintf(m->browser_files[m->browser_count++],96,"%s",stem);
        }
    }
    closedir(d);
#else
    WIN32_FIND_DATAA found;char glob[1100];
    snprintf(glob,sizeof glob,"%s/*.avpm",dir);
    HANDLE h=FindFirstFileA(glob,&found);
    if(h!=INVALID_HANDLE_VALUE){
        do{
            size_t len=strlen(found.cFileName);
            if(len<=5u||strcmp(found.cFileName+len-5u,".avpm")||len-5u>=96u)continue;
            char stem[96];memcpy(stem,found.cFileName,len-5u);stem[len-5u]=0;
            if(map_maker_stem_valid(stem)&&map_maker_file_valid(stem))snprintf(m->browser_files[m->browser_count++],96,"%s",stem);
        }while(m->browser_count<128u&&FindNextFileA(h,&found));
        FindClose(h);
    }
#endif
    qsort(m->browser_files,m->browser_count,sizeof m->browser_files[0],map_maker_browser_cmp);
}
/* Playtesting is a SEPARATE native game process, not a rebind of the
 * campaign/editor's live Jaguar globals. The temporary AVPMAP01 copy is
 * consumed by the child and its memory-backed EEPROM prevents save damage. */
static void map_maker_object_edit(AvpMapMaker *m,int place)
{
    unsigned x=m->mode_3d?(unsigned)m->camera_x:m->x;
    unsigned y=m->mode_3d?(unsigned)m->camera_y:m->y;
    if(place){
        double px=m->mode_3d?m->camera_x:(double)m->x+0.5;
        double py=m->mode_3d?m->camera_y:(double)m->y+0.5;
        if(!avp_mm_object_place(m,px,py))
            snprintf(m->message,sizeof m->message,"Cannot place object: invalid floor or full list.");
    }else if(!avp_mm_object_remove(m,x,y))
        snprintf(m->message,sizeof m->message,"No staged object at selected location.");
}
static void map_maker_play_test(AvpPcVideo *v)
{
    AvpMapMaker *m=&v->map_maker;
    double x=m->mode_3d?m->camera_x:(double)m->x+0.5;
    double y=m->mode_3d?m->camera_y:(double)m->y+0.5;
#ifndef _WIN32
    char exe[640],tmp[]="/tmp/avp_mm_test_XXXXXX",tmpobjects[100];
    char levelstr[16],xstr[48],ystr[48],headingstr[48];
    char *args[19];pid_t child;int fd,rc;ssize_t sz;
    AvpMapMaker staged;
    if(v->map_test_child>0){
        if(waitpid(v->map_test_child,NULL,WNOHANG)==0){
            snprintf(m->message,sizeof m->message,"Close the current playtest before opening another.");return;
        }
        v->map_test_child=0;
    }
    if(!v->map_test_rom_path[0]||!m->source[m->level].original){
        snprintf(m->message,sizeof m->message,"Play Test requires the verified original retail ROM.");return;
    }
    if(!avp_mm_test_start_valid(m,x,y)){
        snprintf(m->message,sizeof m->message,"Invalid start: choose a floor cell clear of walls.");return;
    }
    sz=readlink("/proc/self/exe",exe,sizeof exe-1u);
    if(sz<=0||(size_t)sz>=sizeof exe-1u){
        snprintf(m->message,sizeof m->message,"Cannot find game executable for playtest.");return;
    }
    exe[sz]=0;fd=mkstemp(tmp);
    if(fd<0){snprintf(m->message,sizeof m->message,"Cannot allocate playtest draft file.");return;}
    close(fd);
    staged=*m; /* Save without marking the actual editor draft clean. */
    snprintf(tmpobjects,sizeof tmpobjects,"%s.avpo",tmp);
    if(!avp_mm_save(&staged,tmp)||!avp_mm_objects_save(&staged,tmpobjects)){
        unlink(tmp);unlink(tmpobjects);
        snprintf(m->message,sizeof m->message,"Cannot write playtest snapshot.");return;
    }
    snprintf(levelstr,sizeof levelstr,"%u",m->level);
    snprintf(xstr,sizeof xstr,"%.9f",x);snprintf(ystr,sizeof ystr,"%.9f",y);
    snprintf(headingstr,sizeof headingstr,"%.12f",m->camera_heading);
    args[0]=exe;args[1]="--rom";args[2]=v->map_test_rom_path;
    args[3]="--map-test";args[4]=tmp;args[5]="--map-test-level";args[6]=levelstr;
    args[7]="--map-test-x";args[8]=xstr;args[9]="--map-test-y";args[10]=ystr;
    args[11]="--map-test-heading";args[12]=headingstr;
    args[13]="--map-test-objects";args[14]=tmpobjects;
    args[15]=m->playtest_no_enemies?"--map-test-no-enemies":NULL;
    args[16]=NULL;
    rc=posix_spawn(&child,exe,NULL,NULL,args,environ);
    if(rc){unlink(tmp);unlink(tmpobjects);snprintf(m->message,sizeof m->message,"Play Test launch failed (code %d).",rc);return;}
    v->map_test_child=child;
    snprintf(m->message,sizeof m->message,m->script_modified?
        "Playtest launched with private scripted-door metadata. Close it to return.":
        "Playtest launched in new window. Close it to return here.");
#else
    (void)x;(void)y;
    snprintf(m->message,sizeof m->message,"Playtest process isolation currently requires Linux.");
#endif
}
/* Clipboard stores an authored Jaguar texture ID. For cross-surface
 * painting, resolve the *same FILES artwork* through the destination's
 * wall or flat table. Never silently paint an unrelated numbered texture. */
static int map_maker_paste(AvpMapMaker *m,unsigned x,unsigned y,unsigned field)
{
    u8 value;unsigned source_file=0,target_file=0;
    if(!m||!m->has_copied_texture||field>=6u)return 0;
    value=m->copied_texture;
    if(value&&(m->copied_level!=m->level || (m->copied_field<4u)!=(field<4u))){
        int found=0;
        if(m->copied_field<4u){
            if(!avp_gpu_wall_texture_file(m->copied_level,value,&source_file))return 0;
        }else if(!avp_gpu_flat_texture_file(m->copied_level,value,&source_file))return 0;
        for(unsigned i=1u;i<=255u;i++){
            int ok=field<4u ? avp_gpu_wall_texture_file(m->level,(u8)i,&target_file):
                                  avp_gpu_flat_texture_file(m->level,(u8)i,&target_file);
            if(ok && target_file==source_file){value=(u8)i;found=1;break;}
        }
        if(!found){
            snprintf(m->message,sizeof m->message,"No equivalent Jaguar texture for destination surface.");
            return 0;
        }
    }
    unsigned char saved=m->copied_texture;
    m->copied_texture=value;
    int ok=avp_mm_paste_surface(m,x,y,field);
    m->copied_texture=saved;
    return ok;
}
static int map_maker_paste_target(AvpMapMaker *m)
{
    if(!m||(!m->target_valid&&!m->selected_count))return 0;
    if(m->selected_count){
        /* No partial writes if one destination is script-protected. */
        AvpMapMaker *copy=malloc(sizeof *copy);
        if(!copy)return 0;
        memcpy(copy,m,sizeof *copy);copy->selected_count=0;
        for(unsigned i=0;i<m->selected_count;i++){
            AvpMmSelectedFace f=m->selected[i];
            if(!map_maker_paste(copy,f.x,f.y,f.field)){
                snprintf(m->message,sizeof m->message,"Paste cancelled: %.90s",copy->message);
                free(copy);return 0;
            }
        }
        memcpy(m->cells,copy->cells,sizeof m->cells);
        m->dirty=copy->dirty;m->ghost_walls=copy->ghost_walls;
        free(copy);
        snprintf(m->message,sizeof m->message,"Pasted %u selected surfaces.",m->selected_count);
        return 1;
    }
    return map_maker_paste(m,m->target_x,m->target_y,m->target_field);
}
static void map_maker_key(AvpPcVideo *v,KeySym k)
{
    AvpMapMaker *m=&v->map_maker;
    if(m->name_edit_open){
        size_t len=strlen(m->edit_stem);
        if(k==XK_Escape){m->name_edit_open=0;m->context_open=1;return;}
        if(k==XK_BackSpace){if(len)m->edit_stem[len-1u]=0;return;}
        if(k==XK_Return){
            if(map_maker_save_named(v,m->edit_stem)){
                m->name_edit_open=0;m->context_open=0;
            }else snprintf(m->message,sizeof m->message,"Save As: invalid name or write failure.");
            return;
        }
        if(len<80u && ((k>='a'&&k<='z')||(k>='A'&&k<='Z')||
                       (k>='0'&&k<='9')||k=='_'||k=='-')){
            m->edit_stem[len]=(char)k;m->edit_stem[len+1u]=0;
        }
        return;
    }
    if(m->browser_open){
        if(k==XK_Escape||k==XK_BackSpace){m->browser_open=0;m->context_open=1;return;}
        if(k==XK_Up&&m->browser_cursor>0)m->browser_cursor--;
        else if(k==XK_Down&&m->browser_cursor+1u<m->browser_count)m->browser_cursor++;
        else if(k==XK_Page_Up){m->browser_cursor=m->browser_cursor>10u?m->browser_cursor-10u:0u;}
        else if(k==XK_Page_Down){m->browser_cursor+=10u;if(m->browser_count&&m->browser_cursor>=m->browser_count)m->browser_cursor=m->browser_count-1u;}
        else if(k==XK_Return&&m->browser_count){
            char stem[96];snprintf(stem,sizeof stem,"%s",m->browser_files[m->browser_cursor]);
            if(!map_maker_load_named(v,stem))
                snprintf(m->message,sizeof m->message,"Invalid AVPM draft; current map preserved.");
        }
        return;
    }
    if(m->context_open){
        if(k==XK_Escape||k==XK_BackSpace){m->context_open=0;return;}
        if(k=='s'||k=='S'){
            if(map_maker_save_named(v,m->map_stem))m->context_open=0;
            else snprintf(m->message,sizeof m->message,"Save failed. Check custom_maps folder.");
        }else if(k=='a'||k=='A'){
            m->name_edit_open=1;
            snprintf(m->edit_stem,sizeof m->edit_stem,"%s",m->map_stem);
        }else if(k=='l'||k=='L'){
            map_maker_browser_scan(m);m->browser_open=1;m->context_open=0;
        }else if(k=='b'||k=='B'){
            m->context_open=0;v->menu_page=MENU_MM_BINDINGS;v->menu_cursor=0;
        }else if(k=='p'||k=='P'){
            m->context_open=0;map_maker_play_test(v);
        }else if(k=='e'||k=='E'){
            m->playtest_no_enemies=!m->playtest_no_enemies;
            snprintf(m->message,sizeof m->message,"Play Test enemies: %s",
                     m->playtest_no_enemies?"OFF":"ON");
        }
        return;
    }
    if(k==XK_Escape){
        if(m->palette_open)m->palette_open=0;
        else m->context_open=1;
        return;
    }
    if(k==XK_KP_Enter || (v->cfg.mm_key[MM_MODE] && k==(KeySym)v->cfg.mm_key[MM_MODE])){
        m->palette_open=0;avp_mm_toggle_3d(m);return;
    }
    if(m->palette_open){
        int next=(int)m->palette_id;
        if(k==XK_Escape||k==XK_BackSpace||k=='t'||k=='T')m->palette_open=0;
        else if(k==XK_Left)next-=1;
        else if(k==XK_Right)next+=1;
        else if(k==XK_Up)next-=8;
        else if(k==XK_Down)next+=8;
        else if(k==XK_Page_Up)next-=64;
        else if(k==XK_Page_Down)next+=64;
        /* Fast native-handler jumps when editing a directional wall slot.
         * Exact artwork still comes from the current level's panel table. */
        else {
            unsigned id=0;
            unsigned field=m->mode_3d&&m->selected_count?
                  m->selected[m->selected_count-1u].field:
                  m->mode_3d&&m->target_valid?m->target_field:
                  m->edit_mode_2d==3u?avp_mm_texture_field(m):m->field;
            if(avp_mm_palette_category_start(field,(unsigned)k,&id))next=(int)id;
        }
        if(k==XK_Return||k==XK_space){
            unsigned field=m->mode_3d&&m->selected_count?
                  m->selected[m->selected_count-1u].field:
                  m->mode_3d&&m->target_valid?m->target_field:
                  m->edit_mode_2d==3u?avp_mm_texture_field(m):m->field;
            if(!m->mode_3d && m->edit_mode_2d==3u){
                avp_mm_texture_brush_select(m,(u8)m->palette_id);
            }else if(m->wall_tool&&!m->mode_3d&&field<4u){
                avp_mm_wall_brush_select(m,(u8)m->palette_id);
                snprintf(m->message,sizeof m->message,"Wall brush ID %u selected. Click to place.",
                         (unsigned)m->wall_brush);
            }else{
                int ok=m->mode_3d?avp_mm_assign_target(m,(u8)m->palette_id):
                                  avp_mm_assign(m,(u8)m->palette_id);
                if(ok && field<4u)avp_mm_wall_brush_select(m,(u8)m->palette_id);
            }
            m->palette_open=0;
        }
        if(next<0)next=0;
        if(next>255)next=255;
        m->palette_id=(unsigned)next;
        return;
    }
    if(k=='k'||k=='K'){(void)avp_mm_inspect_script(m);return;}
    if(k==XK_space){
        avp_mm_clear_selection(m);
        m->target_valid=0;
        m->wall_hover_valid=0;
        snprintf(m->message,sizeof m->message,"Selection cleared.");
        return;
    }
    if(!m->mode_3d && (k=='m'||k=='M')){
        v->mm_drag_paint=0;
        avp_mm_history_commit(m);
        avp_mm_cycle_2d_mode(m);return;
    }
    if(m->mode_3d&&(k=='m'||k=='M')){(void)avp_mm_mirror_target(m);return;}
    if(!m->mode_3d&&(k=='w'||k=='W')){
        avp_mm_toggle_wall_tool(m);v->mm_drag_paint=0;return;
    }
    if(k=='t'||k=='T'){
        if(m->mode_3d && m->selected_count){
            AvpMmSelectedFace f=m->selected[m->selected_count-1u];
            m->palette_id=m->cells[((size_t)f.y*m->w+f.x)*8u+f.field];
        }else if(m->mode_3d && m->target_valid)m->palette_id=m->cells[
             ((size_t)m->target_y*m->w+m->target_x)*8u+m->target_field];
        else if(!m->mode_3d&&m->edit_mode_2d==3u)
            m->palette_id=m->texture_brush[m->texture_target_2d];
        else m->palette_id=avp_mm_value(m);
        if(!m->mode_3d&&m->field<4u&&!(m->palette_id&0x7fu))
            m->palette_id=m->wall_brush;
        m->palette_open=1;return;
    }
    if(m->dirty && (k==XK_Page_Up||k==XK_Page_Down||k=='r'||k=='R'||k=='n'||k=='N')){
        snprintf(m->message,sizeof m->message,
            "Unsaved edits: S to save; close/reopen editor to discard.");
        return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_LOOK_UP]||k==XK_Home){
        avp_mm_camera_pitch(m,0.08);(void)avp_mm_pick(m,m->cursor_u,m->cursor_v);return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_LOOK_DOWN]||k==XK_End){
        avp_mm_camera_pitch(m,-0.08);(void)avp_mm_pick(m,m->cursor_u,m->cursor_v);return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_OBJECT_PLACE] || k=='o' || k=='O'){
        map_maker_object_edit(m,1);return;
    }
    if(k==XK_Delete || (v->cfg.mm_key[MM_OBJECT_REMOVE] &&
                        k==(KeySym)v->cfg.mm_key[MM_OBJECT_REMOVE])){
        map_maker_delete_selection(m);return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_OBJECT_NEXT] || k=='i' || k=='I'){
        unsigned id=avp_mm_object_catalog_id(m->object_def);
        m->object_def=avp_mm_object_catalog_def(
          (id+1u)%avp_mm_object_catalog_count());
        return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_OBJECT_GROUP] || k=='j' || k=='J'){
        m->object_group=(m->object_group+1u)%4u;return;
    }
    if(m->mode_3d && (k=='h'||k=='H'||
       (v->cfg.mm_key[MM_HIGHLIGHT] && k==(KeySym)v->cfg.mm_key[MM_HIGHLIGHT]))){
        avp_mm_toggle_highlight(m);return;
    }
    if(!m->mode_3d && (k=='h'||k=='H')){
        unsigned *field=m->mode_3d&&m->target_valid?&m->target_field:&m->field;
        *field=*field==6u?7u:6u;
        snprintf(m->message,sizeof m->message,"Door cell byte %u selected (retail access rules separate).",*field);
        return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_PLAY_TEST] || k=='p' || k=='P'){
        map_maker_play_test(v);return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_GHOST_WALLS] || k=='g' || k=='G'){
        avp_mm_toggle_ghost_walls(m);return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_OVERHEAD_SURFACE] || k=='c' || k=='C'){
        if(!m->mode_3d && m->edit_mode_2d==3u)avp_mm_cycle_texture_target(m);
        else avp_mm_toggle_overhead_surface(m);
        return;
    }
    if(k==XK_Tab||(v->cfg.mm_key[MM_COPY]&&k==(KeySym)v->cfg.mm_key[MM_COPY])){
        if(m->mode_3d&&m->selected_count){
            AvpMmSelectedFace f=m->selected[m->selected_count-1u];
            (void)avp_mm_copy_surface(m,f.x,f.y,f.field);
        }else if(m->mode_3d)(void)avp_mm_copy_target(m);
        else (void)avp_mm_copy_surface(m,m->x,m->y,m->edit_mode_2d==3u?avp_mm_texture_field(m):(m->overhead_ceiling?4u:5u));
        return;
    }
    if(k==XK_Return || (v->cfg.mm_key[MM_PAINT] && k==(KeySym)v->cfg.mm_key[MM_PAINT])){
        if(m->mode_3d){
            if(!map_maker_paste_target(m))snprintf(m->message,sizeof m->message,"Tab to copy a surface texture first.");
        }else if(m->edit_mode_2d==3u)
            (void)avp_mm_texture_paint_selection(m);
        else if(m->wall_tool && m->field<4u)
            (void)avp_mm_wall_place(m,m->x,m->y,m->field);
        else (void)avp_mm_place_floor(m);
        return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_TEX_PLUS]){
        if(m->mode_3d)(void)avp_mm_adjust_target(m,1);
        return;
    }
    if(k==(KeySym)v->cfg.mm_key[MM_TEX_MINUS]){
        if(m->mode_3d)(void)avp_mm_adjust_target(m,-1);
        return;
    }
    if(!m->mode_3d){
        /* Separate, intentionally unbound command: scripted relocation is
         * editor-only, never mapped to the ordinary rotation action. */
        if(k==XK_less){(void)avp_mm_relocate_scripted_wall(m,-1);return;}
        if(k==XK_greater){(void)avp_mm_relocate_scripted_wall(m,1);return;}
        if(k==XK_comma || (v->cfg.mm_key[MM_ROTATE_CCW] &&
           k==(KeySym)v->cfg.mm_key[MM_ROTATE_CCW])){
            (void)(m->wall_tool?avp_mm_wall_turn(m,-1):avp_mm_rotate_wall(m,-1));return;
        }
        if(k==XK_period || (v->cfg.mm_key[MM_ROTATE_CW] &&
           k==(KeySym)v->cfg.mm_key[MM_ROTATE_CW])){
            (void)(m->wall_tool?avp_mm_wall_turn(m,1):avp_mm_rotate_wall(m,1));return;
        }
    }
    switch(k){
    case XK_Up:if(!m->mode_3d)avp_mm_move(m,0,-1);break;
    case XK_Down:if(!m->mode_3d)avp_mm_move(m,0,1);break;
    case XK_Left:if(!m->mode_3d)avp_mm_move(m,-1,0);break;
    case XK_Right:if(!m->mode_3d)avp_mm_move(m,1,0);break;
    case XK_F3:avp_mm_toggle_3d(m);break;
    case 'b':case 'B':v->menu_page=MENU_MM_BINDINGS;v->menu_cursor=0;break;
    case 'f':case 'F':m->field=(m->field+1u)%8u;break;
    case 0xfe20u:m->field=(m->field+7u)%8u;break;
    case '+':case '=':case 0xffabu:if(m->mode_3d)(void)avp_mm_adjust_target(m,1);else (void)avp_mm_adjust(m,1);break;
    case '-':case '_':case 0xffadu:if(m->mode_3d)(void)avp_mm_adjust_target(m,-1);else (void)avp_mm_adjust(m,-1);break;
    case XK_0:case 0xffb0u:if(m->mode_3d)(void)avp_mm_assign_target(m,0);else (void)avp_mm_assign(m,0);break;
    case XK_1:case 0xffb1u:if(m->mode_3d)(void)avp_mm_assign_target(m,1);else (void)avp_mm_assign(m,1);break;
    case XK_Page_Up:(void)avp_mm_select(m,m->level==15u?1u:m->level+1u);(void)avp_mm_blank(m);break;
    case XK_Page_Down:(void)avp_mm_select(m,m->level==1u?15u:m->level-1u);(void)avp_mm_blank(m);break;
    /* File commands are intentionally behind Escape, like BUILD/Mapster32. */
    case 's':case 'S':case 'l':case 'L':break;
    case 'r':case 'R':(void)avp_mm_select(m,m->level);break;
    case 'n':case 'N':(void)avp_mm_blank(m);break;
    default:break;
    }
}
static int map_maker_object_sprite_lookup(void *context,unsigned def,
                                          AvpMmOverheadSprite *sprite);
static void map_maker_texture_preview_draw(AvpPcVideo *v,Drawable target,
                                            unsigned id,int mixed,unsigned field,
                                            int x,int y,unsigned size);
static void map_maker_object_preview_draw(AvpPcVideo *v,Drawable target,
                                          unsigned amp_def,int x,int y);
static void draw_map_maker(AvpPcVideo *v,Drawable target)
{
    static const char *fields[]={"Left wall/panel","Top wall/panel","Right wall/panel","Bottom wall/panel",
        "Ceiling texture","Floor texture","Left-door state","Top-door state"};
    const AvpMapMaker *m=&v->map_maker;
    unsigned ww=v->win_w?v->win_w:v->fb.width,wh=v->win_h?v->win_h:v->fb.height;
    int left=20,top=49,step=17;char line[220];
    unsigned boxh=wh>16u?wh-16u:wh;
    unsigned max_text_w=510u;
    if(m->context_open||m->browser_open||m->name_edit_open){
        XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
        XFillRectangle(v->dpy,target,v->gc,8,8,ww>500u?490u:ww-16u,
                       wh>80u?wh-16u:wh);
        XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
#define MM_MODAL(line,txt) XDrawString(v->dpy,target,v->gc,20,48+(line)*18,txt,(int)strlen(txt))
        MM_MODAL(0,"MAP MAKER - ESCAPE CONTEXT");
        if(m->name_edit_open){
            MM_MODAL(2,"SAVE AS - NAME (without .avpm)");
            MM_MODAL(4,m->edit_stem);
            MM_MODAL(6,"Type letters/numbers/_/-; Enter: save");
            MM_MODAL(7,"Backspace: erase; Esc: context menu");
        }else if(m->browser_open){
            MM_MODAL(2,"LOAD MAP - custom_maps/*.avpm");
            if(!m->browser_count)MM_MODAL(4,"No .avpm maps found. Esc to return.");
            else{
                unsigned begin=m->browser_cursor>10u?m->browser_cursor-10u:0u;
                char entry[150];
                for(unsigned i=begin;i<m->browser_count&&i<begin+14u;i++){
                    snprintf(entry,sizeof entry,"%c %s.avpm",i==m->browser_cursor?'>':' ',m->browser_files[i]);
                    MM_MODAL(4+(int)(i-begin),entry);
                }
            }
            MM_MODAL(20,"Arrows/PgUp/PgDn: browse; Enter: load; Esc: back");
        }else{
            MM_MODAL(2,"S   Save      custom_maps/<name>.avpm");
            MM_MODAL(3,"A   Save As   choose a new map filename");
            MM_MODAL(4,"L   Load      browse saved .avpm maps");
            MM_MODAL(5,"B   Bindings  keyboard / mouse / joystick");
            MM_MODAL(6,"P   Play Test From Here");
            MM_MODAL(7,m->playtest_no_enemies?
                     "E   Enemy-free Play Test: ON":"E   Enemy-free Play Test: OFF");
            MM_MODAL(8,"Esc  Close context menu");
            MM_MODAL(10,m->message);
        }
#undef MM_MODAL
        return;
    }
    if(m->palette_open){
        AvpMmBrowserLayout browser;
        if(avp_mm_browser_layout(v->present_w,v->present_h,&browser)){
            int vx=((int)ww-(int)v->present_w)/2;
            int end=browser.x0+vx-16;
            if(end>8 && (unsigned)end<max_text_w)max_text_w=(unsigned)end;
        }
    }
    XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
    if(!m->mode_3d){
        AvpMmOverheadLayout layout;
        unsigned text_width=ww;
        if(avp_mm_overhead_layout(m,v->present_w,v->present_h,&layout)){
            int vx=((int)ww-(int)v->present_w)/2;
            /* The HUD width depends on the FIXED map viewport boundary,
             * never the zoomed/panned map origin. At high zoom x0 becomes
             * negative, previously expanding this black box over the map. */
            if(layout.clip_x0+vx>20)
                text_width=(unsigned)(layout.clip_x0+vx-8);
        }
        if(m->palette_open && text_width>max_text_w)text_width=max_text_w;
        XFillRectangle(v->dpy,target,v->gc,8,8,text_width,boxh);
    }else XFillRectangle(v->dpy,target,v->gc,8,8,max_text_w,309u);
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
#define AVP_MM_EDITOR_TEXT(offset,fmt,...) do{snprintf(line,sizeof line,fmt,##__VA_ARGS__); \
    XDrawString(v->dpy,target,v->gc,left,top+(offset)*step,line,(int)strlen(line));}while(0)
    AVP_MM_EDITOR_TEXT(0,"MAP MAKER - %s (level %u of 15)",avp_mm_name(m->level),m->level);
    AVP_MM_EDITOR_TEXT(1,"Source: %s",m->custom_map?"Blank custom map":
        m->source[m->level].original?"Original ROM copy":"Blank draft");
    AVP_MM_EDITOR_TEXT(2,"Dimensions: %u x %u    %s",m->w,m->h,m->dirty?"UNSAVED CHANGES":"Saved / unchanged");
    if(m->palette_open){
        unsigned field=m->mode_3d&&m->selected_count?
                  m->selected[m->selected_count-1u].field:
                  m->mode_3d&&m->target_valid?m->target_field:
                  m->edit_mode_2d==3u?avp_mm_texture_field(m):m->field;
        AVP_MM_EDITOR_TEXT(4,"RETAIL TEXTURE BROWSER");
        AVP_MM_EDITOR_TEXT(6,"Arrows: choose   PgUp/PgDn: page");
        if(field<4u)AVP_MM_EDITOR_TEXT(7,"W: wall S: shaped D: door A: strip");
        AVP_MM_EDITOR_TEXT(8,(m->edit_mode_2d==3u&&!m->mode_3d)?"Click / Enter: choose texture brush":
                m->wall_tool&&!m->mode_3d&&field<4u?
                "Click / Enter: choose wall brush":"Click / Enter: assign");
        AVP_MM_EDITOR_TEXT(9,"Esc / T: close");
        if(field<4u){
            AVP_MM_EDITOR_TEXT(10,"Type %u, variant %u: %s",
                    avp_mm_panel_type((u8)m->palette_id),m->palette_id&7u,
                    avp_mm_panel_kind((u8)m->palette_id));
            AVP_MM_EDITOR_TEXT(11,"Mirror: %s | IDs 0 and 128 are empty",
                    (m->palette_id&0x80u)?"ON":"OFF");
        }else AVP_MM_EDITOR_TEXT(11,"Flat/property ID: no wall or door type");
        return;
    }
    if(m->mode_3d){
        unsigned tx=m->target_x,ty=m->target_y,field=m->target_field;
        int object_hover=avp_mm_3d_object_at(m,v->present_w,v->present_h,
            (int)(m->cursor_u*(double)v->present_w),
            (int)(m->cursor_v*(double)v->present_h));
        const char *face[]={"WEST","NORTH","EAST","SOUTH","CEILING","FLOOR","DOOR STATE L","DOOR STATE T"};
        if(m->selected_count){
            AvpMmSelectedFace f=m->selected[m->selected_count-1u];
            tx=f.x;ty=f.y;field=f.field;
        }
        unsigned value=(tx<m->w&&ty<m->h&&field<8u)?
             m->cells[((size_t)ty*m->w+tx)*8u+field]:0u;
        if(object_hover>=0){
            const AvpMmObjectRecord *o=&m->objects[object_hover];
            unsigned id=avp_mm_object_catalog_id(o->def);
            AVP_MM_EDITOR_TEXT(4,"AIMED OBJECT ID %u: %s",id,avp_mm_object_catalog_name(id));
            AVP_MM_EDITOR_TEXT(5,"Object at X %.2f Y %.2f (AMP %u)",
                (double)o->x8/256.0,(double)o->y8/256.0,(unsigned)o->def);
        }else{
            AVP_MM_EDITOR_TEXT(4,"%s: %s FACE (X %02u Y %02u)",m->selected_count?"SELECTED":"AIMED",
                    field<8u?face[field]:"UNKNOWN",tx,ty);
            AVP_MM_EDITOR_TEXT(5,"Texture/property ID: %u (0x%02X)",value,value);
        }
        AVP_MM_EDITOR_TEXT(6,"Camera %.2f %.2f yaw %.2f pitch %.2f",m->camera_x,m->camera_y,m->camera_heading,m->camera_pitch);
        if(field<4u)AVP_MM_EDITOR_TEXT(7,"Panel: %s (type %u, %s)",
            avp_mm_panel_kind((u8)value),avp_mm_panel_type((u8)value),
            (value&0x80u)?"mirrored":"normal");
        AVP_MM_EDITOR_TEXT(8,"Left: select | Shift+L drag: add | Right: deselect");
        AVP_MM_EDITOR_TEXT(9,"WASD/Arrows: move & turn | Q/E: turn | Home/End: look");
        AVP_MM_EDITOR_TEXT(10,"Wheel: texture  M: mirror face  T: palette");
        AVP_MM_EDITOR_TEXT(11,"Tab: copy  Hold Enter: paint  G: %s  H: outline %s",
                m->ghost_walls?"ghost ON":"ghost OFF",m->highlight_3d?"ON":"OFF");
        AVP_MM_EDITOR_TEXT(12,"Space: clear all | K: script | Ctrl+Z/Y: undo/redo");
        AVP_MM_EDITOR_TEXT(13,"P: Play Test From Here (new game window)");
        AVP_MM_EDITOR_TEXT(14,"O: place Del: remove I: Object ID %u %s (AMP %u)",
          avp_mm_object_catalog_id(m->object_def),
          avp_mm_object_catalog_name(avp_mm_object_catalog_id(m->object_def)),
          m->object_def);
        AVP_MM_EDITOR_TEXT(15,"%s",m->message);
        AVP_MM_EDITOR_TEXT(16,"Selected: %u faces, %u objects (cyan object markers)",
                m->selected_count,avp_mm_selected_object_count(m));
        XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
        {int vx=((int)ww-(int)v->present_w)/2,vy=((int)wh-(int)v->present_h)/2;
         int cx=vx+(int)(m->cursor_u*(double)v->present_w);
         int cy=vy+(int)(m->cursor_v*(double)v->present_h);
         XDrawLine(v->dpy,target,v->gc,cx-7,cy,cx+7,cy);
         XDrawLine(v->dpy,target,v->gc,cx,cy-7,cx,cy+7);}
        return;
    }
    AVP_MM_EDITOR_TEXT(4,"Cell: %02u,%02u  Edge: %s",m->x,m->y,
        m->field==0u?"LEFT":m->field==1u?"TOP":m->field==2u?"RIGHT":
        m->field==3u?"BOTTOM":fields[m->field]);
    AVP_MM_EDITOR_TEXT(5,"ID: %u %s",(unsigned)avp_mm_value(m),
        m->field<4u?avp_mm_panel_kind(avp_mm_value(m)):"(flat/property)");
    AVP_MM_EDITOR_TEXT(7,"M: %s MODE (Cells > Walls > Objects > Textures)",avp_mm_2d_mode_name(m));
    AVP_MM_EDITOR_TEXT(8,"Delete: %s only | Space: clear selection",avp_mm_2d_mode_name(m));
    AVP_MM_EDITOR_TEXT(9,m->wall_tool?"Left drag: draw | Right: deselect / erase":
        m->edit_mode_2d==2u?"Left: select object position | O: place":
        m->edit_mode_2d==3u?"Left: select | Enter: paint selection | Right: deselect":"Left: select cell | Right: paste floor");
    AVP_MM_EDITOR_TEXT(10,m->wall_tool?"T: choose wall | ,/. turn brush | Preview green/red":
        m->edit_mode_2d==2u?"I: object type | J: group | Del: remove":
        m->edit_mode_2d==3u?"C: Floor / Ceiling / Walls | T: texture browser":"Middle drag: pan | Enter: place floor");
    AVP_MM_EDITOR_TEXT(11,",/. change brush direction in Walls mode | W: walls shortcut");
    AVP_MM_EDITOR_TEXT(12,"Wheel: smooth zoom to pointer (up to 256px/tile) | KP Enter: 3D");
    AVP_MM_EDITOR_TEXT(13,"Shift+L drag (edge scroll) / Shift+Arrows: select | Enter: paint");
    AVP_MM_EDITOR_TEXT(14,"Esc: Save / Load | P: play | B: bindings");
    AVP_MM_EDITOR_TEXT(15,"O: place | I: Object ID %u %s | J: group",
      avp_mm_object_catalog_id(m->object_def),
      avp_mm_object_catalog_name(avp_mm_object_catalog_id(m->object_def)));
    if(m->edit_mode_2d==3u){
        u8 id=0;int mixed=0;
        (void)avp_mm_texture_selection(m,&id,&mixed);
        AVP_MM_EDITOR_TEXT(16,"TEXTURE: %s   Name: %s",
                avp_mm_texture_target_name(m),mixed?"Mix":"ID");
        if(!mixed)AVP_MM_EDITOR_TEXT(17,"Texture ID: %u | Brush ID: %u",(unsigned)id,
                           (unsigned)m->texture_brush[m->texture_target_2d]);
        else AVP_MM_EDITOR_TEXT(17,"Different IDs in selection | Brush: %u",
                        (unsigned)m->texture_brush[m->texture_target_2d]);
        map_maker_texture_preview_draw(v,target,mixed?0u:id,
                                       mixed?1:0,avp_mm_texture_field(m),
                                       left+12,top+18*step,80);
    }else if(m->edit_mode_2d==2u){
        AVP_MM_EDITOR_TEXT(16,"OBJECT PREVIEW: ID %u",avp_mm_object_catalog_id(m->object_def));
        map_maker_object_preview_draw(v,target,m->object_def,left+15,top+17*step);
    }else{
        AVP_MM_EDITOR_TEXT(16,"K: inspect scripted doors");
        AVP_MM_EDITOR_TEXT(17,"Shift+,/. : scripted rotation");
        AVP_MM_EDITOR_TEXT(18,"Scripts: %s",avp_mm_script_flags(m,m->x,m->y,m->field)?
            "RETAIL - PROTECTED":"none at selection");
        AVP_MM_EDITOR_TEXT(19,"Ctrl+Z: undo | Ctrl+Y: redo");
        AVP_MM_EDITOR_TEXT(20,"%s",m->message);
    }
#undef AVP_MM_EDITOR_TEXT
    /* The textured 2D map was composed into present_pixels before the
     * X11 overlay, using the retail floor/ceiling RGB panels. */
}

static void draw_map_maker_bindings(AvpPcVideo *v,Drawable target)
{
    char line[220];const char *const cat[]={"Keyboard","Mouse","Joystick / Gamepad"};
    const int parent=v->menu_page==MENU_MM_BINDINGS;
    int type=v->menu_page==MENU_MM_KEYS?0:v->menu_page==MENU_MM_MOUSE?1:2;
    int count=parent?3:AVP_MM_ACTIONS;
    /* A 640x456 window cannot show every binding on one screen. Scroll the
     * list with its current selection so even the last joystick action is
     * reachable and visible; no binding/navigation semantics are changed. */
    int rows=(int)(v->win_h>150u?(v->win_h-150u)/22u:3u);
    int begin=0;
    if(rows<3)rows=3;
    if(rows>count)rows=count;
    if(v->menu_cursor>=rows)begin=v->menu_cursor-rows+1;
    if(begin>count-rows)begin=count-rows;
    XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
    XFillRectangle(v->dpy,target,v->gc,8,8,750u,(unsigned)(rows*22+92));
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
    snprintf(line,sizeof line,"MAP MAKER BINDINGS%s",parent?"":type==0?" - KEYBOARD":type==1?" - MOUSE":" - JOYSTICK");
    XDrawString(v->dpy,target,v->gc,24,35,line,(int)strlen(line));
    for(int i=begin;i<begin+rows;i++){
        char val[70];
        if(parent)snprintf(line,sizeof line,"%c %s : PRESS ENTER",v->menu_cursor==i?'>':' ',cat[i]);
        else{
            if(type==0)snprintf(val,sizeof val,"%s",v->cfg.mm_key[i]?key_name(v->cfg.mm_key[i]):"Unbound");
            else {int num=type==1?v->cfg.mm_mouse[i]:v->cfg.mm_button[i];
                  if(type==2)button_desc(val,sizeof val,num);
                  else if(num<0)snprintf(val,sizeof val,"Unbound");
                  else snprintf(val,sizeof val,"Mouse %d",num);}
            snprintf(line,sizeof line,"%c %-24s : %s",v->menu_cursor==i?'>':' ',mm_action_names[i],val);
        }
        XDrawString(v->dpy,target,v->gc,24,62+(i-begin)*22,line,(int)strlen(line));
    }
    snprintf(line,sizeof line,"%s",v->capture_mode?"PRESS INPUT TO BIND - Escape cancels":
             "Arrows: select | Enter: capture | Delete: unbind | Esc: back");
    XDrawString(v->dpy,target,v->gc,24,74+rows*22,line,(int)strlen(line));
}
static void draw_map_maker_palette(AvpPcVideo *v,Drawable target)
{
    const AvpMapMaker *m=&v->map_maker;
    AvpMmBrowserLayout layout;
    char label[120];unsigned page=m->palette_id/64u;
    unsigned field=m->mode_3d&&m->selected_count?
                  m->selected[m->selected_count-1u].field:
                  m->mode_3d&&m->target_valid?m->target_field:
                  m->edit_mode_2d==3u?avp_mm_texture_field(m):m->field;
    int vx=((int)v->win_w-(int)v->present_w)/2;
    int vy=((int)v->win_h-(int)v->present_h)/2;
    if(!avp_mm_browser_layout(v->present_w,v->present_h,&layout))return;
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
    snprintf(label,sizeof label,"TEXTURES: %s   ID %u / 255   PAGE %u / 4",
             field<4u?"WALL":field==4u?"CEILING":field==5u?"FLOOR":"PROPERTY",
             m->palette_id,page+1u);
    XDrawString(v->dpy,target,v->gc,layout.x0+vx,layout.y0+vy-14,label,(int)strlen(label));
    for(unsigned i=0u;i<64u;i++){
        unsigned id=page*64u+i;
        int xx=layout.x0+vx+(int)(i%8u*layout.pitch)+3;
        int yy=layout.y0+vy+(int)(i/8u*layout.pitch+layout.swatch+layout.pitch/4u);
        snprintf(label,sizeof label,"%03u",id);
        XDrawString(v->dpy,target,v->gc,xx,yy,label,3);
    }
}
static void draw_menu(AvpPcVideo*v,Drawable target){
    if(v->menu_page==MENU_MM_BINDINGS||v->menu_page==MENU_MM_KEYS||
       v->menu_page==MENU_MM_MOUSE||v->menu_page==MENU_MM_PAD){draw_map_maker_bindings(v,target);return;}
    if(v->menu_page==MENU_MAP_MAKER){
        draw_map_maker(v,target);
        if(v->map_maker.palette_open)draw_map_maker_palette(v,target);
        return;
    }
    static const char*names[AVP_MENU_ITEMS]={
        "Enhancement: Smooth Presentation","Gamepad / Joystick Enabled","Gamepad Device","Gamepad Deadzone",
        "Bind Move Axis","Move Axis Invert","Bind Strafe Axis","Strafe Axis Invert","Bind Turn Axis","Turn Axis Invert",
        "Bind Fire Button","Bind Use Button","Bind Noclip Button","Key: Forward","Key: Back","Key: Turn Left","Key: Turn Right",
        "Key: Strafe Left","Key: Strafe Right","Key: Fire","Key: Use","Key: Map","Key: Noclip","Developer Species",
        "Developer: Full CPU Systems","Controller Mode",
        "Jaguar D-Pad Up","Jaguar D-Pad Down","Jaguar D-Pad Left","Jaguar D-Pad Right","Jaguar A","Jaguar B","Jaguar C",
        "Jaguar Pause","Jaguar Option","Jaguar Keypad 0","Jaguar Keypad 1","Jaguar Keypad 2","Jaguar Keypad 3","Jaguar Keypad 4",
        "Jaguar Keypad 5","Jaguar Keypad 6","Jaguar Keypad 7","Jaguar Keypad 8","Jaguar Keypad 9","Jaguar Keypad *","Jaguar Keypad #","Bind Map Button",
        "Cheat: Restore Health","Cheat: Restore Ammo","Cheat: Infinite Health","Cheat: Infinite Ammo","Cheat: Give Weapons",
        "Quicksave (also F5)","Quickload (also F9)","Close Options","Enhancement: Window-resolution 3D",
        "Cheat: Motion Tracker","Cheat: Security Increase","Cheat: Security Decrease",
        "Key: Previous Weapon","Key: Next Weapon","Key: Alien Claw","Key: Alien Bite","Key: Alien Tail",
        "Key: Terminal Next","Key: Terminal Previous","Key: Terminal Exit",
        "Button: Previous Weapon","Button: Next Weapon","Button: Alien Claw","Button: Alien Bite","Button: Alien Tail",
        "Button: Terminal Next","Button: Terminal Previous","Button: Terminal Exit",
        "Cheat: Freeze Enemies","Audio: Motion Tracker Sounds","Controls: Analog Movement","Controls: Turn Sensitivity","Controls: Turn Speed",
        "Key: Pause","Key: Option","Button: Strafe Left","Button: Strafe Right",
        "Key: Predator Medkit","Key: Predator Cloak","Key: Predator Vision",
        "Button: Predator Medkit","Button: Predator Cloak","Button: Predator Vision",
        "Button: Pause","Button: Option",
        "Button: Move Forward","Button: Move Backward",
        "Mouse Enabled","Mouse Sensitivity","Mouse Acceleration",
        "Mouse: Fire","Mouse: Use","Mouse: Map","Mouse: Previous Weapon","Mouse: Next Weapon",
        "Mouse: Terminal Next","Mouse: Terminal Previous","Mouse: Terminal Exit",
        "Mouse: Predator Medkit","Mouse: Predator Cloak","Mouse: Predator Vision",
        "Cheat: Noclip","Audio: Player Pain Sounds",
        "Frontend Key: Up","Frontend Key: Down","Frontend Key: Left",
        "Frontend Key: Right","Frontend Key: Accept","Frontend Key: Back",
        "Frontend Button: Up","Frontend Button: Down","Frontend Button: Left",
        "Frontend Button: Right","Frontend Button: Accept","Frontend Button: Back",
        "Cheat: Enemy Type", "Cheat: Kill Selected Type", "Cheat: Kill All Enemies",
        "Cheat: Facehuggers Cannot Attach", "Cheat: Enemies Cannot Attack",
        "Audio: Facehugged Pain Sounds", "Audio: Facehug Shake Pain Sounds",
        "Deadzone: Axis 0","Deadzone: Axis 1","Deadzone: Axis 2","Deadzone: Axis 3",
        "Deadzone: Axis 4","Deadzone: Axis 5","Deadzone: Axis 6","Deadzone: Axis 7",
        "Deadzone: Axis 8","Deadzone: Axis 9","Deadzone: Axis 10","Deadzone: Axis 11",
        "Deadzone: Axis 12","Deadzone: Axis 13","Deadzone: Axis 14","Deadzone: Axis 15",
        "Enhancement: Buffer Short Button Presses","Enhancement: Enhanced Analog Controls",
        "Enhancement: Buffer Short Analog Controls",
        "Quick Turn [Extra] - Key 1","Quick Turn [Extra] - Key 2",
        "Quick Turn [Extra] - Key 3","Quick Turn [Extra] - Key 4",
        "Quick Turn [Extra] - Button 1","Quick Turn [Extra] - Button 2",
        "Quick Turn [Extra] - Button 3","Quick Turn [Extra] - Button 4",
        "Enhancement: Marine Line-of-Sight Hits",
        "Enhancement: Marine Fire Interrupt",
        "Local Multiplayer (PC extension)",
        "Marine Start: Shotgun + Full Ammo",
        "Menu Key: Open F12 Options", "Menu Button: Open F12 Options",
        "Enhancement: Remember Explored Automap",
        "P1 Button: Map Zoom In","P1 Button: Map Zoom Out",
        "Restart Game (Return to Title)",
        "Enhancement: Extend Draw Distance", "Enhancement: Extend Fog Distance",
        "Enhancement: Prototype Texture Restoration",
        "Enhancement: Unlimited Shooting Range",
        "Fixed HUD Hue (Ignore Background Color)",
        "Game Micro-Optimization","Cheat: Reveal Entire Map",
        "Normalize Diagonal Contact Range",
        "Clear Enemies From Level",
        "Clear Hazards From Level",
        "Extend Enemy Activation Distance"};
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->menu_page==MENU_MP_CONTROLS){draw_mp_controls(v,target);return;}
#endif
    int x=36,i,step=17;char value[256],linebuf[420];int count=menu_count(v);
    unsigned ww=v->win_w?v->win_w:v->fb.width,wh=v->win_h?v->win_h:v->fb.height;
    unsigned boxw=ww>800?760:ww-24;int visible=(int)(((wh>72u)?(wh-72u):(unsigned)step)/(unsigned)step);
    if(visible<3)visible=3;
    if(visible>count)visible=count;
    int first=v->menu_cursor-visible/2;if(first<0)first=0;if(first>count-visible)first=count-visible;
    unsigned boxh=(unsigned)(visible*step+52);int y=42;
    XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));XFillRectangle(v->dpy,target,v->gc,12,12,boxw,boxh);XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
    {const char*t=v->capture_mode?"INPUT CAPTURE: press button/axis or key (Delete clears, Backspace cancels)":menu_caption(v->menu_page);XDrawString(v->dpy,target,v->gc,x,28,t,(int)strlen(t));}
    for(int row=first;row<first+visible;row++){const char*label;
        i=menu_id(v,row);value[0]=0;
        if(i<0){
            if(v->menu_page==MENU_CHEATS){
                if(row==0)label="Original >";
                else if(row==1)label="New >";
                else label="Activate For";
                if(row==2)
                    snprintf(linebuf,sizeof(linebuf),"%c %-29s : < Player %u >",
                        row==v->menu_cursor?'>':' ',label,v->cheat_target+1u);
                else
                    snprintf(linebuf,sizeof(linebuf),"%c %s",row==v->menu_cursor?'>':' ',label);
                XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;
            }
            else if(v->menu_page==MENU_CHEATS_ORIGINAL){
                const unsigned p=v->cheat_target;
                const char *name=row==0?"Hold That Fuji >":row==1?"Banana Stops >":"Global Positioning >";
                int enabled=row==0?v->orig_fuji[p]:row==1?v->orig_banana[p]:v->orig_position[p];
                snprintf(linebuf,sizeof(linebuf),"%c %-31s : %s (P%u)",row==v->menu_cursor?'>':' ',name,
                    enabled?"On":"Off",p+1u);
                XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;
            }
            else if(v->menu_page==MENU_CHEATS_FUJI||v->menu_page==MENU_CHEATS_BANANA||
                    v->menu_page==MENU_CHEATS_GPS||v->menu_page==MENU_CHEATS_LEVEL){
                const unsigned p=v->cheat_target;
                const int banana=v->menu_page==MENU_CHEATS_BANANA;
                const int gps=v->menu_page==MENU_CHEATS_GPS;
                const int level=v->menu_page==MENU_CHEATS_LEVEL;
                const int enabled=banana?v->orig_banana[p]:v->orig_fuji[p];
                const unsigned live_type=p==0u&& !v->orig_actor_valid[p]?(unsigned)player_type:v->orig_species[p];
                const unsigned live_weps=p==0u&& !v->orig_actor_valid[p]?(unsigned)cur_weps:v->orig_weapons[p];
                const int live_tracker=p==0u&& !v->orig_actor_valid[p]?(show_mt!=0):v->orig_tracker[p];
                const int live_valid=p==0u||v->orig_actor_valid[p];
                if(level){
                    label=row==0?"Current Level":row==1?"Next Level":"Previous Level";
                    if(row==0)snprintf(value,sizeof(value),"l(%02u)",v->orig_gps_level[p]);
                    else snprintf(value,sizeof(value),"%s",v->orig_banana[p]?"PRESS ENTER":"Enable Banana Stops first");
                }else if(gps){
                    label=row==0?"Enabled":row==1?"Course Position":row==2?"Fine Position":"Current Level";
                    if(row==0)snprintf(value,sizeof(value),"%s",v->orig_position[p]?"On":"Off");
                    else if(!live_valid)snprintf(value,sizeof(value),"Not in game");
                    else if(row==1)snprintf(value,sizeof(value),"dec(%02u,%02u)",
                        (unsigned)(v->orig_gps_x[p]>>16),(unsigned)(v->orig_gps_y[p]>>16));
                    else if(row==2)snprintf(value,sizeof(value),"hex(%04X,%04X)",
                        (unsigned)((v->orig_gps_x[p]>>8)&0xffffu),(unsigned)((v->orig_gps_y[p]>>8)&0xffffu));
                    else snprintf(value,sizeof(value),"l(%02u)",v->orig_gps_level[p]);
                }else{
                    static const char *const marine[]={"Shotgun","Pulse Rifle","Flame Thrower","Smart Gun"};
                    static const char *const predator[]={"Combi Stick","Shoulder Cannon","Smart Disc","Wrist Blade"};
                    static const char *const alien[]={"Tail","Claw","Bite","Unavailable"};
                    const char *const *weapon=live_type==PT_ALIEN?alien:
                        live_type==PT_PREDATOR?predator:marine;
                    label=row==0?"Enabled":row==1?"Raise Security Clearance":row==2?"Lower Security Clearance":
                        row==3?"Motion Tracker":row>=4&&row<=7?weapon[row-4]:
                        row==8?"Refill All Ammunition":row==9?"God Mode":"Level Select >";
                    if(row==0)snprintf(value,sizeof(value),"%s",enabled?"On":"Off");
                    else if(row==9)snprintf(value,sizeof(value),"%s",v->orig_god[p]&&v->orig_banana[p]?"On":"Off");
                    else if(row==10)snprintf(value,sizeof(value),"%s",enabled?"PRESS ENTER":"Enable Banana Stops first");
                    else if(!enabled)snprintf(value,sizeof(value),"Enable %s first",banana?"Banana Stops":"Hold That Fuji");
                    else if(!live_valid)snprintf(value,sizeof(value),"Not in game");
                    else if(row==3)snprintf(value,sizeof(value),"%s",live_type==PT_HUMAN?(live_tracker?"On":"Off"):"Marine only");
                    else if(row>=4&&row<=7)snprintf(value,sizeof(value),"%s",live_type==PT_ALIEN&&row==7?"N/A":
                        live_weps&(1u<<(row-3))?"On":"Off");
                    else if(row==1||row==2)snprintf(value,sizeof(value),"%s",live_type==PT_HUMAN?"PRESS ENTER":"Marine only");
                    else snprintf(value,sizeof(value),"PRESS ENTER");
                }
                snprintf(linebuf,sizeof(linebuf),"%c %s : %s",row==v->menu_cursor?'>':' ',label,value);
                XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;
            }
            else if(v->menu_page==MENU_DEBUG){
                snprintf(linebuf,sizeof linebuf,"%c Refresh Level : PRESS ENTER",
                    row==v->menu_cursor?'>':' ');
                XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;
            }
            else if(v->menu_page==MENU_ROOT){
                static const char *labels[]={"Controls >","Video >","Audio >","Enhancements >","Cheats >","Debug >","Miscellaneous >","Map Maker"};
                label=labels[row];
                if(row==7){
                    snprintf(linebuf,sizeof linebuf,"%c %-34s : PRESS ENTER",row==v->menu_cursor?'>':' ',label);
                    XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;
                }
            }
            else if(v->menu_page==MENU_AUDIO){
                switch(row){
                case 0:label="Speaker Mode";snprintf(value,sizeof(value),"%s",avp_audio_layout_name(v->cfg.audio_speaker_mode));break;
                case 1:label="Subwoofer Channels";snprintf(value,sizeof(value),"%u",v->cfg.audio_subwoofers);break;
                case 2:label="Active Output Channels";
                    snprintf(value,sizeof(value),"%u / requested %u",
                        avp_pc_audio_active_channels(),avp_audio_layout_channels(
                          v->cfg.audio_speaker_mode,v->cfg.audio_subwoofers));break;
                case 3:label="Music Volume";snprintf(value,sizeof(value),"%u%%",v->cfg.audio_music_volume);break;
                case 4:label="SFX Volume";snprintf(value,sizeof(value),"%u%%",v->cfg.audio_sfx_volume);break;
                case 5:label="Ambience Volume";snprintf(value,sizeof(value),"%u%%",v->cfg.audio_ambience_volume);break;
                case 6:label="Ambient Hissing Noise Volume";snprintf(value,sizeof(value),"%u%%",v->cfg.audio_hissing_volume);break;
                case 7:label="Voice Volume";snprintf(value,sizeof(value),"%u%%",v->cfg.audio_voice_volume);break;
                case 8:label="Music";snprintf(value,sizeof(value),"%s",v->cfg.audio_music_on?"On":"Off");break;
                case 9:label="Ambience";snprintf(value,sizeof(value),"%s",v->cfg.audio_ambience_on?"On":"Off");break;
                default:label="Ambient Hissing Noises";snprintf(value,sizeof(value),"%s",v->cfg.audio_hissing_on?"On":"Off");break;
                }
                snprintf(linebuf,sizeof(linebuf),"%c %s : %s",row==v->menu_cursor?'>':' ',label,value);
                XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;
            }
            else if(v->menu_page==MENU_VIDEO){
                static const char *aspect[]={"4:3","16:9","16:10","21:9","32:9"};
                static const char *modes[]={"Windowed","Borderless","Fullscreen"};
                if(row==0){label="Aspect Ratio";snprintf(value,sizeof(value),"%s",aspect[v->cfg.video_aspect<5u?v->cfg.video_aspect:0u]);}
                else if(row==1){label="Window Mode";snprintf(value,sizeof(value),"%s",modes[v->cfg.video_window_mode<3u?v->cfg.video_window_mode:0u]);}
                else if(row==2){label="Window Size";snprintf(value,sizeof(value),"%u x %u",v->cfg.video_window_width,v->cfg.video_window_height);}
                else if(row==3){
                    unsigned rates[48],n=video_refresh_options(v,rates,48);
                    label="Fullscreen Refresh";
                    if(n==1 && v->cfg.video_refresh!=0u)snprintf(value,sizeof(value),"Desktop (unsupported)");
                    else if(v->cfg.video_refresh==0u)snprintf(value,sizeof(value),"Desktop default");
                    else snprintf(value,sizeof(value),"%u Hz",v->cfg.video_refresh);
                }
                else if(row==4){label="Apply Display Settings";snprintf(value,sizeof(value),"Press Enter");}
#ifdef AVP_LOCAL_MULTIPLAYER
                else if(row==5){label="Two-Player View Layout";snprintf(value,sizeof(value),"%s",avp_local_mp_layout_name(v->local_mp.layout));}
                else if(row==6){label="Three-Player View Layout";snprintf(value,sizeof(value),"%s",avp_local_mp_three_layout_name(v->local_mp.three_layout));}
                else if(row==7){label="PC Presentation Rate";snprintf(value,sizeof(value),"%u Hz",v->cfg.presentation_hz);}
                else if(row==8){label="Show FPS";snprintf(value,sizeof(value),"%s",v->cfg.show_fps?"On":"Off");}
                else if(row==9){label="V-Sync";snprintf(value,sizeof(value),"%s",avp_present_available(v)?(v->cfg.video_vsync?"On":"Off"):"Unavailable");}
                else {label="Disable Fog";snprintf(value,sizeof(value),"%s",v->cfg.disable_fog?"On":"Off");}
#else
                else if(row==5){label="PC Presentation Rate";snprintf(value,sizeof(value),"%u Hz",v->cfg.presentation_hz);}
                else if(row==6){label="Show FPS";snprintf(value,sizeof(value),"%s",v->cfg.show_fps?"On":"Off");}
                else if(row==7){label="V-Sync";snprintf(value,sizeof(value),"%s",avp_present_available(v)?(v->cfg.video_vsync?"On":"Off"):"Unavailable");}
                else {label="Disable Fog";snprintf(value,sizeof(value),"%s",v->cfg.disable_fog?"On":"Off");}
#endif
                snprintf(linebuf,sizeof(linebuf),"%c %s : %s",row==v->menu_cursor?'>':' ',label,value);
                XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;
            }
            else if(v->menu_page==MENU_CONTROLS)label=row==1?"Gamepad / Joystick >":(row==2?"Keyboard >":(row==3?"Mouse >":"Local Players (2-4) >"));
            else if(v->menu_page==MENU_GAMEPAD)label=row==0?"Axes & Deadzones >":(row==2?"Frontend Buttons >":(v->cfg.controller_mode_jaguar?"Jaguar Virtual Pad Buttons >":"PC Action Buttons >"));
            else if(v->menu_page==MENU_KEYS)label=(v->cfg.controller_mode_jaguar||row==1)?"Frontend Keys >":"PC Action Keys >";
            else label=row==0?"Mouse Settings >":"Mouse Bindings >";
            snprintf(linebuf,sizeof(linebuf),"%c %s",row==v->menu_cursor?'>':' ',label);XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;}
        label=names[i];
#ifdef AVP_LOCAL_MULTIPLAYER
        if(v->menu_page==MENU_CHEATS_NEW && v->cheat_target>0u){
            const AvpMpInputProfile *p=&v->local_mp_controls.player[v->cheat_target];
            int player_value=-1;
            switch(i){
            case 50:player_value=p->cheat_infinite_health;break;
            case 51:player_value=p->cheat_infinite_ammo;break;
            case 57:player_value=p->cheat_tracker;break;
            case 109:player_value=p->cheat_noclip;break;
            case 126:player_value=p->cheat_no_facehug;break;
            case 127:player_value=p->cheat_no_enemy_attack;break;
            case 173:player_value=p->cheat_reveal_map;break;
            default:break;
            }
            if(player_value!=-1){
                snprintf(value,sizeof(value),"%s (Player %u)",player_value?"Enabled":"Disabled",v->cheat_target+1u);
                snprintf(linebuf,sizeof(linebuf),"%c %s : %s",row==v->menu_cursor?'>':' ',label,value);
                XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));continue;
            }
        }
#endif
        switch(i){
        case 0:snprintf(value,sizeof(value),"%s",v->cfg.smooth_presentation?"Enabled":"Disabled");break;case 1:snprintf(value,sizeof(value),"%s",v->cfg.gamepad_enabled?"Enabled":"Disabled");break;
        case 2:if(v->js_fd>=0)snprintf(value,sizeof(value),"%s | %s",v->js_path,v->js_name);else if(v->cfg.gamepad_device[0])snprintf(value,sizeof(value),"%s | unavailable",v->cfg.gamepad_device);else snprintf(value,sizeof(value),"Auto / none detected");break;
        case 3:snprintf(value,sizeof(value),"%.2f (all axes)",v->cfg.deadzone);break;case 4:snprintf(value,sizeof(value),"Axis %d",v->cfg.move_axis);break;case 5:snprintf(value,sizeof(value),"%s",v->cfg.move_invert?"Yes":"No");break;case 6:snprintf(value,sizeof(value),"Axis %d",v->cfg.strafe_axis);break;case 7:snprintf(value,sizeof(value),"%s",v->cfg.strafe_invert?"Yes":"No");break;case 8:snprintf(value,sizeof(value),"Axis %d",v->cfg.turn_axis);break;case 9:snprintf(value,sizeof(value),"%s",v->cfg.turn_invert?"Yes":"No");break;
        case 10:button_desc(value,sizeof(value),v->cfg.fire_button);break;case 11:button_desc(value,sizeof(value),v->cfg.use_button);break;case 12:button_desc(value,sizeof(value),v->cfg.noclip_button);break;
        case 13:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_forward));break;case 14:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_back));break;case 15:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_turn_left));break;case 16:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_turn_right));break;case 17:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_strafe_left));break;case 18:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_strafe_right));break;case 19:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_fire));break;case 20:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_use));break;case 21:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_map));break;case 22:snprintf(value,sizeof(value),"%s",key_name(v->cfg.key_noclip));break;
        case 23:snprintf(value,sizeof(value),"%s",v->species_request==0?"Human":(v->species_request==1?"Alien":"Predator"));break;case 24:snprintf(value,sizeof(value),"%s",v->cfg.full_cpu_systems?"Enabled":"Movement only");break;case 25:snprintf(value,sizeof(value),"%s",v->cfg.controller_mode_jaguar?"Atari Jaguar virtual pad":"PC action bindings");break;
        case 47:button_desc(value,sizeof(value),v->cfg.map_button);break;case 48:case 49:case 52:case 53:case 54:case 166:snprintf(value,sizeof(value),"Press Enter");break;case 50:snprintf(value,sizeof(value),"%s",v->infinite_health?"Enabled":"Disabled");break;case 51:snprintf(value,sizeof(value),"%s",v->infinite_ammo?"Enabled":"Disabled");break;case 55:snprintf(value,sizeof(value),"Press Enter");break;case 56:snprintf(value,sizeof(value),"%s",v->cfg.high_resolution_world?"Enabled":"Jaguar native");break;
        case 57:snprintf(value,sizeof(value),"%s",v->tracker_cheat?"Enabled":"Disabled");break;
        case 58:case 59:snprintf(value,sizeof(value),"Press Enter");break;
        case 76:snprintf(value,sizeof(value),"%s",v->freeze_enemies?"Enabled":"Disabled");break;
        case 77:snprintf(value,sizeof(value),"%s",v->cfg.tracker_sounds_enabled?"On":"Off");break;
        case 110:snprintf(value,sizeof(value),"%s",v->cfg.player_pain_sounds_enabled?"On":"Off");break;
        case 123:{static const char *kinds[]={"Alien","Crawling Alien","Marine","Predator","Facehugger","Facehugger Egg","Queen","Generator"};snprintf(value,sizeof(value),"%s",kinds[(unsigned)v->kill_type_choice%8u]);break;}
        case 124:case 125:case 175:case 176:snprintf(value,sizeof(value),"Press Enter");break;
        case 126:snprintf(value,sizeof(value),"%s",v->facehug_attach_disabled?"Enabled":"Disabled");break;
        case 127:snprintf(value,sizeof(value),"%s",v->attacks_disabled?"Enabled":"Disabled");break;
#ifdef AVP_LOCAL_MULTIPLAYER
        case 173:snprintf(value,sizeof(value),"%s",v->local_mp_controls.player[0].cheat_reveal_map?"Enabled":"Disabled");break;
#endif
        case 128:snprintf(value,sizeof(value),"%s",v->cfg.facehug_pain_sounds_enabled?"On":"Off");break;
        case 129:snprintf(value,sizeof(value),"%s",v->cfg.facehug_shake_pain_sounds_enabled?"On":"Off");break;
        case 111:case 112:case 113:case 114:case 115:case 116:{
            const unsigned long keys[6]={v->cfg.menu_key_up,v->cfg.menu_key_down,
                v->cfg.menu_key_left,v->cfg.menu_key_right,v->cfg.menu_key_accept,
                v->cfg.menu_key_back};
            snprintf(value,sizeof(value),"%s",keys[i-111]?key_name(keys[i-111]):"Unbound");break;
        }
        case 117:case 118:case 119:case 120:case 121:case 122:{
            const int buttons[6]={v->cfg.menu_button_up,v->cfg.menu_button_down,
                v->cfg.menu_button_left,v->cfg.menu_button_right,
                v->cfg.menu_button_accept,v->cfg.menu_button_back};
            button_desc(value,sizeof(value),buttons[i-117]);break;
        }
        case 78:snprintf(value,sizeof(value),"%s",v->cfg.analog_movement?"Enabled":"Disabled");break;
        case 79:snprintf(value,sizeof(value),"%.2fx",v->cfg.turn_sensitivity);break;
        case 80:snprintf(value,sizeof(value),"%.2fx",v->cfg.turn_speed);break;
        case 81:snprintf(value,sizeof(value),"%s",v->cfg.key_pause?key_name(v->cfg.key_pause):"Unbound");break;
        case 82:snprintf(value,sizeof(value),"%s",v->cfg.key_option?key_name(v->cfg.key_option):"Unbound");break;
        case 83:button_desc(value,sizeof(value),v->cfg.strafe_left_button);break;
        case 84:button_desc(value,sizeof(value),v->cfg.strafe_right_button);break;
        case 85:snprintf(value,sizeof(value),"%s",v->cfg.key_predator_medkit?key_name(v->cfg.key_predator_medkit):"Unbound");break;
        case 86:snprintf(value,sizeof(value),"%s",v->cfg.key_predator_cloak?key_name(v->cfg.key_predator_cloak):"Unbound");break;
        case 87:snprintf(value,sizeof(value),"%s",v->cfg.key_predator_vision?key_name(v->cfg.key_predator_vision):"Unbound");break;
        case 88:button_desc(value,sizeof(value),v->cfg.predator_medkit_button);break;
        case 89:button_desc(value,sizeof(value),v->cfg.predator_cloak_button);break;
        case 90:button_desc(value,sizeof(value),v->cfg.predator_vision_button);break;
        case 91:button_desc(value,sizeof(value),v->cfg.pc_pause_button);break;
        case 92:button_desc(value,sizeof(value),v->cfg.pc_option_button);break;
        case 93:button_desc(value,sizeof(value),v->cfg.forward_button);break;
        case 94:button_desc(value,sizeof(value),v->cfg.backward_button);break;
        case 95:snprintf(value,sizeof(value),"%s",v->cfg.mouse_enabled?"Enabled (capture in game)":"Disabled (free cursor)");break;
        case 96:snprintf(value,sizeof(value),"%.2fx",v->cfg.mouse_sensitivity);break;
        case 97:snprintf(value,sizeof(value),"%.2f",v->cfg.mouse_acceleration);break;
        case 109:snprintf(value,sizeof(value),"%s",v->noclip?"Enabled":"Disabled");break;
        case 163:snprintf(value,sizeof(value),"%s",v->cfg.remember_automap?"Enabled":"Disabled");break;
        case 167:snprintf(value,sizeof(value),"%s",avp_host_draw_distance_label(v->cfg.extend_draw_distance));break;
        case 168:snprintf(value,sizeof(value),"%s",avp_host_fog_distance_label(v->cfg.extend_fog_distance));break;
        case 170:snprintf(value,sizeof(value),"%s",v->cfg.unlimited_shooting_range?"Enabled (whole level)":"Disabled (original)");break;
        case 169:{
            if(v->cfg.prototype_texture_restoration)
                snprintf(value,sizeof(value),v->prototype_textures.applied?
                    "Enabled (%u panels)":"Enabled (no valid pack)",
                    v->prototype_textures.applied);
            else
                snprintf(value,sizeof(value),"Disabled (retail)");
            break;
        }
        case 146:snprintf(value,sizeof(value),"%s",v->cfg.buffer_short_presses?"Enabled":"Disabled");break;
        case 147:snprintf(value,sizeof(value),"%s",v->cfg.enhanced_analog_controls?"Enabled":"Disabled");break;
        case 148:snprintf(value,sizeof(value),"%s",v->cfg.buffer_short_analog?"Enabled":"Disabled");break;
        case 157:snprintf(value,sizeof(value),"%s",v->cfg.marine_pulse_los_hits?"Enabled":"Disabled");break;
        case 158:snprintf(value,sizeof(value),"%s",v->cfg.marine_fire_interrupt?"Enabled":"Disabled");break;
        case 174:snprintf(value,sizeof(value),"%s",v->cfg.normalize_diagonal_contact_range?"On":"Off (Jaguar)");break;
        case 177:{static const char *const modes[]={"Original (Jaguar)","2x","3x","4x","Unlimited"};
            int setting=v->cfg.enemy_activation_distance;
            snprintf(value,sizeof(value),"%s",modes[setting>=0&&setting<5?setting:0]);break;}
        case 160:snprintf(value,sizeof(value),"%s",v->cfg.marine_start_shotgun?"Enabled":"Disabled");break;
        case 171:snprintf(value,sizeof(value),"%s",v->cfg.hud_fixed_hue?"Enabled":"Disabled (Jaguar)");break;
        case 172:snprintf(value,sizeof(value),"%s",!avp_game_micro_opt_available()?"Unavailable":(v->cfg.game_micro_optimization?"Enabled":"Disabled (original)"));break;
        case 161:snprintf(value,sizeof(value),"%s",v->cfg.menu_key_open?key_name(v->cfg.menu_key_open):"Unbound (F12 always works)");break;
        case 162:button_desc(value,sizeof(value),v->cfg.menu_button_open);break;
#ifdef AVP_LOCAL_MULTIPLAYER
        case 164:case 165:button_desc(value,sizeof(value),
            v->local_mp_controls.player[0].gamepad_button[
                i==164?AVP_MP_MAP_ZOOM_IN:AVP_MP_MAP_ZOOM_OUT]);break;
#endif
#ifdef AVP_LOCAL_MULTIPLAYER
        case 159:snprintf(value,sizeof(value),"%s",v->local_mp.enabled?"Enabled":"Disabled");break;
#endif
        case 149:case 150:case 151:case 152:{unsigned long k=v->cfg.quick_turn_keys[i-149];
            snprintf(value,sizeof(value),"%s",k?key_name(k):"Unbound");break;}
        case 153:case 154:case 155:case 156:
            button_desc(value,sizeof(value),v->cfg.quick_turn_buttons[i-153]);break;
        default:if(i>=130&&i<146)snprintf(value,sizeof(value),"%.2f",v->cfg.axis_deadzone[i-130]);
          else if(i>=98&&i<98+AVP_MOUSE_ACTIONS){int b=v->cfg.mouse_button[i-98];snprintf(value,sizeof(value),b<0?"Unbound":"Mouse %d",b);}
          else if(i>=60&&i<68){snprintf(value,sizeof(value),"%s",v->cfg.action_key[i-60]?key_name(v->cfg.action_key[i-60]):"Unbound");}
          else if(i>=68&&i<76){button_desc(value,sizeof(value),v->cfg.action_button[i-68]);}
          else {button_desc(value,sizeof(value),jag_binding_value(v,i));}break;}
        snprintf(linebuf,sizeof(linebuf),"%c %-34s : %s",row==v->menu_cursor?'>':' ',label,value);XDrawString(v->dpy,target,v->gc,x,y+(row-first)*step,linebuf,(int)strlen(linebuf));}
    {const char *hint=v->capture_mode?"Backspace/Esc: Cancel | Delete: Unbind":"Backspace/Esc: Back | Delete: Clear binding";
     XDrawString(v->dpy,target,v->gc,x,y+visible*step+9,hint,(int)strlen(hint));}
}


#ifdef AVP_LOCAL_MULTIPLAYER
/* PC-only addition, painted over (never into) the Jaguar retail title pixels.
 * Start Game enters the explicit experimental local-MP native runtime, not
 * an unimplemented full multi-world/mixed-species campaign. */
static void draw_local_mp_title(AvpPcVideo *v)
{
    const unsigned ww=v->win_w?v->win_w:v->fb.width;
    const unsigned wh=v->win_h?v->win_h:v->fb.height;
    const unsigned boxw=ww>800u?760u:(ww>24u?ww-24u:0u);
    const int x=36, step=v->local_mp_page==AVP_LOCAL_MP_MENU_MODIFIERS?33:17;
    const int available=(int)(wh>76u?wh-76u:0u);
    int visible=available/step;
    int first;
    unsigned boxh;
    const char *const labels[]={"Number of Players","Campaign","Player 1 Character",
        "Player 2 Character","Player 3 Character","Player 4 Character","Split-Screen Layout",
        "Respawn at Campaign Start", "Start Game",
        "P1 AI Controlled", "P2 AI Controlled", "P3 AI Controlled", "P4 AI Controlled",
        "Game Mode", "Score Limit", "Time Limit", "Modifier: Fast Movement",
        "Deathmatch Level", "Enemies", "Modifier: Fast Equip",
        "Number of Bots",
        "Bot 1 Character", "Bot 2 Character", "Bot 3 Character", "Bot 4 Character",
        "Bot 1 Behavior", "Bot 2 Behavior", "Bot 3 Behavior", "Bot 4 Behavior",
        "Modifier: Character Change on Death For Random Character Players", "Show Whole Map"};
    char b[256], value[128];
    unsigned row;
    unsigned visible_rows[AVP_LOCAL_MP_MENU_ROWS];
    int count=0,selected_index=0;
    if(!(v->local_mp.enabled || v->local_mp_walktest) ||
       !(v->local_mp_walktest ||
         (v->presenting_frontend && v->gpu.frontend_mode==AVP_MJP_TITLE)))
        return;
    XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
    if(!v->local_mp_open){
        if(v->local_mp_walktest)return; /* Never obscure live co-op HUD. */
        XFillRectangle(v->dpy,v->backbuf,v->gc,12,(int)wh-48,boxw,35);
        XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
        {const char *label="F2: Local Multiplayer >";
         XDrawString(v->dpy,v->backbuf,v->gc,x,(int)wh-26,label,(int)strlen(label));}
        return;
    }
    count=(int)avp_local_mp_menu_rows(&v->local_mp,v->local_mp_page,visible_rows);
    for(int i=0;i<count;i++)if(visible_rows[i]==v->local_mp_cursor)selected_index=i;
    if(visible<1)visible=1;
    if(visible>count)visible=count;
    first=selected_index-visible/2;
    if(first<0)first=0;
    if(first>count-visible)first=count-visible;
    /* Match the ordinary F12 panel: fixed maximum width and content-sized
     * height, not a full-screen black rectangle for nine options. */
    boxh=(unsigned)(visible*step+52);
    XFillRectangle(v->dpy,v->backbuf,v->gc,12,12,boxw,boxh);
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
    {const char *title=v->local_mp_page==AVP_LOCAL_MP_MENU_BOTS?
            "LOCAL MULTIPLAYER - BOTS":
            v->local_mp_page==AVP_LOCAL_MP_MENU_MODIFIERS?
            "LOCAL MULTIPLAYER - MODIFIERS":
            v->local_mp_page==AVP_LOCAL_MP_MENU_OPTIONS?
            "LOCAL MULTIPLAYER - OPTIONS":v->local_mp_walktest?
            "LOCAL MULTIPLAYER - IN-GAME SETUP":"LOCAL MULTIPLAYER - SETUP";
     XDrawString(v->dpy,v->backbuf,v->gc,x,28,title,(int)strlen(title));}
    for(int visible_row=first;visible_row<first+visible;visible_row++){
        row=visible_rows[visible_row];
        if(row>=AVP_LOCAL_MP_MENU_BOTS_ROW)snprintf(value,sizeof(value),"PRESS ENTER");
        else if(row==0)snprintf(value,sizeof(value),"%u",v->local_mp.players);
        else if(row==1)snprintf(value,sizeof(value),"%s",avp_local_mp_campaign_name(v->local_mp.campaign));
        else if(row>=2&&row<=5)snprintf(value,sizeof(value),"%s",avp_local_mp_species_name(v->local_mp.species[row-2]));
        else if(row==6)snprintf(value,sizeof(value),"%s",avp_local_mp_layout_name(v->local_mp.layout));
        else if(row==7)snprintf(value,sizeof(value),"%s",v->local_mp.respawn_campaign_start?"Enabled":"Disabled");
        else if(row==AVP_LOCAL_MP_START_ROW)snprintf(value,sizeof(value),"START");
        else if(row>=9&&row<=12)snprintf(value,sizeof(value),"%s",
                v->local_mp.bot_enabled[row-9]?"Enabled":"Disabled");
        else if(row==AVP_LOCAL_MP_MODE_ROW)snprintf(value,sizeof(value),"%s",
                v->local_mp.game_mode==AVP_MP_DEATHMATCH?"Deathmatch":"Co-op");
        else if(row==AVP_LOCAL_MP_SCORE_ROW){if(v->local_mp.dm_score_limit)
                snprintf(value,sizeof(value),"%u kills",v->local_mp.dm_score_limit);
                else snprintf(value,sizeof(value),"None (Unlimited)");}
        else if(row==AVP_LOCAL_MP_TIME_ROW){if(v->local_mp.dm_time_limit)
                snprintf(value,sizeof(value),"%u minutes",v->local_mp.dm_time_limit);
                else snprintf(value,sizeof(value),"None (Unlimited)");}
        else if(row==AVP_LOCAL_MP_FAST_ROW)snprintf(value,sizeof(value),"%s",
                v->local_mp.dm_fast_movement?"Enabled (2x)":"Disabled");
        else if(row==AVP_LOCAL_MP_LEVEL_ROW)snprintf(value,sizeof(value),"Level %02u / 15",v->local_mp.dm_level);
        else if(row==AVP_LOCAL_MP_ENEMIES_ROW)snprintf(value,sizeof(value),"%s",v->local_mp.dm_enemies?"On":"Off");
        else if(row==AVP_LOCAL_MP_EQUIP_ROW)snprintf(value,sizeof(value),"%s",v->local_mp.dm_fast_equip?"Enabled":"Disabled");
        else if(row==AVP_LOCAL_MP_BOT_COUNT_ROW)snprintf(value,sizeof(value),"%u",v->local_mp.bot_count);
        else if(row>=21&&row<=24)snprintf(value,sizeof(value),"%s",
            avp_local_mp_species_name(v->local_mp.bot_species[row-21]));
        else if(row>=25&&row<=28)snprintf(value,sizeof(value),"%s",
            avp_local_mp_bot_behavior_name(v->local_mp.bot_behavior[row-25]));
        else if(row>=AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW &&
                row<AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW)snprintf(value,sizeof(value),"%s",
            avp_local_mp_species_name(v->local_mp.bot_species[row-AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW+4u]));
        else if(row>=AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW && row<AVP_LOCAL_MP_TRAVEL_ROW)
            snprintf(value,sizeof(value),"%s",
            avp_local_mp_bot_behavior_name(v->local_mp.bot_behavior[row-AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW+4u]));
        else if(row==AVP_LOCAL_MP_RANDOM_RESPAWN_ROW)snprintf(value,sizeof(value),"%s",v->local_mp.random_species_on_respawn?"Enabled":"Disabled");
        else if(row==AVP_LOCAL_MP_WHOLE_MAP_ROW)snprintf(value,sizeof(value),"%s",v->local_mp.show_whole_map?"Enabled":"Disabled");
        else if(row==AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW)snprintf(value,sizeof(value),"%s",
            v->local_mp.show_other_players_on_automap?"Enabled":"Disabled");
        else if(row==AVP_LOCAL_MP_TRAVEL_ROW)snprintf(value,sizeof(value),"%s",v->local_mp.dm_level_travel?"Enabled":"Disabled");
        else if(row==AVP_LOCAL_MP_CLOAK_DAMAGE_ROW)snprintf(value,sizeof(value),"%s",v->local_mp.dm_cloak_damage?"Enabled (4x)":"Disabled");
        else snprintf(value,sizeof(value),"---");
        {char generated_label[96];
         const char *label=row==1 && v->local_mp.game_mode==AVP_MP_DEATHMATCH?
             "Starting Campaign Area":row<(int)(sizeof(labels)/sizeof(labels[0]))?
             labels[row]:generated_label;
         if(row>=AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW &&
            row<AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW)
             snprintf(generated_label,sizeof(generated_label),"Bot %u Character",
                 (unsigned)(row-AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW+5));
         else if(row>=AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW && row<AVP_LOCAL_MP_TRAVEL_ROW)
             snprintf(generated_label,sizeof(generated_label),"Bot %u Behavior",
                 (unsigned)(row-AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW+5));
         if(row==AVP_LOCAL_MP_MENU_BOTS_ROW)
             snprintf(generated_label,sizeof(generated_label),"Bots");
         if(row==AVP_LOCAL_MP_MENU_MODIFIERS_ROW)
             snprintf(generated_label,sizeof(generated_label),"Modifiers");
         if(row==AVP_LOCAL_MP_MENU_OPTIONS_ROW)
             snprintf(generated_label,sizeof(generated_label),"Options");
         if(row==AVP_LOCAL_MP_TRAVEL_ROW)
             snprintf(generated_label,sizeof(generated_label),"Players Can Move Between Levels");
         if(row==AVP_LOCAL_MP_CLOAK_DAMAGE_ROW)
             snprintf(generated_label,sizeof(generated_label),"Cloaked Predator Takes 4x Damage");
         if(row==AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW)
             snprintf(generated_label,sizeof(generated_label),"Show Other Players On Automap");
         if(row==AVP_LOCAL_MP_RANDOM_RESPAWN_ROW){
            snprintf(b,sizeof(b),"%c Modifier: Character Change on Death",
                v->local_mp_cursor==row?'>':' ');
            XDrawString(v->dpy,v->backbuf,v->gc,x,
                42+(visible_row-first)*step,b,(int)strlen(b));
            snprintf(b,sizeof(b),"  For Random Character Players : %s",value);
            XDrawString(v->dpy,v->backbuf,v->gc,x,
                57+(visible_row-first)*step,b,(int)strlen(b));
            continue;
        }
        snprintf(b,sizeof(b),"%c %-31s : %s",v->local_mp_cursor==(unsigned)row?'>':' ',
             label,value);}
        XDrawString(v->dpy,v->backbuf,v->gc,x,42+(visible_row-first)*step,b,(int)strlen(b));
    }
    {const char *hint=v->local_mp_page!=AVP_LOCAL_MP_MENU_ROOT?
        "Arrows: Select | Left/Right: Adjust | Esc/Backspace: Main":
        "Up/Down: Select | Left/Right: Adjust | Enter: Open | Esc/Backspace: Back";
     XDrawString(v->dpy,v->backbuf,v->gc,x,42+visible*step+5,hint,(int)strlen(hint));}
}

#endif

static void draw_system_status(AvpPcVideo*v)
{
    char b[320];
    unsigned active=0,i;
    const AvpWeaponDef *wd=avp_weapon_def(player_type,cur_wepno);
    for(i=0;i<AVP_NUM_AMPS;i++)if(level1amps[i].mode||level1amps[i].host_static)++active;
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
    snprintf(b,sizeof(b),"CPU:%s  level:%d  species:%d  energy:%d/%d  AMPs:%u",v->cfg.full_cpu_systems?"FULL":"MOVE",(int)cur_level,(int)player_type,(int)player_energy,(int)max_energy,active);
    XDrawString(v->dpy,v->backbuf,v->gc,8,(int)(v->win_h?v->win_h:v->fb.height)-38,b,(int)strlen(b));
    snprintf(b,sizeof(b),"weapon:%d %s  available:%02x  fire:%u  action:%u",(int)cur_wepno,wd?wd->name:"none",(unsigned)cur_weps,(unsigned)wep_fire,(unsigned)(((joy_cur&(1u<<FIRE_B))==0u)?255u:0u));
    XDrawString(v->dpy,v->backbuf,v->gc,8,(int)(v->win_h?v->win_h:v->fb.height)-22,b,(int)strlen(b));
}


/* PC-only overlay. Render after scaling/composition so neither the Jaguar image
 * nor the shared multiplayer framebuffers contain the diagnostic text. The
 * XShm direct-to-window path keeps its fast path when Show FPS is enabled. */
static void draw_original_gps(AvpPcVideo *v,Drawable target)
{
    unsigned count=1u;
    if(!v || v->presenting_frontend ||
       (v->menu_page==MENU_MAP_MAKER && v->menu_open))return;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_walktest){
        count=v->local_mp_runtime_setup.players;
        if(count<1u || count>4u)count=1u;
    }
#endif
    for(unsigned p=0u;p<count;p++){
        char text[120],prefix[14];
        unsigned x=8u,y=40u,w=365u;
        if(!v->orig_position[p])continue;
#ifdef AVP_LOCAL_MULTIPLAYER
        if(v->local_mp_walktest && count>1u){
            AvpLocalMpRect rect[AVP_LOCAL_MP_MAX_PLAYERS];
            if(avp_local_mp_viewports(&v->local_mp_runtime_setup,v->present_w,v->present_h,rect)==count){
                unsigned vx=(v->win_w-v->present_w)/2u,vy=(v->win_h-v->present_h)/2u;
                unsigned rx=rect[p].x,ry=rect[p].y;
                x=vx+rx+4u;y=vy+ry+18u;
                w=rect[p].width>8u?rect[p].width-8u:0u;
            }
        }
#endif
        if(w<30u)continue;
        avp_pc_original_cheat_gps_format(text,sizeof(text),v->orig_gps_level[p],
            v->orig_gps_x[p],v->orig_gps_y[p]);
        snprintf(prefix,sizeof(prefix),"P%u ",p+1u);
        if(count>1u){
            char labeled[140];
            snprintf(labeled,sizeof(labeled),"%s%s",prefix,text);
            snprintf(text,sizeof(text),"%.118s",labeled);
        }
        XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
        XFillRectangle(v->dpy,target,v->gc,x-4u,y-16u,w,21u);
        XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
        XDrawString(v->dpy,target,v->gc,(int)x,(int)y,text,(int)strlen(text));
    }
}
static void draw_host_fps(AvpPcVideo *v,int direct_window,unsigned width)
{
    char label[32];
    unsigned x;
    if(!v || !v->cfg.show_fps)return;
    x=width>108u?width-108u:0u;
    if(v->fps_meter.sample_valid)
        snprintf(label,sizeof(label),"FPS: %u",v->fps_meter.display_fps);
    else
        snprintf(label,sizeof(label),"FPS: --");
    XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
    if(direct_window)XFillRectangle(v->dpy,v->win,v->gc,(int)x,0,108u,21u);
    else XFillRectangle(v->dpy,v->backbuf,v->gc,(int)x,0,108u,21u);
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
    if(direct_window)XDrawString(v->dpy,v->win,v->gc,(int)x+7,15,label,(int)strlen(label));
    else XDrawString(v->dpy,v->backbuf,v->gc,(int)x+7,15,label,(int)strlen(label));
}

#ifdef AVP_LOCAL_MULTIPLAYER
/* Render the deathmatch banner on either drawable. The previous path forced
 * the full frame through an intermediate X11 pixmap solely to show these
 * 23 text-overlay pixels, even for a one-player/zero-bot Deathmatch session.
 * No source/game framebuffer is modified: the banner is host UI only. */
static void draw_local_mp_dm_status(AvpPcVideo *v,Drawable target,unsigned width)
{
    if(!v || !v->local_mp_walktest || !v->local_mp_dm_status[0])return;
    XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
    XFillRectangle(v->dpy,target,v->gc,0,0,width,23u);
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
    XDrawString(v->dpy,target,v->gc,8,16,v->local_mp_dm_status,
                (int)strlen(v->local_mp_dm_status));
}
#endif

#ifndef _WIN32
/* RE31 R15: a multi-request overlay on the visible Window is not atomic.
 * At any presentation rate (including 50 Hz), XPutImage can become visible
 * before the menu's XFillRectangle/XDrawString requests. For F12, always
 * prepare the full frame offscreen, then expose it with one XCopyArea.
 * Preserve the direct-window fast path when the menu is closed. */
static int pc_x11_direct_window_allowed(const AvpPcVideo *v,int show_status)
{
    if(!v || show_status || (v->orig_position[0]||v->orig_position[1]||v->orig_position[2]||v->orig_position[3]) || v->menu_open ||
       (v->cfg.video_vsync && avp_present_available(v)))return 0;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_paused || v->local_mp_open ||
       (v->local_mp.enabled && v->presenting_frontend &&
        v->gpu.frontend_mode==AVP_MJP_TITLE))return 0;
#endif
    return 1;
}
#endif

/* The overhead uses the same retail flat mapping and rotation as the
 * original floor renderer. The decoded ROM image pixels are read only. */
static int map_editor_flat_lookup(void *context,unsigned level,unsigned flat,
                                  AvpMmOverheadTexture *out)
{
    const AvpPcVideo *v=(const AvpPcVideo *)context;
    unsigned file_id=0;
    const AvpGpuRgbImage *t;
    if(!out||!v||!avp_gpu_flat_texture_file(level,(u8)flat,&file_id)||file_id>=74u)
        return 0;
    t=&v->gpu.panel_tex[file_id];
    if(!t->rgb||t->width<=0||t->height<=0)return 0;
    out->rgb=t->rgb;out->width=(unsigned)t->width;out->height=(unsigned)t->height;
    out->rotation=avp_gpu_flat_texture_rotation(level,(u8)flat);
    return 1;
}
static int map_editor_browser_lookup(void *context,unsigned level,unsigned id,
                                     unsigned field,AvpMmOverheadTexture *out)
{
    const AvpPcVideo *v=(const AvpPcVideo *)context;
    unsigned file_id=0;const AvpGpuRgbImage *tex;
    if(!v||!out||!id||id>255u)return 0;
    if(field<4u){if(!avp_gpu_wall_texture_file(level,(u8)id,&file_id))return 0;}
    else if(field==4u||field==5u){
        if(!avp_gpu_flat_texture_file(level,(u8)id,&file_id))return 0;
        out->rotation=avp_gpu_flat_texture_rotation(level,(u8)id);
    }else return 0; /* door-state byte is not a texture ID */
    if(file_id>=74u)return 0;
    tex=&v->gpu.panel_tex[file_id];
    if(!tex->rgb||tex->width<=0||tex->height<=0)return 0;
    out->rgb=tex->rgb;out->width=(unsigned)tex->width;
    out->height=(unsigned)tex->height;return 1;
}
/* Original ROM artwork only: panel swatches and AAF frame-zero thumbnails.
 * These previews are presentation-only. They neither change the Jaguar map
 * nor imply that a palette ID has been semantically identified. */
static void map_maker_texture_preview_draw(AvpPcVideo *v,Drawable target,
                                            unsigned id,int mixed,unsigned field,
                                            int x,int y,unsigned size)
{
    AvpMmOverheadTexture t={0};unsigned block=4u;
    int valid=!mixed&&id&&map_editor_browser_lookup(v,v->map_maker.level,id,field,&t);
    if(size<16u)return;
    for(unsigned row=0;row<size;row+=block)for(unsigned col=0;col<size;col+=block){
        unsigned color;
        if(valid&&t.rgb&&t.width&&t.height){
            unsigned tx=(unsigned)(((uint64_t)col*t.width)/size);
            unsigned ty=(unsigned)(((uint64_t)row*t.height)/size);
            if(t.width==t.height){unsigned old;
                switch(t.rotation&3u){
                case 1u:old=tx;tx=ty;ty=t.height-1u-old;break;
                case 2u:tx=t.width-1u-tx;ty=t.height-1u-ty;break;
                case 3u:old=tx;tx=t.width-1u-ty;ty=old;break;
                default:break;
                }
            }
            const u8 *rgb=t.rgb+((size_t)ty*t.width+tx)*3u;
            color=((unsigned)rgb[0]<<16)|((unsigned)rgb[1]<<8)|rgb[2];
        }else if(mixed)color=((row/block/3u+col/block/3u)&1u)?0x576f7fu:0x263242u;
        else color=((row/block/3u+col/block/3u)&1u)?0x353b45u:0x11141cu;
        XSetForeground(v->dpy,v->gc,color);
        XFillRectangle(v->dpy,target,v->gc,x+(int)col,y+(int)row,block,block);
    }
    XSetForeground(v->dpy,v->gc,0xffd54au);
    XDrawRectangle(v->dpy,target,v->gc,x-1,y-1,size+1u,size+1u);
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
}
/* AMP definition -> an independently verified frame-zero AAF from the
 * current software renderer's sprite_choice table. Do not guess values for
 * unmapped definitions: they retain an ID-only preview. */
static int map_maker_object_aaf(unsigned def)
{
    switch(def){
    case 0:return 8;   /* Alien */
    case 1:return 29;  /* Predator */
    case 2:return 15;  /* Marine */
    case 3:return 20;  /* Egg */
    case 4:return 28;  /* Facehugger */
    case 5:return 33;
    case 6:return 43;
    case 7:return 11;
    case 8:return 39;
    case 9:return 47;
    case 11:return 2;
    case 12:return 6;
    case 13:return 7;
    case 15:return 12;
    case 16:return 13;
    case 17:return 14;
    case 18:return 16;
    case 20:return 51;
    case 21:return 22; /* Food (Map Maker Object ID 0) */
    case 25:return 46;
    case 26:return 49;
    case 27:return 50;
    case 38:return 9;
    case 40:return 10;
    case 41:return 35;
    case 42:return 19;
    case 49:return 52;
    case 50:return 58;
    default:return -1;
    }
}
static unsigned map_maker_be16(const u8 *b){return ((unsigned)b[0]<<8)|b[1];}
static unsigned map_maker_be32(const u8 *b){return ((unsigned)b[0]<<24)|((unsigned)b[1]<<16)|((unsigned)b[2]<<8)|b[3];}
/* Sprite thumbnails for the overhead map use the same verified AAF
 * frame-zero lookup as the left editor panel. This is a visual preview,
 * not an AMP definition decoder or a change to original object sprites.
 * One scratch frame is safe here: the overhead painter consumes it before
 * requesting the next object. */
static int map_maker_object_sprite_lookup(void *context,unsigned def,
                                          AvpMmOverheadSprite *sprite)
{
    static uint32_t frame[96u*96u];
    AvpPcVideo *v=(AvpPcVideo *)context;
    int ai=map_maker_object_aaf(def);
    const u8 *a;
    size_t n,fo;
    unsigned count;
    unsigned minx=96u,miny=96u,maxx=0u,maxy=0u;
    if(!v||!sprite||ai<0||ai>=63||!v->gpu.aaf[ai])return 0;
    a=v->gpu.aaf[ai];n=v->gpu.aaf_size[ai];
    if(n<8u)return 0;
    count=map_maker_be16(a);fo=(size_t)map_maker_be32(a+4u);
    if(!count||4u+(size_t)count*4u>n||fo>n||n-fo<2u)return 0;
    {
        int left=(int)(int8_t)a[fo],right=(int)(int8_t)a[fo+1u];
        if(right<left||right-left>160||n-fo-2u<(size_t)(right-left)*2u)return 0;
        memset(frame,0,sizeof frame);
        for(int col=0;col<right-left;col++){
            unsigned rel=map_maker_be16(a+fo+2u+(size_t)col*2u);
            size_t off=fo+(size_t)rel;
            if(!rel||off>n||n-off<2u)continue;
            int top=(int)(int8_t)a[off],bottom=(int)(int8_t)a[off+1u];
            if(bottom<top||bottom-top>180||n-off-2u<(size_t)(bottom-top)*2u)continue;
            for(int row=0;row<bottom-top;row++){
                u16 cry=(u16)map_maker_be16(a+off+2u+(size_t)row*2u);
                int px=48+left+col,py=48+top+row;
                if(!cry||px<0||px>=96||py<0||py>=96)continue;
                u8 r,g,b;
                avp_jaguar_cry16_to_rgb(cry,&r,&g,&b);
                frame[(size_t)py*96u+(unsigned)px]=0xff000000u|
                    ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                if((unsigned)px<minx)minx=(unsigned)px;
                if((unsigned)py<miny)miny=(unsigned)py;
                if((unsigned)px+1u>maxx)maxx=(unsigned)px+1u;
                if((unsigned)py+1u>maxy)maxy=(unsigned)py+1u;
            }
        }
    }
    if(maxx<=minx||maxy<=miny)return 0;
    sprite->rgba=frame;
    sprite->width=96u;sprite->height=96u;
    sprite->x0=minx;sprite->y0=miny;sprite->x1=maxx;sprite->y1=maxy;
    return 1;
}
static void map_maker_object_preview_draw(AvpPcVideo *v,Drawable target,
                                          unsigned amp_def,int x,int y)
{
    int ai=map_maker_object_aaf(amp_def),drawn=0;
    if(ai>=0&&ai<63 && v->gpu.aaf[ai]){
        const u8 *a=v->gpu.aaf[ai];size_t n=v->gpu.aaf_size[ai];
        if(n>=8u){
            unsigned count=map_maker_be16(a),fo=map_maker_be32(a+4u);
            if(count && 4u+(size_t)count*4u<=n && fo+2u<=n){
                int left=(int)(int8_t)a[fo],right=(int)(int8_t)a[fo+1u];
                if(right>=left && right-left<=160 && fo+2u+(size_t)(right-left)*2u<=n){
                    /* Draw at native source pixels at 2x, centred inside a
                     * fixed thumbnail box. No fake recoloring or mirroring. */
                    for(int col=0;col<right-left;col++){
                        unsigned rel=map_maker_be16(a+fo+2u+(size_t)col*2u);
                        size_t off=fo+(size_t)rel;
                        if(!rel||off+2u>n)continue;
                        int top=(int)(int8_t)a[off],bottom=(int)(int8_t)a[off+1u];
                        if(bottom<top||bottom-top>180||off+2u+(size_t)(bottom-top)*2u>n)continue;
                        for(int row=0;row<bottom-top;row++){
                            u16 cry=(u16)map_maker_be16(a+off+2u+(size_t)row*2u);
                            int px=48+left+col,py=48+top+row;
                            if(!cry||px<0||px>=96||py<0||py>=96)continue;
                            u8 r,g,b;avp_jaguar_cry16_to_rgb(cry,&r,&g,&b);
                            XSetForeground(v->dpy,v->gc,((unsigned)r<<16)|((unsigned)g<<8)|b);
                            XFillRectangle(v->dpy,target,v->gc,x+px,y+py,1u,1u);
                            drawn=1;
                        }
                    }
                }
            }
        }
    }
    XSetForeground(v->dpy,v->gc,drawn?0xffd54au:0x536778u);
    XDrawRectangle(v->dpy,target,v->gc,x,y,96u,96u);
    XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
}

/* 3D mode uses the original game's maze raster in a detached GPU. 2D mode
 * uses its same original decoded texture panels for an unlit overhead view.
 * Neither mode ever rebinds or edits the live Jaguar world. */
static void map_editor_preview(AvpPcVideo *v)
{
    const AvpMapMaker *m=&v->map_maker;
    unsigned width,height;
    uint32_t angle;
    double turns;
    if(!v->menu_open||v->menu_page!=MENU_MAP_MAKER||
       !v->present_pixels||!v->present_w||!v->present_h||!m->w||!m->h)return;
    if(!m->mode_3d){
        uint64_t now=avp_pc_now_ns();
        if(v->mm_last_zoom_ns && now>v->mm_last_zoom_ns)
            avp_mm_overhead_zoom_tick((AvpMapMaker*)m,
                (double)(now-v->mm_last_zoom_ns)*1e-9);
        v->mm_last_zoom_ns=now;
        avp_mm_render_overhead_with_sprites(m,v->present_pixels,
                    v->present_w,v->present_h,map_editor_flat_lookup,v,
                    map_maker_object_sprite_lookup);
        if(m->palette_open)avp_mm_browser_draw(m,v->present_pixels,v->present_w,
                          v->present_h,map_editor_browser_lookup,v);
        return;
    }
    v->mm_last_zoom_ns=0;
    if(!map_editor_ensure(v)){
        memset(v->present_pixels,0,(size_t)v->present_w*v->present_h*sizeof(uint32_t));return;
    }
    turns=-m->camera_heading*(4294967296.0/6.2831853071795864769);
    angle=(uint32_t)(int64_t)turns;
    v->map_editor_gpu->host_continuous_heading=1;
    v->map_editor_gpu->map_editor_pitch=m->camera_pitch;
    ((AvpMapMaker *)m)->view_ray_scale=avp_host_has_extra_columns(v->map_editor_fb.width,v->map_editor_fb.height)?
        (double)v->map_editor_fb.width/(2.0*avp_host_wide_focal(v->map_editor_fb.height)):1.0;
    avp_gpu_render_maze(v->map_editor_gpu,&v->map_editor_fb,
        m->cells,m->w,m->h,(uint32_t)(m->camera_x*65536.0),
        (uint32_t)(m->camera_y*65536.0),angle,1,m->level,0u,0,0);
    width=v->map_editor_fb.width;height=v->map_editor_fb.height;
    if(width&&height)avp_pc_scale_nearest32(v->map_editor_fb.pixels,width,height,
        v->present_pixels,v->present_w,v->present_h);
    /* The crosshair outline is drawn after scaling onto the detached editor
     * image. Original Jaguar pixels, maze state and game render stay intact. */
    if(!m->palette_open&&!m->context_open&&!m->browser_open&&!m->name_edit_open){
        (void)avp_mm_pick((AvpMapMaker *)m,m->cursor_u,m->cursor_v);
        if(m->highlight_3d){
            AvpMapMaker view=*m;
            for(unsigned i=0;i<m->selected_count;i++){
                const AvpMmSelectedFace *f=&m->selected[i];
                view.target_x=f->x;view.target_y=f->y;
                view.target_field=f->field;view.target_valid=1;
                (void)avp_mm_draw_3d_highlight(&view,v->present_pixels,v->present_w,v->present_h);
            }
        }
        /* 2D selected cells remain highlighted in 3D as an editor-only
         * cyan footprint. Preserve precise face selection separately. */
        (void)avp_mm_draw_3d_cell_selection(m,v->present_pixels,v->present_w,v->present_h);
        (void)avp_mm_draw_3d_highlight(m,v->present_pixels,v->present_w,v->present_h);
        (void)avp_mm_draw_3d_objects(m,v->present_pixels,v->present_w,v->present_h);
    }
    if(m->palette_open)avp_mm_browser_draw(m,v->present_pixels,v->present_w,
                          v->present_h,map_editor_browser_lookup,v);
}
static void present_prepared_pixels(AvpPcVideo *v,int show_status,int show_menu)
{
    unsigned ww,wh;
    int vx,vy;
    if(!v)return;
    map_editor_preview(v);
    ww=v->win_w?v->win_w:v->fb.width;
    wh=v->win_h?v->win_h:v->fb.height;
    vx=(int)(ww-v->present_w)/2;
    vy=(int)(wh-v->present_h)/2;
    XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
#ifndef _WIN32
    /* Linux/X11-only direct presentation. Keep the preexisting Windows GDI
     * compatibility route unchanged until verified separately on Windows. */
    /* XPutImage, like XShmPutImage, can target the window directly when
     * no backbuffer-only overlay is active. This matters on PCs built without
     * optional XShm headers: the R11 deathmatch banner otherwise forces an
     * extra full-window server-side copy even with just one human player. */
    if(pc_x11_direct_window_allowed(v,show_status)){
        /* No host overlay: put the frame straight onto the window (one
         * server-side copy) and clear only the letterbox margins.  XSync
         * keeps the next frame from overwriting the segment mid-read. */
        if(vx>0){XFillRectangle(v->dpy,v->win,v->gc,0,0,(unsigned)vx,wh);
                 XFillRectangle(v->dpy,v->win,v->gc,vx+(int)v->present_w,0,ww-(unsigned)vx-v->present_w,wh);}
        if(vy>0){XFillRectangle(v->dpy,v->win,v->gc,0,0,ww,(unsigned)vy);
                 XFillRectangle(v->dpy,v->win,v->gc,0,vy+(int)v->present_h,ww,wh-(unsigned)vy-v->present_h);}
#ifdef AVP_HAVE_XSHM
        if(v->shm_active)
            XShmPutImage(v->dpy,v->win,v->gc,v->present_img,0,0,vx,vy,
                         v->present_w,v->present_h,False);
        else
#endif
            XPutImage(v->dpy,v->win,v->gc,v->present_img,0,0,vx,vy,
                      v->present_w,v->present_h);
        /* Direct presentation is allowed only while F12 is closed. Drawing
         * F12 into the visible window after XPutImage exposes a transient
         * menu-free frame between requests (seen as flicker with V-Sync Off,
         * including 50 Hz). While F12 is open, draw into the back pixmap and
         * expose the completed image/menu in ONE XCopyArea instead.
         * The final XSync still protects the shared pixel buffer. */
#ifdef AVP_LOCAL_MULTIPLAYER
        draw_local_mp_dm_status(v,v->win,ww);
#endif
        /* F12 is necessarily closed on this direct-window path. */
        draw_host_fps(v,1,ww);
#ifdef AVP_HAVE_XSHM
        /* The X server still reads the shared segment after the request. */
        if(v->shm_active)XSync(v->dpy,False);
        else XFlush(v->dpy);
#else
        /* Normal XPutImage copies the client-side buffer into the request. */
        XFlush(v->dpy);
#endif
        avp_fps_count_host_frame(v);
        return;
    }
#endif
    /* Menu composition must remain offscreen even when V-Sync is disabled.
     * No additional copy is made during normal gameplay when F12 is closed. */
    XFillRectangle(v->dpy,v->backbuf,v->gc,0,0,ww,wh);
#ifdef AVP_HAVE_XSHM
    if(v->shm_active)XShmPutImage(v->dpy,v->backbuf,v->gc,v->present_img,0,0,vx,vy,v->present_w,v->present_h,False);
    else
#endif
    XPutImage(v->dpy,v->backbuf,v->gc,v->present_img,0,0,vx,vy,v->present_w,v->present_h);
#ifdef AVP_LOCAL_MULTIPLAYER
    draw_local_mp_dm_status(v,v->backbuf,ww);
    if(!v->menu_open)draw_local_mp_title(v);
#endif
    if(show_status)draw_system_status(v);
    draw_original_gps(v,v->backbuf);
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_paused){
        const char *msg="PAUSED - Press Pause to resume";
        XSetForeground(v->dpy,v->gc,BlackPixel(v->dpy,v->screen));
        XFillRectangle(v->dpy,v->backbuf,v->gc,ww/2u>120u?ww/2u-120u:0u,wh>28u?wh-28u:0u,240u,22u);
        XSetForeground(v->dpy,v->gc,WhitePixel(v->dpy,v->screen));
        XDrawString(v->dpy,v->backbuf,v->gc,ww/2u>112u?(int)ww/2-112:8,(int)wh-13,msg,(int)strlen(msg));
    }
#endif
    if(show_menu&&v->menu_open)draw_menu(v,v->backbuf);
    draw_host_fps(v,0,ww);
    if(v->cfg.video_vsync && avp_present_available(v)){
        /* Complete any XShm transfer into the back pixmap before scheduling
         * it for next VBlank.  On Windows the existing GDI blit must happen
         * before the DWM presentation wait. */
#ifdef _WIN32
        XCopyArea(v->dpy,v->backbuf,v->win,v->gc,0,0,ww,wh,0,0);
#else
        XSync(v->dpy,False);
#endif
        avp_present_wait_vblank(v);
    }else{
        XCopyArea(v->dpy,v->backbuf,v->win,v->gc,0,0,ww,wh,0,0);
#ifdef AVP_HAVE_XSHM
        if(v->shm_active)XSync(v->dpy,False);
#endif
    }
    avp_fps_count_host_frame(v);
}


/* The original single-player path still scales its Jaguar framebuffer exactly
 * as before.  The optional multiplayer path already contains FOUR complete
 * independent frames in present_pixels and must never rescale that surface
 * from the single-player framebuffer. Both routes use the SAME host window,
 * XImage/Win32 DIB and letterbox/overlay/presentation path. */
static void present_framebuffer(AvpPcVideo *v,int show_status,int show_menu)
{
    if(!v)return;
    scale_present(v);
    present_prepared_pixels(v,show_status,show_menu);
}

#ifdef AVP_LOCAL_MULTIPLAYER
/* Single-window live-frame *output bridge*. The caller must provide a session
 * with an actual independently implemented simulation/render backend. A
 * failed/incomplete session renders nothing; no duplicated P1 or placeholder
 * picture is ever substituted for missing players.  This API is deliberately
 * never called by single-player or the F2 setup-only screen. */
void avp_pc_video_local_mp_deathmatch_status(AvpPcVideo *v,const char *status)
{
    if(!v)return;
    snprintf(v->local_mp_dm_status,sizeof(v->local_mp_dm_status),"%s",
             status?status:"");
}
void avp_pc_video_local_mp_set_paused(AvpPcVideo *v,int paused)
{
    if(v)v->local_mp_paused=!!paused;
}
unsigned avp_pc_video_present_local_mp(AvpPcVideo *v,AvpMpSession *session)
{
    AvpLocalMpOutputFrame out;
    unsigned count;
    if(!v||!session||!(v->local_mp.enabled||v->local_mp_walktest)||!session->running||session->faulted||
       !v->present_pixels||!v->present_img||!v->dpy||!v->win||
       v->presenting_frontend)return 0;
    out.pixels=v->present_pixels;
    out.width=v->present_w;
    out.height=v->present_h;
    out.pitch=v->present_w;
    /* F12 Video options are presentation-only: do not reinitialize a world,
     * respawn an actor or overwrite the campaign/species setup. */
    session->setup.layout=v->local_mp.layout;
    session->setup.three_layout=v->local_mp.three_layout;
    count=avp_mp_session_compose(session,&out,0);
    if(!count)return 0; /* Preserve the previous window on failure. */
    present_prepared_pixels(v,0,1);
    XFlush(v->dpy);
    return count;
}
#endif

/* The boot hold must show an actual black framebuffer, not the default
 * TITLE menu text (which is normally drawn before the TITL resource event).
 * Show no debug HUD or menu; respond to window resize through normal polling. */
void avp_pc_video_present_boot_black(AvpPcVideo *v)
{
    if(!v||!v->fb.pixels)return;
    memset(v->fb.pixels,0,(size_t)v->fb.width*v->fb.height*sizeof(*v->fb.pixels));
    v->presenting_world_hires=0;
    present_framebuffer(v,0,0);
    XFlush(v->dpy);
}

/* HUD_MSG.S owns per-actor message *timing*. These callbacks own the
 * corresponding per-viewport mutable text brushes. Do not update the
 * default v->gpu when a scoped multiplayer actor is being advanced. */
#ifdef AVP_LOCAL_MULTIPLAYER
int avp_pc_video_local_mp_hud_message_route(AvpPcVideo *v,int player)
{
    if(!v || player< -1 || player>=(int)AVP_LOCAL_MP_MAX_PLAYERS)return 0;
    if(player>=0 && !local_mp_view_ensure(v,(unsigned)player))return 0;
    v->local_mp_hud_owner=player;
    return 1;
}
int avp_pc_video_local_mp_hud_message_copy(const AvpPcVideo *v,unsigned player,
    unsigned line,char *out,size_t capacity)
{
    const AvpAvpGpuState *s;
    if(!v || !out || !capacity || player>=AVP_LOCAL_MP_MAX_PLAYERS ||
       line>=3u || !(s=v->local_mp_views[player].state))return 0;
    if(!(s->hud_msg_valid&(1u<<line))) {out[0]='\0';return 1;}
    snprintf(out,capacity,"%s",s->hud_msg[line]);
    return 1;
}
#endif
#ifdef AVP_LOCAL_MULTIPLAYER
int avp_pc_video_local_mp_draw_kill_notice(AvpPcVideo *v,unsigned player,
    unsigned kind,unsigned other_player,int viewing_type)
{
    if(!v||player>=AVP_LOCAL_MP_MAX_PLAYERS||
       !v->local_mp_views[player].state||
       !v->local_mp_views[player].frame.pixels)return 0;
    avp_gpu_draw_dm_kill_notice(v->local_mp_views[player].state,
        &v->local_mp_views[player].frame,kind,other_player,viewing_type);
    return 1;
}
#endif
void avp_pc_video_hud_message_clear(AvpPcVideo *v)
{
    if(!v)return;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_hud_owner>=0){
        avp_gpu_hud_message_clear(v->local_mp_views[v->local_mp_hud_owner].state);
        return;
    }
#endif
    avp_gpu_hud_message_clear(&v->gpu);
}
void avp_pc_video_hud_message_line(AvpPcVideo *v,unsigned line,const char *text,unsigned x_pixels)
{
    if(!v)return;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_hud_owner>=0){
        avp_gpu_hud_message_line(v->local_mp_views[v->local_mp_hud_owner].state,
                                 line,text,x_pixels);
        return;
    }
#endif
    avp_gpu_hud_message_line(&v->gpu,line,text,x_pixels);
}

void avp_pc_video_object_event(AvpPcVideo *v,unsigned event,u32 a,u32 b,u32 c)
{
    if(!v)return;
    if(event==0x200u){
#ifdef AVP_LOCAL_MULTIPLAYER
        if(v->local_mp_hud_owner>=0 && v->local_mp_hud_owner<4 &&
           v->local_mp_views[v->local_mp_hud_owner].state)
            avp_hud_set_tracker_distance(avp_gpu_tracker_nearest_contact_for_view(
                v->local_mp_views[v->local_mp_hud_owner].state,
                x_pos,y_pos,centre_angle));
        else
#endif
            avp_hud_set_tracker_distance(
                avp_gpu_tracker_nearest_contact(x_pos,y_pos,centre_angle));
    }
    /* HUD.S::HP2 uses event 0x201 with its 0x100 marker; advance the
     * Predator audio raster precisely once per native simulation update. */
    if(event==0x201u && (c&0x100u)){
        s16 previous=0,unused=0;
        avp_hud_pred_meter_span(0u,&previous,&unused);
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v && v->local_mp_hud_owner>=0 && v->local_mp_hud_owner<4 &&
           local_mp_view_ensure(v,(unsigned)v->local_mp_hud_owner)){
            AvpAvpGpuState *scope=v->local_mp_views[v->local_mp_hud_owner].state;
            local_mp_waveform_select_species(scope,(unsigned)player_type>>2);
            if(scope->mp_waveform_species==2u)
                avp_gpu_predator_scope_step(scope,previous,(s16)a,(s16)b);
        }else
#endif
            avp_gpu_predator_scope_step(&v->gpu,previous,(s16)a,(s16)b);
    }
    /* MAZESCRN.S::SaveOvers snapshots the *visible* gameplay picture.
     * In high-resolution mode, the last world picture lives in world_fb;
     * fb may still contain the previous MJP character-selection screen.
     * Transfer the displayed world to the native pause framebuffer before
     * saving the original overlays. Only the pause snapshot is affected. */
    if(event==0x5053564fu && v->presenting_world_hires &&
       v->world_fb.pixels && v->fb.pixels)
        avp_pc_scale_nearest32(v->world_fb.pixels,
            v->world_fb.width,v->world_fb.height,
            v->fb.pixels,v->fb.width,v->fb.height);
    avp_jaguar_pause_op_event(&v->gpu.pause_op,event,a,b,c,v->fb.pixels,v->fb.width,v->fb.height);
}
int avp_pc_video_pause_active(const AvpPcVideo *v)
{
    return v?avp_jaguar_pause_op_active(&v->gpu.pause_op):0;
}
void avp_pc_video_present_pause(AvpPcVideo *v)
{
    if(!v||!avp_jaguar_pause_op_active(&v->gpu.pause_op))return;
    v->presenting_world_hires=0;
    avp_jaguar_pause_op_prepare_frame(&v->gpu.pause_op,v->fb.pixels,v->fb.width,v->fb.height);
    /* The Jaguar pause picture is still composited below the PC-only F12
     * interface.  Rendering the pause surface must not suppress the host
     * menu (normal gameplay and terminal presentation already permit it). */
    present_framebuffer(v,0,1);
    XFlush(v->dpy);
}
/* On the original Jaguar, MazeList continues darkening the outgoing world
 * while entering a duct or using a terminal. The hosted PC transition is
 * synchronous, so preserve the last completed gameplay frame for the
 * CPU-owned VBlank fade. Reapply the original Jaguar CRY RMW modifier to
 * that unchanged frame every step, never accumulating the darkening or
 * re-rendering high-resolution geometry during the interaction handoff. */
int avp_pc_video_begin_gameplay_fade(AvpPcVideo *v)
{
    const AvpJaguarFramebuffer *src;
    u32 *snapshot;
    size_t n;
    if(!v || v->gameplay_fade_snapshot)return 0;
    src=v->presenting_world_hires&&v->world_fb.pixels?&v->world_fb:&v->fb;
    if(!src->pixels||!src->width||!src->height||
       src->height>SIZE_MAX/sizeof(u32)/src->width)return 0;
    n=(size_t)src->width*src->height;
    snapshot=(u32*)malloc(n*sizeof(u32));
    if(!snapshot)return 0;
    memcpy(snapshot,src->pixels,n*sizeof(u32));
    v->gameplay_fade_snapshot=snapshot;
    v->gameplay_fade_width=src->width;v->gameplay_fade_height=src->height;
    v->gameplay_fade_hires=(src==&v->world_fb);
    return 1;
}
void avp_pc_video_end_gameplay_fade(AvpPcVideo *v)
{
    if(!v)return;
    free(v->gameplay_fade_snapshot);v->gameplay_fade_snapshot=NULL;
    v->gameplay_fade_width=v->gameplay_fade_height=0u;
    v->gameplay_fade_hires=0;
}
void avp_pc_video_present_gameplay(AvpPcVideo *v)
{
    if(!v)return;
    if(v->gameplay_fade_snapshot){
        AvpJaguarFramebuffer *dst=v->gameplay_fade_hires?&v->world_fb:&v->fb;
        const size_t n=(size_t)v->gameplay_fade_width*v->gameplay_fade_height;
        if(dst->pixels && dst->width==v->gameplay_fade_width &&
           dst->height==v->gameplay_fade_height){
            const u16 mod=avp_jaguar_game_clut(fade_c,xtra_c,fade_level);
            memcpy(dst->pixels,v->gameplay_fade_snapshot,n*sizeof(u32));
            if(fade_level>=0x80){
                memset(dst->pixels,0,n*sizeof(u32));
            }else if(mod && v->gpu.qconv && v->gpu.qconv_size>=131072u){
                for(size_t i=0;i<n;i++)
                    dst->pixels[i]=avp_jaguar_rmw_xrgb8888(
                        v->gameplay_fade_snapshot[i],mod,v->gpu.qconv,v->gpu.qconv_size);
            }
            v->presenting_world_hires=v->gameplay_fade_hires;
            present_framebuffer(v,0,1);
            XFlush(v->dpy);
            memcpy(dst->pixels,v->gameplay_fade_snapshot,n*sizeof(u32));
            return;
        }
        /* A window resize can invalidate the captured framebuffer. */
        avp_pc_video_end_gameplay_fade(v);
    }
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_terminal_visible){
        AvpLocalMpSourceFrame src;
        AvpLocalMpOutputFrame out;
        AvpLocalMpSetup setup=v->local_mp_runtime_setup;
        if(!v->local_mp_terminal_background||!v->present_pixels||
           !v->present_w||!v->present_h)return;
        /* Reconstruct the frozen composite before each modal page/cursor
         * redraw; otherwise stale terminal pixels can survive source changes.
         * A resize scales the saved composite to the new split layout. */
        avp_pc_scale_nearest32(v->local_mp_terminal_background,
            v->local_mp_terminal_width,v->local_mp_terminal_height,
            v->present_pixels,v->present_w,v->present_h);
        if(v->gpu.computer_active){
            avp_gpu_computer_present(&v->gpu,&v->fb);
            src=(AvpLocalMpSourceFrame){v->fb.pixels,v->fb.width,v->fb.height,v->fb.width};
            out=(AvpLocalMpOutputFrame){v->present_pixels,v->present_w,v->present_h,v->present_w};
            /* The F2 layout may change between sessions; actual displayed
             * dimensions and current layout must drive this viewport. */
            setup.layout=v->local_mp.layout;
            setup.three_layout=v->local_mp.three_layout;
            if(!avp_local_mp_overlay_player(&setup,
                    v->local_mp_terminal_owner,&src,&out))return;
        }
        present_prepared_pixels(v,0,1);
        XFlush(v->dpy);
        return;
    }
#endif
    if(v->gpu.computer_active){v->presenting_world_hires=0;avp_gpu_computer_present(&v->gpu,&v->fb);}
    present_framebuffer(v,v->cfg.debug_immediate_in_game,1);
    XFlush(v->dpy);
}
void avp_pc_video_automap_save(const AvpPcVideo *v,AvpGpuAutomapSnapshot *dst)
{
    if(v&&dst)avp_gpu_automap_save(&v->gpu,dst);
}
void avp_pc_video_automap_load(AvpPcVideo *v,const AvpGpuAutomapSnapshot *src)
{
    if(v&&src)avp_gpu_automap_load(&v->gpu,src);
}
void avp_pc_video_automap_observe(AvpPcVideo *v,const u8 *maze,u16 mw,u16 mh,
                                   u32 xpos,u32 ypos,u32 angle,unsigned logical_level)
{
    if(v){
        avp_gpu_automap_history_enter(&v->automap_history,&v->gpu,
                                      logical_level,v->cfg.remember_automap);
        avp_gpu_automap_observe(&v->gpu,maze,mw,mh,xpos,ypos,angle,logical_level);
#ifdef AVP_LOCAL_MULTIPLAYER
        /* The same Cheats > New > Reveal Entire Map setting applies to P1
         * in both single-player and local multiplayer. The selected player
         * determines whose private automap gets revealed. */
        if(v->local_mp_controls.player[0].cheat_reveal_map)
            avp_gpu_automap_reveal_all(&v->gpu,maze,mw,mh,logical_level);
#endif
    }
}
void avp_pc_video_reset_automap(AvpPcVideo *v)
{
    if(v){
        if(v->cfg.remember_automap)
            avp_gpu_automap_history_capture(&v->automap_history,&v->gpu);
        avp_gpu_automap_reset(&v->gpu);
    }
}

void avp_pc_video_reset_presentation(AvpPcVideo *v)
{
    if(!v)return;
    v->gpu.have_prev_sim=0;
    v->gpu.render_alpha=1.0;
}

void avp_pc_video_render_maze(AvpPcVideo*v,const u8*maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,int rom_ok,unsigned logical_level,u64 ticks,unsigned simulation_hz)
{
    char title[280];
    if(!v)return;
    {u64 now=avp_pc_now_ns();u64 vbl=(now>=v->vblank_epoch_ns)?((now-v->vblank_epoch_ns)*60ull/1000000000ull):0ull;
    AvpJaguarFramebuffer *target=&v->fb;
    if(v->present_w&&v->present_h &&
       (v->cfg.high_resolution_world || v->cfg.video_aspect!=0u)){
        unsigned rw=v->present_w,rh=v->present_h;
        if(!v->cfg.high_resolution_world){
            /* A 228-line wide scene keeps Jaguar vertical geometry while
             * adding genuine world columns on the left and right. */
            rh=228u;
            rw=avp_host_wide_native_width(v->present_w,v->present_h);
        }
        if(v->world_fb.width!=rw||v->world_fb.height!=rh){
            avp_jaguar_framebuffer_destroy(&v->world_fb);
            (void)avp_jaguar_framebuffer_init(&v->world_fb,rw,rh);
        }
        if(v->world_fb.pixels)target=&v->world_fb;
    }
    v->presenting_world_hires=(target==&v->world_fb);
    v->gpu.host_widescreen_fov=v->cfg.video_aspect!=0u &&
        avp_host_has_extra_columns(target->width,target->height);
    v->gpu.host_extend_draw_distance=v->cfg.extend_draw_distance;
    v->gpu.host_extend_fog_distance=v->cfg.extend_fog_distance;
    v->gpu.host_disable_fog=v->cfg.disable_fog;
    v->gpu.hud_fixed_hue=(unsigned char)!!v->cfg.hud_fixed_hue;
    /* Full 32-bit fractional heading belongs to Smooth Presentation only. */
    v->gpu.host_continuous_heading=v->cfg.smooth_presentation&&avp_pc_video_enhanced_analog_enabled(v);
    avp_gpu_render_maze(&v->gpu,target,maze,mw,mh,xpos,ypos,angle,rom_ok,logical_level,vbl,v->cfg.smooth_presentation,v->cfg.debug_immediate_in_game);}
    present_framebuffer(v,v->cfg.debug_immediate_in_game,1);
    snprintf(title,sizeof(title),"AVP Jaguar PC port M4.4 | L%u | %u Hz | tick %llu | ROM %s | textures %d | NOCLIP %s | Smooth %s | CPU %s | [/] Hz | PgUp/PgDn stages | F12",logical_level,simulation_hz,(unsigned long long)ticks,rom_ok?"verified":"diagnostic",v->gpu.textures_loaded,v->noclip?"ON":"OFF",v->cfg.smooth_presentation?"ON":"OFF",v->cfg.full_cpu_systems?"FULL":"MOVE");
    XStoreName(v->dpy,v->win,title);
    XFlush(v->dpy);
}

/* -------------------------------------------------------------------------
 * Retail MJP front-end bridge.  Game-owned pixels are decoded only from the
 * user's ROM.  The CPU state machine in mjp.c remains authoritative for
 * timing, menu selection, transitions and species choice. */
int avp_pc_video_frontend_load(AvpPcVideo *v,const AvpRomImage *rom,unsigned mode,unsigned file_index,unsigned role)
{
    return v?avp_gpu_assets_frontend_load(&v->gpu,rom,mode,file_index,role):0;
}
void avp_pc_video_frontend_event(AvpPcVideo *v,unsigned event,s32 arg0,s32 arg1)
{
    if(!v)return;
    avp_gpu_frontend_event(&v->gpu,event,arg0,arg1);
    avp_gpu_computer_event(&v->gpu,event,arg0,arg1);
}
int avp_pc_video_computer_active(const AvpPcVideo *v){return v&&v->gpu.computer_active;}
void avp_pc_video_computer_draw_glyph(AvpPcVideo *v,unsigned brush,unsigned source_x,
                                      unsigned width,unsigned x,unsigned y,int clear)
{
    if(v)avp_gpu_computer_draw_glyph(&v->gpu,brush,source_x,width,x,y,clear);
}
void avp_pc_video_computer_draw_retail_menu(AvpPcVideo *v,const char *record,u32 source_address)
{
    if(v)avp_gpu_computer_draw_retail_menu(&v->gpu,record,source_address);
}
void avp_pc_video_computer_draw_retail_overlay(AvpPcVideo *v,const char *record,u32 source_address)
{
    if(v)avp_gpu_computer_draw_retail_overlay(&v->gpu,record,source_address);
}
int avp_pc_video_frontend_transition_done(AvpPcVideo *v,unsigned phase){return v?avp_gpu_frontend_transition_done(&v->gpu,phase):1;}
void avp_pc_video_frontend_draw_text(AvpPcVideo *v,unsigned line,const char *text,unsigned x){if(v)avp_gpu_frontend_draw_text(&v->gpu,line,text,x);}
void avp_pc_video_frontend_select_instructions(AvpPcVideo *v,const char *rotate,const char *fire)
{
    if(v)avp_gpu_frontend_select_instructions(&v->gpu,rotate,fire);
}
void avp_pc_video_frontend_select_briefing(AvpPcVideo *v,const char *lines,const char *status)
{
    if(v)avp_gpu_frontend_select_briefing(&v->gpu,lines,status);
}
int avp_pc_video_frontend_select_loading_ready(const AvpPcVideo *v)
{
    return v && avp_gpu_frontend_select_loading_ready(&v->gpu);
}
void avp_pc_video_frontend_select_prompt(AvpPcVideo *v,const char *prompt)
{
    if(v)avp_gpu_frontend_select_prompt(&v->gpu,prompt);
}
void avp_pc_video_frontend_tick(AvpPcVideo *v,unsigned mode,s32 choice)
{
    if(!v)return;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(mode!=AVP_MJP_TITLE)v->local_mp_open=0;
#endif
    v->gpu.frontend_mode=mode;
    /* The retail MJP frontend draws into the native Jaguar framebuffer.
     * High-resolution world rendering may have left world_fb selected after
     * gameplay; do not present that stale image over Game Over/end screens.
     * Resizing the window already cleared this flag, which is why minimizing
     * and restoring temporarily made SIMULATION TERMINATED visible. */
    v->presenting_world_hires=0;
    if(!v->frontend_fb.pixels && !avp_jaguar_framebuffer_init(&v->frontend_fb,320u,254u))return;
    avp_gpu_frontend_present(&v->gpu,&v->frontend_fb,mode,choice);
    v->presenting_frontend=1;
    present_framebuffer(v,0,1);
    v->presenting_frontend=0;
    XStoreName(v->dpy,v->win,"Alien vs Predator - Atari Jaguar retail frontend");
    XFlush(v->dpy);
}
