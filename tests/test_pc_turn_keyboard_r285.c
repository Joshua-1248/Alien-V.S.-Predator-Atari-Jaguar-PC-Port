/* Production keyboard binding/focus/control-mode gating for predictive yaw. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_turn_presentation.h"
#include "../src/platform/pc/pc_video_x11.c"
s16 player_type=PT_HUMAN;
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));
    uint32_t a;
    assert(v);
    settings_defaults(&v->cfg);
    v->mouse_focused=1;
    assert(avp_pc_video_keyboard_turn_direction(v)==0);
    setkey(v,12,(KeySym)v->cfg.key_turn_left,1);
    assert(avp_pc_video_keyboard_turn_direction(v)==-1);
    assert(avp_pc_digital_turn_display(0x20000000u,0,0x300,0x600,-1,0.25,&a));
    assert(a==0x20c00000u);
    setkey(v,13,(KeySym)v->cfg.key_turn_right,1);
    assert(avp_pc_video_keyboard_turn_direction(v)==0); /* opposing keys */
    setkey(v,12,(KeySym)v->cfg.key_turn_left,0);
    assert(avp_pc_video_keyboard_turn_direction(v)==1);
    v->menu_open=1;assert(!avp_pc_video_keyboard_turn_direction(v));v->menu_open=0;
    v->frontend_input=1;assert(!avp_pc_video_keyboard_turn_direction(v));v->frontend_input=0;
    v->capture_mode=1;assert(!avp_pc_video_keyboard_turn_direction(v));v->capture_mode=0;
    v->mouse_focused=0;assert(!avp_pc_video_keyboard_turn_direction(v));v->mouse_focused=1;
    v->cfg.controller_mode_jaguar=1;
    assert(!avp_pc_video_keyboard_turn_direction(v));
    v->cfg.controller_mode_jaguar=0;
    assert(avp_pc_video_keyboard_turn_direction(v)==1);
    setkey(v,13,(KeySym)v->cfg.key_turn_right,0);
    assert(!avp_pc_video_keyboard_turn_direction(v));
    free(v);
    puts("R285 production keyboard left/right binding and focus/menu gates PASS");
    return 0;
}
