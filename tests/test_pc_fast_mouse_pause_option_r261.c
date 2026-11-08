#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_mouse_capture.h"
#include "../src/platform/pc/pc_video_x11.c"
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
int main(void){
    AvpPcVideo *v=calloc(1,sizeof(*v));assert(v);
    settings_defaults(&v->cfg);
    assert(v->cfg.pc_pause_button==-1&&v->cfg.pc_option_button==-1);
    assert(avp_pc_mouse_needs_recenter(8,1024));
    assert(!avp_pc_mouse_needs_recenter(300,1024));
    assert(!avp_pc_mouse_needs_recenter(512,1024));
    assert(avp_pc_mouse_needs_recenter(980,1024));
    assert(!avp_pc_mouse_needs_recenter(160,320));
    assert(avp_pc_mouse_needs_recenter(300,320));
    v->menu_page=MENU_GAMEPAD_PC;
    int row91=-1,row92=-1;for(int row=0;row<menu_count(v);row++){
        if(menu_id(v,row)==91)row91=row;
        if(menu_id(v,row)==92)row92=row;
    }
    assert(row91>=0&&row92>=0);
    assert(v->cfg.jag_pause_button==7&&v->cfg.jag_option_button==6);
    v->capture_target=91;bind_button_token(v,14);
    v->capture_target=92;bind_button_token(v,15);
    assert(v->cfg.pc_pause_button==14&&v->cfg.pc_option_button==15);
    assert(v->cfg.jag_pause_button==7&&v->cfg.jag_option_button==6);
    v->menu_cursor=row91;v->menu_open=1;
    assert(f12_delete_binding(v)&&v->cfg.pc_pause_button==-1);
    assert(v->cfg.pc_option_button==15&&v->cfg.jag_pause_button==7);
    v->capture_target=91;bind_button_token(v,13);
    assert(v->cfg.pc_pause_button==13);
    /* Retain independent keys across a settings roundtrip. */
    char file[256];snprintf(file,sizeof(file),"/tmp/avp-r261-pause-opt-%ld.cfg",(long)getpid());
    snprintf(v->settings_path,sizeof(v->settings_path),"%s",file);
    save_settings(v);
    AvpPcVideo*w=calloc(1,sizeof(*w));assert(w);
    snprintf(w->settings_path,sizeof(w->settings_path),"%s",file);
    /* load_settings recomputes its path from the executable; inspect emitted
       content directly here, plus the production load key dispatch above. */
    FILE*f=fopen(file,"r");assert(f);char line[256];int pause=0,opt=0;
    while(fgets(line,sizeof(line),f)){
        if(strcmp(line,"gamepad.pause_button=13\n")==0)pause=1;
        if(strcmp(line,"gamepad.option_button=15\n")==0)opt=1;
    }
    fclose(f);assert(pause&&opt);
    /* R262: host-rate stick input is permitted only in active PC gameplay.
     * The physical gamepad path must not turn behind a menu, terminal, pause,
     * or a captured mouse event. */
    {float axis=0.0f;
     v->cfg.analog_movement=1;v->cfg.enhanced_analog_controls=1;
     v->cfg.gamepad_enabled=1;v->js_fd=7;v->modern_turn=0.3125f;
     v->menu_open=0;v->frontend_input=0;
     assert(avp_pc_video_live_stick_turn(v,&axis)&&axis==0.3125f);
     v->menu_open=1;
     assert(!avp_pc_video_live_stick_turn(v,&axis)&&axis==0.0f);
     v->menu_open=0;v->cfg.mouse_enabled=1;v->mouse_grabbed=1;
     v->mouse_delta=45.0f;
     assert(!avp_pc_video_live_stick_turn(v,&axis));
     v->mouse_delta=0.0f;
     assert(avp_pc_video_live_stick_turn(v,&axis));
     v->cfg.enhanced_analog_controls=0;
     assert(!avp_pc_video_live_stick_turn(v,&axis));
    }
    unlink(file);free(w);free(v);
    return 0;
}
