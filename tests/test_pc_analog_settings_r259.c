/* Test the actual F12 settings table, persistence and separate enhancements. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/platform/pc/pc_video_x11.c"
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof *v);
    FILE *f;char line[256];int saw_buffer=0,saw_enhanced=0;
    assert(v);
    settings_defaults(&v->cfg);
    v->mouse_focused=1; /* Real host window begins focused. */
    assert(!v->cfg.buffer_short_analog&&!v->cfg.buffer_short_presses);
    assert(!v->cfg.enhanced_analog_controls);
    assert(!v->cfg.marine_pulse_los_hits);
    assert(AVP_MENU_ITEMS==159);
    assert(menu_count(v)==4);
    v->menu_page=MENU_ENHANCEMENTS;
    assert(menu_id(v,menu_count(v)-3)==148);
    assert(menu_id(v,menu_count(v)-2)==157);
    snprintf(v->settings_path,sizeof v->settings_path,"/tmp/avp-r259-analog-settings-%ld.cfg",(long)getpid());
    v->menu_index=148;menu_adjust(v,1);
    assert(v->cfg.buffer_short_analog&&!v->cfg.buffer_short_presses);
    v->menu_index=157;menu_adjust(v,1);
    assert(v->cfg.marine_pulse_los_hits);
    v->menu_index=147;menu_adjust(v,1);
    assert(v->cfg.enhanced_analog_controls);
    f=fopen(v->settings_path,"r");assert(f);
    while(fgets(line,sizeof line,f)){
        if(strstr(line,"enhancement.buffer_short_analog=true"))saw_buffer=1;
        if(strstr(line,"enhancement.enhanced_analog_controls=true"))saw_enhanced=1;
    }
    fclose(f);assert(saw_buffer&&saw_enhanced);
    v->menu_index=157;menu_adjust(v,1);
    assert(!v->cfg.marine_pulse_los_hits);
    /* Real host bridge: a one-shot axis pulse must not replay across two
     * catch-up simulation ticks with no intervening host input poll. */
    v->cfg.analog_movement=1;
    v->cfg.gamepad_enabled=1;
    v->js_fd=1;
    v->cfg.buffer_short_analog=1;
    v->modern_forward=v->modern_strafe=v->modern_turn=0.0f;
    avp_pc_analog_buffer_poll(&v->analog_buffer,0.65f,-0.25f,0.8f,1);
    avp_pc_video_simulation_pad(v,0xffffffffu);
    assert(v->sim_forward==0.65f&&v->sim_strafe==-0.25f&&v->sim_turn==0.8f);
    avp_pc_video_simulation_pad(v,0xffffffffu);
    assert(v->sim_forward==0.0f&&v->sim_strafe==0.0f&&v->sim_turn==0.0f);
    /* Current stick motion takes priority over a pending earlier pulse. */
    avp_pc_analog_buffer_poll(&v->analog_buffer,0.1f,0,0,1);
    v->modern_forward=-0.4f;
    avp_pc_video_simulation_pad(v,0xffffffffu);
    assert(v->sim_forward==-0.4f);
    v->cfg.buffer_short_analog=0;
    v->modern_forward=0;
    avp_pc_analog_buffer_poll(&v->analog_buffer,0.5f,0,0,1);
    avp_pc_video_simulation_pad(v,0xffffffffu);
    assert(v->sim_forward==0.0f); /* enhancement disabled */
    unlink(v->settings_path);free(v);
    puts("R259 F12 analog settings and persisted flags PASS");
    return 0;
}
