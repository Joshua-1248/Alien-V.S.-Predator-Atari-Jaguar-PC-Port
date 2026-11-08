/* R266 production F12, persistence, gating and joystick-edge Quick Turn tests. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "pc/pc_quick_turn.h"
#include "pc/pc_live_heading.h"
static char executable[512];
static ssize_t fake_readlink(const char *p,char *dst,size_t size)
{
    size_t n=strlen(executable);assert(!strcmp(p,"/proc/self/exe"));
    if(n>size)n=size;
    memcpy(dst,executable,n);return (ssize_t)n;
}
#define readlink fake_readlink
#include "../src/platform/pc/pc_video_x11.c"
#undef readlink
s16 player_type=PT_HUMAN;
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
static void test_menu(AvpPcVideo *v)
{
    v->menu_page=MENU_KEYS_PC;
    assert(menu_id(v,menu_count(v)-4)==149);
    assert(menu_id(v,menu_count(v)-1)==152);
    v->menu_page=MENU_GAMEPAD_PC;
    assert(menu_id(v,menu_count(v)-4)==153);
    assert(menu_id(v,menu_count(v)-1)==156);
    v->menu_open=1;
    v->menu_cursor=menu_count(v)-3;
    menu_choose(v);
    assert(v->capture_mode==1&&v->capture_target==154);
    bind_button_token(v,3);
    assert(v->cfg.quick_turn_buttons[1]==3);
    v->menu_page=MENU_KEYS_PC;
    v->menu_cursor=menu_count(v)-2;
    menu_choose(v);
    assert(v->capture_mode==3&&v->capture_target==151);
    apply_capture_key(v,XK_F6);
    assert(v->cfg.quick_turn_keys[2]==XK_F6);
    v->menu_cursor=menu_count(v)-2;
    assert(f12_delete_binding(v));
    assert(!v->cfg.quick_turn_keys[2]);
    v->menu_page=MENU_GAMEPAD_PC;
    v->menu_cursor=menu_count(v)-3;
    assert(f12_delete_binding(v));
    assert(v->cfg.quick_turn_buttons[1]==-1);
    v->menu_open=0;
}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));
    AvpPcLiveHeading h={0};
    char dir[]="/tmp/avp-quick-turn-r266-XXXXXX";
    char settings[512];FILE *f;
    assert(v&&mkdtemp(dir));
    snprintf(executable,sizeof(executable),"%s/avp-test",dir);
    snprintf(settings,sizeof(settings),"%s/settings.cfg",dir);
    /* Real avp_pc_video_create starts focused; use the same state for
     * input acceptance now that the R270 FocusOut gate is enforced. */
    settings_defaults(&v->cfg);v->mouse_focused=1;make_settings_path(v);
    assert(v->cfg.quick_turn_keys[0]==XK_r);
    assert(v->cfg.quick_turn_buttons[0]==-1);
    /* Arbitrary fractional 32-bit angles, wrap, and signed right direction. */
    assert(avp_pc_quick_turn_right(0,1)==0x80000000u);
    assert(avp_pc_quick_turn_right(0x03001234u,1)==0x83001234u);
    assert(avp_pc_quick_turn_right(0x83001234u,3)==0x03001234u);
    assert(avp_pc_quick_turn_right(0xffffffffu,4)==0xffffffffu);
    assert(avp_pc_quick_turn_right(0xaabbccddu,0)==0xaabbccddu);
    /* The action must visibly interpolate, never instantly apply 180 degrees.
     * Samples cover wrap, fractional headings, queued presses and exact end. */
    {
        AvpPcQuickTurn t={0};
        const uint64_t begin=UINT64_C(1000000000);
        const uint32_t source=UINT32_C(0x03001234);
        avp_pc_quick_turn_enqueue(&t,source,begin,1);
        assert(t.active);
        assert(avp_pc_quick_turn_angle(&t,begin)==source);
        assert(avp_pc_quick_turn_angle(&t,begin+50000000u)==source-UINT32_C(0x20000000));
        assert(avp_pc_quick_turn_angle(&t,begin+100000000u)==source-UINT32_C(0x40000000));
        assert(avp_pc_quick_turn_angle(&t,begin+199999999u)!=avp_pc_quick_turn_right(source,1));
        assert(avp_pc_quick_turn_angle(&t,begin+200000000u)==avp_pc_quick_turn_right(source,1));
        assert(avp_pc_quick_turn_complete(&t,begin+200000000u));
        avp_pc_quick_turn_cancel(&t);
        assert(!t.active);
        avp_pc_quick_turn_enqueue(&t,source,begin,1);
        {uint32_t midpoint=avp_pc_quick_turn_angle(&t,begin+100000000u);
         avp_pc_quick_turn_enqueue(&t,source,begin+100000000u,1);
         assert(avp_pc_quick_turn_angle(&t,begin+100000000u)==midpoint);
         assert(!avp_pc_quick_turn_complete(&t,begin+200000000u));
         assert(avp_pc_quick_turn_angle(&t,begin+400000000u)==avp_pc_quick_turn_right(source,2));
        }
        avp_pc_quick_turn_cancel(&t);
        avp_pc_quick_turn_enqueue(&t,source,begin,4);
        assert(avp_pc_quick_turn_angle(&t,begin+800000000u)==source);
        avp_pc_quick_turn_cancel(&t);
        assert(!t.active);
    }
    assert(quick_turn_key_matches(v,XK_r));
    assert(!quick_turn_key_matches(v,XK_F6));
    queue_quick_turn(v);assert(avp_pc_video_take_quick_turn(v)==1);
    assert(avp_pc_video_take_quick_turn(v)==0);
    queue_quick_turn(v);queue_quick_turn(v);
    assert(avp_pc_video_take_quick_turn(v)==2);
    v->menu_open=1;queue_quick_turn(v);assert(!avp_pc_video_take_quick_turn(v));v->menu_open=0;
    v->frontend_input=1;queue_quick_turn(v);assert(!avp_pc_video_take_quick_turn(v));v->frontend_input=0;
    v->capture_mode=1;queue_quick_turn(v);assert(!avp_pc_video_take_quick_turn(v));v->capture_mode=0;
    v->cfg.quick_turn_buttons[0]=2;
    assert(quick_turn_button_matches(v,2));
    assert(!quick_turn_button_matches(v,4));
    v->cfg.quick_turn_buttons[1]=avp_pc_axis_button_token(1,1);
    quick_turn_axis_event(v,1,0,19000);
    assert(!avp_pc_video_take_quick_turn(v));
    quick_turn_axis_event(v,1,19000,19660);
    assert(avp_pc_video_take_quick_turn(v)==1);
    quick_turn_axis_event(v,1,19660,30000);
    assert(!avp_pc_video_take_quick_turn(v));
    quick_turn_axis_event(v,1,1000,29000);
    assert(avp_pc_video_take_quick_turn(v)==1);
    v->cfg.quick_turn_buttons[2]=avp_pc_axis_button_token(1,0);
    quick_turn_axis_event(v,1,0,-19660);
    assert(avp_pc_video_take_quick_turn(v)==1);
    queue_quick_turn(v);assert(v->quick_turn_pending);
    clear_focus_input(v);assert(!avp_pc_video_take_quick_turn(v));
    assert(!v->quick_turn_pending);
    v->cfg.quick_turn_keys[1]=XK_F7;
    v->cfg.quick_turn_keys[3]=XK_F8;
    v->cfg.quick_turn_buttons[3]=18;
    save_settings(v);
    f=fopen(settings,"r");assert(f);fclose(f);
    settings_defaults(&v->cfg);load_settings(v);
    assert(v->cfg.quick_turn_keys[0]==XK_r);
    assert(v->cfg.quick_turn_keys[1]==XK_F7);
    assert(v->cfg.quick_turn_keys[3]==XK_F8);
    assert(v->cfg.quick_turn_buttons[0]==2);
    assert(v->cfg.quick_turn_buttons[1]==avp_pc_axis_button_token(1,1));
    assert(v->cfg.quick_turn_buttons[3]==18);
    test_menu(v);
    /* Host live heading reanchors from the new angle, no old-axis delta. */
    avp_pc_live_heading_poll(&h,0,1000000000ull,1,1,1,100,10);
    assert(h.active);
    avp_pc_live_heading_reset(&h);
    assert(avp_pc_live_heading_poll(&h,0xc3001234u,1100000000ull,0,1,1,100,10)==0xc3001234u);
    unlink(settings);rmdir(dir);free(v);
    puts("R267 interpolated 180-degree Quick Turn 200ms, exact endpoint, queued events, 4 slots, settings and gating PASS");
    return 0;
}
