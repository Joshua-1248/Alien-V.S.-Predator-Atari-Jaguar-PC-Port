/* R271: focus loss must discard *new* host buffered events, not just old ones. */
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
    AvpPcVideo *v=calloc(1,sizeof(*v));assert(v);
    settings_defaults(&v->cfg);v->js_fd=-1;v->mouse_focused=1;
    v->cfg.buffer_short_presses=1;
    v->cfg.buffer_short_analog=1;
    /* Prove the source's normal focused short input still works. */
    short_joystick_press(v,v->cfg.fire_button);
    assert(v->pad_buffered&(1u<<FIRE_A));
    clear_focus_input(v);
    assert(!v->pad_buffered);
    /* A new joystick press arriving after FocusOut must be ignored. */
    short_joystick_press(v,v->cfg.fire_button);
    assert(v->pad_buffered==0);
    short_key_press(v,(KeySym)v->cfg.key_fire);
    assert(v->pad_buffered==0);
    /* No previously queued digital edge may resurrect after focus loss. */
    v->pad_buffered=1u<<FIRE_A;
    assert(avp_pc_video_simulation_pad(v,0xffffffffu)==0xffffffffu);
    assert(v->pad_buffered==0);
    /* No short analog pulse may bleed into a background gameplay tick. */
    v->cfg.analog_movement=1;v->cfg.gamepad_enabled=1;v->js_fd=1;
    v->analog_buffer.forward=0.625f;
    v->modern_forward=0.0f;
    (void)avp_pc_video_simulation_pad(v,0xffffffffu);
    assert(v->sim_forward==0.0f);
    assert(v->analog_buffer.forward==0.0f);
    /* Once refocused, new presses work as before. */
    v->mouse_focused=1;v->js_fd=-1;
    short_joystick_press(v,v->cfg.fire_button);
    assert(v->pad_buffered&(1u<<FIRE_A));
    assert(!(avp_pc_video_simulation_pad(v,0xffffffffu)&(1u<<FIRE_A)));
    free(v);
    puts("R271 FocusOut new short-input buffering PASS");
    return 0;
}
