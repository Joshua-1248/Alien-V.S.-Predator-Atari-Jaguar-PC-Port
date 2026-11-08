/* Noninteractive regression of the PC-only Jaguar frontend binding editor.
 * Original Jaguar CPU/input translation stays completely unchanged. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/platform/pc/pc_video_x11.c"
static int menu_contains(AvpPcVideo *v,int page,int id){
    v->menu_page=page;
    for(int i=0;i<menu_count(v);i++)if(menu_id(v,i)==id)return 1;
    return 0;
}
int main(void){
    AvpPcVideo *v=calloc(1,sizeof(*v));
    assert(v);
    settings_defaults(&v->cfg);
    for(int i=0;i<6;i++){
        assert(menu_contains(v,MENU_KEYS_FRONTEND,111+i));
        assert(menu_contains(v,MENU_GAMEPAD_FRONTEND,117+i));
    }
    assert(v->cfg.menu_key_up==XK_Up && v->cfg.menu_key_back==XK_Escape);
    assert(v->cfg.menu_button_up==-1 && v->cfg.menu_button_back==-1);
    snprintf(v->settings_path,sizeof(v->settings_path),"/tmp/avp-r143-bindings-%ld.cfg",(long)getpid());
    apply_capture_key(v,XK_KP_8);
    assert(v->cfg.menu_key_up==XK_Up); /* target is not set */
    v->capture_target=111;
    apply_capture_key(v,XK_KP_8);
    assert(v->cfg.menu_key_up==XK_KP_8 && v->cfg.key_forward==XK_w);
    v->capture_target=115;
    apply_capture_key(v,XK_KP_Enter);
    assert(v->cfg.menu_key_accept==XK_KP_Enter);
    v->js_buttons_count=8;
    v->capture_target=121;
    bind_button_token(v,4);
    assert(v->cfg.menu_button_accept==4 && v->cfg.jag_a_button==0);
    v->js_buttons[4]=1;
    assert(button_down(v,v->cfg.menu_button_accept));
    v->js_buttons[4]=0;
    assert(!button_down(v,v->cfg.menu_button_accept));
    v->capture_target=117;
    bind_button_token(v,avp_pc_axis_button_token(2,1));
    v->js_axes_count=4;
    v->js_axes[2]=29000;
    assert(button_down(v,v->cfg.menu_button_up));
    v->js_axes[2]=0;
    assert(!button_down(v,v->cfg.menu_button_up));
    clear_binding(v,117);
    assert(v->cfg.menu_button_up==-1);
    FILE *f=fopen(v->settings_path,"r");assert(f);
    char line[200];int got_key=0,got_button=0,got_default=0;
    while(fgets(line,sizeof(line),f)){
        if(strncmp(line,"menu.key_up=",12)==0&&strstr(line,"0xffb8"))got_key=1;
        if(strcmp(line,"menu.button_accept=4\n")==0)got_button=1;
        if(strcmp(line,"menu.button_up=-1\n")==0)got_default=1;
    }
    fclose(f);unlink(v->settings_path);
    assert(got_key&&got_button&&got_default);
    free(v);
    puts("R143 PC frontend key/button bindings, axis capture, persistence, and clear PASS");
    return 0;
}
