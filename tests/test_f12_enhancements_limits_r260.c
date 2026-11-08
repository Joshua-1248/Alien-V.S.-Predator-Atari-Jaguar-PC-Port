/* R260: verify production F12 enhancement routing and maximum analog turn settings. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/platform/pc/pc_video_x11.c"
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
static int count_id(AvpPcVideo *v,int page,int id)
{
    int n=0;
    v->menu_page=page;
    for(int i=0;i<menu_count(v);i++)if(menu_id(v,i)==id)n++;
    return n;
}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof *v);
    FILE *f;
    char line[256];
    int sensitivity_saved=0,speed_saved=0;
    assert(v);
    settings_defaults(&v->cfg);
    snprintf(v->settings_path,sizeof v->settings_path,
        "/tmp/avp-r260-enhancements-limits-%ld.cfg",(long)getpid());
    v->menu_open=1;
    v->menu_page=MENU_ROOT;
    assert(menu_count(v)==4);
    v->menu_cursor=1;
    menu_choose(v);
    assert(v->menu_page==MENU_ENHANCEMENTS);
    assert(menu_count(v)==7);
    for(int i=0;i<7;i++){
        int id=enhancement_items[i];
        assert(count_id(v,MENU_ENHANCEMENTS,id)==1);
        assert(count_id(v,MENU_MISC,id)==0);
        assert(count_id(v,MENU_GAMEPAD_AXES,id)==0);
    }
    assert(count_id(v,MENU_GAMEPAD_AXES,79)==1);
    assert(count_id(v,MENU_GAMEPAD_AXES,80)==1);
    assert(count_id(v,MENU_GAMEPAD_AXES,130)==1);
    assert(count_id(v,MENU_GAMEPAD_AXES,145)==1);
    v->menu_page=MENU_ENHANCEMENTS;
    assert(f12_backspace(v,XK_BackSpace));
    assert(v->menu_page==MENU_ROOT);
    v->menu_page=MENU_ENHANCEMENTS;
    /* Navigating to options does not silently enable them. */
    assert(!v->cfg.buffer_short_presses && !v->cfg.buffer_short_analog &&
           !v->cfg.enhanced_analog_controls);
    v->menu_cursor=3;
    menu_choose(v);
    assert(v->cfg.enhanced_analog_controls);
    assert(!v->cfg.buffer_short_analog);
    v->menu_cursor=4;
    menu_choose(v);
    assert(v->cfg.buffer_short_analog);
    assert(!v->cfg.buffer_short_presses);
    /* Both GUI controls must reach, but never exceed, the requested 5.00. */
    v->menu_index=79;
    for(int i=0;i<120;i++)menu_adjust(v,1);
    assert(v->cfg.turn_sensitivity==5.0f);
    v->menu_index=80;
    for(int i=0;i<120;i++)menu_adjust(v,1);
    assert(v->cfg.turn_speed==5.0f);
    for(int i=0;i<15;i++){
        v->menu_index=79;menu_adjust(v,1);
        v->menu_index=80;menu_adjust(v,1);
    }
    assert(v->cfg.turn_sensitivity==5.0f&&v->cfg.turn_speed==5.0f);
    f=fopen(v->settings_path,"r");assert(f);
    while(fgets(line,sizeof line,f)){
        if(!strcmp(line,"controls.turn_sensitivity=5.00\n"))sensitivity_saved=1;
        if(!strcmp(line,"controls.turn_speed=5.00\n"))speed_saved=1;
    }
    fclose(f);
    assert(sensitivity_saved&&speed_saved);
    v->menu_index=79;
    for(int i=0;i<120;i++)menu_adjust(v,-1);
    assert(v->cfg.turn_sensitivity==0.25f);
    v->menu_index=80;
    for(int i=0;i<120;i++)menu_adjust(v,-1);
    assert(v->cfg.turn_speed==0.25f);
    unlink(v->settings_path);
    free(v);
    puts("R260 F12 enhancement category and 5.00 analog turn limits PASS");
    return 0;
}
