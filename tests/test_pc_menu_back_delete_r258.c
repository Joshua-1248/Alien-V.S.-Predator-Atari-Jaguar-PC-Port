/* R258: exercise the production F12 back-navigation and Delete-unbind paths.
 * This is a host-only test; no X server or Jaguar ROM is needed. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/platform/pc/pc_video_x11.c"
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}

static void test_back_path(AvpPcVideo *v)
{
    /* No interference with Backspace as a gameplay binding when F12 is shut. */
    v->menu_open=0;
    v->menu_page=MENU_KEYS_PC;
    assert(!f12_backspace(v,XK_BackSpace));
    assert(v->menu_page==MENU_KEYS_PC);
    assert(!f12_delete_binding(v));
    assert(!f12_backspace(v,XK_Delete));

    v->menu_open=1;
    v->menu_page=MENU_ROOT;
    assert(!f12_backspace(v,XK_space));
    v->menu_cursor=0;menu_choose(v);assert(v->menu_page==MENU_CONTROLS);
    v->menu_cursor=2;menu_choose(v);assert(v->menu_page==MENU_KEYS);
    v->menu_cursor=0;menu_choose(v);assert(v->menu_page==MENU_KEYS_PC);
    assert(f12_backspace(v,XK_BackSpace) && v->menu_page==MENU_KEYS);
    assert(f12_backspace(v,XK_BackSpace) && v->menu_page==MENU_CONTROLS);
    assert(f12_backspace(v,XK_BackSpace) && v->menu_page==MENU_ROOT);
    assert(f12_backspace(v,XK_BackSpace) && !v->menu_open);

    /* Capture cancellation must be different from unbinding. */
    v->menu_open=1;v->menu_page=MENU_KEYS_PC;
    v->cfg.action_key[3]=XK_v;
    v->capture_mode=3;v->capture_target=63;
    assert(f12_backspace(v,XK_BackSpace));
    assert(!v->capture_mode && v->menu_page==MENU_KEYS_PC);
    assert(v->cfg.action_key[3]==XK_v);
    assert(f12_backspace(v,XK_BackSpace));
    assert(v->menu_page==MENU_KEYS);

    /* Every nesting category has the expected immediate parent. */
    v->menu_page=MENU_GAMEPAD_AXES;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_GAMEPAD);
    v->menu_page=MENU_GAMEPAD_PC;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_GAMEPAD);
    v->menu_page=MENU_GAMEPAD_JAGUAR;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_GAMEPAD);
    v->menu_page=MENU_GAMEPAD_FRONTEND;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_GAMEPAD);
    v->menu_page=MENU_KEYS_FRONTEND;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_KEYS);
    v->menu_page=MENU_MOUSE_SETTINGS;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_MOUSE);
    v->menu_page=MENU_MOUSE_BINDINGS;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_MOUSE);
    v->menu_page=MENU_CHEATS;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_ROOT);
    v->menu_page=MENU_MISC;assert(f12_backspace(v,XK_BackSpace));assert(v->menu_page==MENU_ROOT);
}
static void test_delete_path(AvpPcVideo *v)
{
    v->menu_open=1;
    /* Highlighted PC keyboard action, without capture. */
    v->menu_page=MENU_KEYS_PC;
    v->cfg.action_key[3]=XK_v;
    int row=-1;
    for(int i=0;i<menu_count(v);i++)if(menu_id(v,i)==63)row=i;
    assert(row>=0);v->menu_cursor=row;
    assert(f12_delete_binding(v));
    assert(!v->cfg.action_key[3] && v->menu_page==MENU_KEYS_PC && !v->capture_mode);

    /* Capture mode clears the capture target, not whatever row is selected. */
    v->cfg.key_use=XK_Shift_L;
    v->cfg.key_forward=XK_w;
    v->capture_mode=3;v->capture_target=20;
    assert(f12_delete_binding(v));
    assert(!v->cfg.key_use && v->cfg.key_forward==XK_w && !v->capture_mode);
    /* Mouse binding, both selected row and active capture. */
    v->menu_page=MENU_MOUSE_BINDINGS;v->menu_cursor=0;
    v->cfg.mouse_button[0]=1;
    assert(f12_delete_binding(v)&&v->cfg.mouse_button[0]==-1);
    v->cfg.mouse_button[1]=3;
    v->capture_mode=4;v->capture_target=99;v->menu_cursor=0;
    assert(f12_delete_binding(v));
    assert(v->cfg.mouse_button[1]==-1&&!v->capture_mode);
    /* Joystick axis and gamepad button. */
    v->menu_page=MENU_GAMEPAD_AXES;v->capture_mode=2;v->capture_target=4;
    v->cfg.move_axis=1;
    assert(f12_delete_binding(v));
    assert(v->cfg.move_axis==-1&&!v->capture_mode);
    v->menu_page=MENU_GAMEPAD_PC;v->capture_mode=1;v->capture_target=71;
    v->cfg.action_button[3]=5;
    assert(f12_delete_binding(v));
    assert(v->cfg.action_button[3]==-1&&!v->capture_mode);

    /* Delete on an F12 category does not modify the bindings or exit. */
    v->menu_page=MENU_ROOT;v->menu_cursor=0;
    v->cfg.key_forward=XK_w;
    assert(f12_delete_binding(v));
    assert(v->cfg.key_forward==XK_w&&v->menu_open&&v->menu_page==MENU_ROOT);
}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));assert(v);
    settings_defaults(&v->cfg);
    snprintf(v->settings_path,sizeof(v->settings_path),"/tmp/avp-r258-menu-%ld.cfg",(long)getpid());
    test_back_path(v);
    test_delete_path(v);
    unlink(v->settings_path);
    free(v);
    puts("R258 F12 Backspace navigation and Delete unbinding PASS");
    return 0;
}
