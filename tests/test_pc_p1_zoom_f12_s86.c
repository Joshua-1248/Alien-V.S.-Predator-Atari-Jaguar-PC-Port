/* Player 1 F12 joystick zoom choices share the existing co-op profile. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/platform/pc/pc_video_x11.c"
s16 player_type=PT_HUMAN;
void avp_mp_input_write_settings(const AvpMpInputConfig *p,FILE *f){(void)p;(void)f;}
void avp_mazescrn_set_facehug_shake_pain_sounds(int x){(void)x;}
void avp_player_set_facehug_pain_sounds(int x){(void)x;}
void avp_player_set_pain_sounds(int x){(void)x;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *p){(void)p;return 0;}
int main(void){
    AvpPcVideo *v=calloc(1,sizeof(*v));int found;
    assert(v);settings_defaults(&v->cfg);
    for(int mode=0;mode<2;mode++){
        v->cfg.controller_mode_jaguar=mode;
        v->menu_page=mode?MENU_GAMEPAD_JAGUAR:MENU_GAMEPAD_PC;
        found=0;
        for(int row=0;row<menu_count(v);row++){
            int id=menu_id(v,row);
            if(id!=164&&id!=165)continue;
            found++;
            v->menu_cursor=row;
            menu_choose(v);
            assert(v->capture_mode==1&&v->capture_target==id);
            bind_button_token(v,mode+5+(id-164));
            assert(v->local_mp_controls.player[0].gamepad_button[
                id==164?AVP_MP_MAP_ZOOM_IN:AVP_MP_MAP_ZOOM_OUT]==mode+5+(id-164));
            clear_binding(v,id);
            assert(v->local_mp_controls.player[0].gamepad_button[
                id==164?AVP_MP_MAP_ZOOM_IN:AVP_MP_MAP_ZOOM_OUT]==-1);
        }
        assert(found==2);
    }
    free(v);
    puts("S86 P1 F12 joystick zoom entries, capture and unbind PASS");
    return 0;
}
