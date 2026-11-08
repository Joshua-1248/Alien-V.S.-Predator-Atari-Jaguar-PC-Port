/* Compile the real static menu/config routines without an X11 session. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/platform/pc/pc_video_x11.c"
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
static int contains(AvpPcVideo *v,int page,int id){
    v->menu_page=page;
    for(int i=0;i<menu_count(v);i++)if(menu_id(v,i)==id)return 1;
    return 0;
}
int main(void){
    AvpPcVideo *v=calloc(1,sizeof(*v));assert(v);
    settings_defaults(&v->cfg);
    assert(v->cfg.controller_mode_jaguar==0);
    assert(v->cfg.buffer_short_presses==0);
    assert(v->cfg.action_key[3]==XK_v);
    assert(contains(v,MENU_CONTROLS,25));
    assert(contains(v,MENU_GAMEPAD_PC,71)); /* Alien bite button */
    assert(!contains(v,MENU_GAMEPAD_PC,30));
    assert(contains(v,MENU_GAMEPAD_JAGUAR,30));
    assert(!contains(v,MENU_GAMEPAD_JAGUAR,71));
    assert(contains(v,MENU_GAMEPAD_FRONTEND,117));
    assert(contains(v,MENU_KEYS_PC,63));
    for(int i=0;i<16;i++)assert(contains(v,MENU_GAMEPAD_AXES,130+i));
    assert(contains(v,MENU_ENHANCEMENTS,146));
    assert(!contains(v,MENU_MISC,146));
    snprintf(v->settings_path,sizeof(v->settings_path),"/tmp/avp-r257-controls-%ld.cfg",(long)getpid());
    v->menu_index=134;menu_adjust(v,1);
    assert(v->cfg.axis_deadzone[4]>0.24f&&v->cfg.axis_deadzone[3]==0.20f);
    v->menu_index=146;menu_adjust(v,1);
    assert(v->cfg.buffer_short_presses==1);
    v->menu_page=MENU_CONTROLS;v->menu_cursor=0;
    menu_choose(v);assert(v->cfg.controller_mode_jaguar==1);
    v->menu_page=MENU_GAMEPAD;v->menu_cursor=1;
    menu_choose(v);assert(v->menu_page==MENU_GAMEPAD_JAGUAR);
    menu_back(v);assert(v->menu_page==MENU_GAMEPAD);
    v->cfg.controller_mode_jaguar=0;
    v->menu_cursor=1;menu_choose(v);assert(v->menu_page==MENU_GAMEPAD_PC);
    /* Load the persisted independent per-axis deadzone and new option. */
    AvpPcVideo *w=calloc(1,sizeof(*w));assert(w);
    snprintf(w->settings_path,sizeof(w->settings_path),"%s",v->settings_path);
    FILE *f=fopen(v->settings_path,"r");assert(f);
    int dz=0,enh=0;char line[128];
    while(fgets(line,sizeof(line),f)){
        if(strncmp(line,"gamepad.axis_deadzone_4=0.25",27)==0)dz=1;
        if(strncmp(line,"enhancement.buffer_short_presses=true",37)==0)enh=1;
    }
    fclose(f);assert(dz&&enh);
    unlink(v->settings_path);free(w);free(v);
    return 0;
}
