/* R265: production FocusOut cancellation of already received short inputs. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../src/platform/pc/pc_video_x11.c"
s16 player_type=PT_ALIEN;
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));assert(v);
    settings_defaults(&v->cfg);
    v->js_fd=-1;
    v->cfg.buffer_short_presses=1;
    v->cfg.buffer_short_analog=1;
    v->mouse_focused=1;
    v->previous_poll_pad=0xffffffffu;
    short_key_press(v,(KeySym)v->cfg.key_fire);
    short_key_press(v,(KeySym)v->cfg.key_use);
    avp_pc_analog_buffer_poll(&v->analog_buffer,0.5f,-0.75f,0.375f,1);
    v->key[0]=1;
    v->pressed_sym[0]=42;
    v->use_button_held=1;
    assert((v->pad_buffered&(1u<<FIRE_A))!=0);
    assert(v->use_button_buffered);
    assert(v->analog_buffer.forward==0.5f);
    clear_focus_input(v);
    assert(!v->mouse_focused);
    assert(!v->key[0]&&!v->pressed_sym[0]);
    assert(!v->pad_buffered&&!v->use_button_buffered&&!v->use_button_held);
    assert(v->previous_poll_pad==0xffffffffu);
    assert(v->analog_buffer.forward==0.0f);
    assert(v->analog_buffer.strafe==0.0f);
    assert(v->analog_buffer.turn==0.0f);
    assert(avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1)==0xffffffffu);
    assert(!avp_pc_video_take_alien_use(v));
    /* A genuinely new press after focus returns is still buffered normally. */
    v->mouse_focused=1;
    short_key_press(v,(KeySym)v->cfg.key_fire);
    assert((avp_pc_buffer_take(&v->pad_buffered,0xffffffffu,1)&(1u<<FIRE_A))==0);
    /* Default Jaguar route: focus loss is equally safe with the buffer on. */
    v->cfg.controller_mode_jaguar=1;
    short_key_press(v,XK_x);
    assert(v->pad_buffered&(1u<<FIRE_B));
    clear_focus_input(v);
    assert(!v->pad_buffered);
    free(v);
    puts("R265 FocusOut event buffer reset PASS");
    return 0;
}
