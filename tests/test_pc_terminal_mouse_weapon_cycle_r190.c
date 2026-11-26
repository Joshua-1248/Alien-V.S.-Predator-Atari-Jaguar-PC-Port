/* R190: real X11 mouse weapon-cycle routing, tested without an X server.
 * Keyboard and gamepad already blocked weapon cycling in a computer screen.
 * This test calls the same helper used for ButtonPress dispatch and verifies
 * the original in-game terminal input ownership for mouse bindings too. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../new/pc_host/src/pc_video_x11.c"
static int computer_open;
int avp_computer_active(void){return computer_open;}
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));assert(v);
    settings_defaults(&v->cfg);
    v->cfg.mouse_button[3]=4; /* wheel up */
    v->cfg.mouse_button[4]=5; /* wheel down */
    v->mouse_grabbed=1;
    /* Normal gameplay remains enabled with and without press buffering. */
    v->cfg.buffer_short_presses=0;
    mouse_weapon_cycle_press(v,4);assert(v->weapon_delta==-1);
    mouse_weapon_cycle_press(v,5);assert(v->weapon_delta==1);
    v->cfg.buffer_short_presses=1;
    mouse_weapon_cycle_press(v,4);assert(v->weapon_delta==-1);
    v->weapon_delta=0;
    /* Real terminal state must prevent BOTH directions with mouse grab on. */
    computer_open=1;
    mouse_weapon_cycle_press(v,4);assert(v->weapon_delta==0);
    mouse_weapon_cycle_press(v,5);assert(v->weapon_delta==0);
    computer_open=0;
    /* Other input-owning UI states have the same keyboard/gamepad gate. */
    v->menu_open=1;mouse_weapon_cycle_press(v,4);assert(v->weapon_delta==0);
    v->menu_open=0;v->frontend_input=1;
    mouse_weapon_cycle_press(v,5);assert(v->weapon_delta==0);
    v->frontend_input=0;v->capture_mode=4;
    mouse_weapon_cycle_press(v,4);assert(v->weapon_delta==0);
    v->capture_mode=0;v->mouse_grabbed=0;
    mouse_weapon_cycle_press(v,5);assert(v->weapon_delta==0);
    v->mouse_grabbed=1;
    /* Unassigned buttons cannot cycle weapons, and normal gameplay resumes. */
    mouse_weapon_cycle_press(v,1);assert(v->weapon_delta==0);
    mouse_weapon_cycle_press(v,5);assert(v->weapon_delta==1);
    free(v);
    puts("R190 mouse weapon cycling honors terminal input ownership PASS");
    return 0;
}
