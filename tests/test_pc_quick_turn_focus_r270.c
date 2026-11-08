/* R270: real PC input gate must reject new joystick Quick Turn edges after
 * FocusOut, not merely clear the events that were pending at the loss. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
    assert(v);
    settings_defaults(&v->cfg);
    v->js_fd=-1;
    v->mouse_focused=1;
    v->cfg.quick_turn_buttons[0]=avp_pc_axis_button_token(1,0);
    /* Confirm a real focused joystick axis crossing queues one turn. */
    quick_turn_axis_event(v,1,0,-19660);
    assert(avp_pc_video_take_quick_turn(v)==1);
    /* A turn already queued before losing focus must be canceled. */
    queue_quick_turn(v);
    assert(v->quick_turn_pending==1);
    clear_focus_input(v);
    assert(v->quick_turn_pending==0);
    assert(!avp_pc_video_quick_turn_allowed(v));
    /* A NEW joystick axis event after FocusOut must not rotate offscreen. */
    quick_turn_axis_event(v,1,0,-19660);
    assert(v->quick_turn_pending==0);
    queue_quick_turn(v);
    assert(v->quick_turn_pending==0);
    assert(avp_pc_video_take_quick_turn(v)==0);
    /* Normal focused behavior resumes without any stale queued rotation. */
    v->mouse_focused=1;
    assert(avp_pc_video_quick_turn_allowed(v));
    assert(avp_pc_video_take_quick_turn(v)==0);
    quick_turn_axis_event(v,1,0,-19660);
    assert(avp_pc_video_take_quick_turn(v)==1);
    /* Menu, capture and frontend gates still take precedence. */
    v->menu_open=1;
    queue_quick_turn(v);
    assert(avp_pc_video_take_quick_turn(v)==0);
    v->menu_open=0;
    v->capture_mode=1;
    queue_quick_turn(v);
    assert(avp_pc_video_take_quick_turn(v)==0);
    v->capture_mode=0;
    v->frontend_input=1;
    queue_quick_turn(v);
    assert(avp_pc_video_take_quick_turn(v)==0);
    free(v);
    puts("R270 PC Quick Turn focus-event gate PASS");
    return 0;
}
