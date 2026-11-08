/* R264: real production X11 press-event routing, no debug instrumentation. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../src/platform/pc/pc_video_x11.c"
s16 player_type=PT_HUMAN;
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));assert(v);
    settings_defaults(&v->cfg);
    v->mouse_focused=1; /* Real PC window starts focused. */
    player_type=PT_HUMAN;
    v->cfg.buffer_short_presses=1;
    v->previous_poll_pad=0xffffffffu;
    short_key_press(v,(KeySym)v->cfg.key_fire);
    /* A whole press/release occurred before end-of-poll: pad is released. */
    avp_pc_buffer_poll(&v->previous_poll_pad,&v->pad_buffered,0xffffffffu,1);
    assert((avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1)&(1u<<FIRE_A))==0);
    assert(avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1)==0xffffffffu);
    short_key_press(v,(KeySym)v->cfg.key_pause);
    short_key_press(v,(KeySym)v->cfg.key_option);
    {u32 out=avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1);
     assert(!(out&(1u<<PAUSE)) && !(out&(1u<<OPTION)));}
    /* Separate Pause and Option both survived the same event batch. */
    short_key_press(v,(KeySym)v->cfg.key_pause);
    short_key_press(v,(KeySym)v->cfg.key_option);
    {u32 out=avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1);
     assert(!(out&(1u<<PAUSE))&&!(out&(1u<<OPTION)));}
    v->menu_open=1;
    short_key_press(v,(KeySym)v->cfg.key_fire);
    assert(!v->pad_buffered);v->menu_open=0;
    v->cfg.buffer_short_presses=0;
    short_key_press(v,(KeySym)v->cfg.key_fire);
    assert(!v->pad_buffered);
    v->cfg.buffer_short_presses=1;
    v->cfg.controller_mode_jaguar=1;
    short_key_press(v,XK_x);
    assert(!(avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1)&(1u<<FIRE_B)));
    v->cfg.controller_mode_jaguar=0;player_type=PT_ALIEN;
    short_key_press(v,(KeySym)v->cfg.key_use);
    assert(v->use_button_buffered);
    assert(!(v->pad_buffered&(1u<<FIRE_B)));
    assert(avp_pc_video_take_alien_use(v));
    assert(!avp_pc_video_take_alien_use(v));
    short_key_press(v,(KeySym)v->cfg.action_key[3]);
    assert(v->pad_buffered&(1u<<FIRE_B));
    assert(!v->use_button_buffered);
    v->pad_buffered=0;
    v->cfg.mouse_enabled=1;v->mouse_grabbed=1;
    short_mouse_press(v,3);
    assert(v->use_button_buffered && !v->pad_buffered);
    v->use_button_buffered=0;
    short_mouse_press(v,1);
    assert(v->pad_buffered&(1u<<FIRE_A));
    v->mouse_grabbed=0;v->pad_buffered=0;
    short_mouse_press(v,1);
    assert(!v->pad_buffered);
    /* Real joystick press/release delivered in one read() loop. */
    v->cfg.gamepad_enabled=1;
    player_type=PT_HUMAN;
    short_joystick_press(v,v->cfg.fire_button);
    assert(v->pad_buffered&(1u<<FIRE_A));
    {u32 out=avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1);
     assert(!(out&(1u<<FIRE_A)));
     assert(avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1)==0xffffffffu);}
    player_type=PT_ALIEN;
    short_joystick_press(v,v->cfg.use_button);
    assert(v->use_button_buffered && !v->pad_buffered);
    assert(avp_pc_video_take_alien_use(v));
    short_joystick_press(v,v->cfg.action_button[4]);
    assert(v->pad_buffered&(1u<<FIRE_C));
    v->pad_buffered=0;
    v->cfg.controller_mode_jaguar=1;
    short_joystick_press(v,v->cfg.jag_pause_button);
    assert(v->pad_buffered&(1u<<PAUSE));
    v->menu_open=1;v->pad_buffered=0;
    short_joystick_press(v,v->cfg.jag_a_button);
    assert(!v->pad_buffered);
    free(v);
    puts("R264 event-level short keyboard/mouse presses PASS");
    return 0;
}
