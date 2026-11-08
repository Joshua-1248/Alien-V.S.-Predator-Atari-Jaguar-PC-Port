/* Actual PC-action key-state/gating tests, plus original Jaguar pad mapping. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/platform/pc/pc_video_x11.c"
s16 player_type=PT_HUMAN;
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}

static void check_chord(AvpPcVideo *v,int tl,int tr,int sl,int sr,
                        int expected_strafe,int pad_left,int pad_right,int modifier)
{
    u32 p=0xffffffffu;
    setkey(v,10,(KeySym)v->cfg.key_turn_left,tl);
    setkey(v,11,(KeySym)v->cfg.key_turn_right,tr);
    setkey(v,12,(KeySym)v->cfg.key_strafe_left,sl);
    setkey(v,13,(KeySym)v->cfg.key_strafe_right,sr);
    p=avp_pc_keyboard_action_horizontal_pad(p,tl,tr,sl,sr,
                 player_type==PT_ALIEN?OPTION:FIRE_C);
    assert(avp_pc_video_keyboard_strafe_during_turn(v)==expected_strafe);
    assert((!(p&(1u<<JOY_LEFT)))==pad_left);
    assert((!(p&(1u<<JOY_RIGHT)))==pad_right);
    assert((!(p&(1u<<(player_type==PT_ALIEN?OPTION:FIRE_C))))==modifier);
}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));assert(v);
    settings_defaults(&v->cfg);v->mouse_focused=1;
    for(int species=0;species<3;species++){
        player_type=species==0?PT_HUMAN:(species==1?PT_ALIEN:PT_PREDATOR);
        check_chord(v,0,1,1,0,-1,0,1,1); /* Q + D: turn right, strafe left */
        check_chord(v,1,0,0,1,+1,1,0,1); /* E + A: turn left, strafe right */
        check_chord(v,1,0,1,0,-1,1,0,1); /* Q + A: left turn + left strafe */
        check_chord(v,0,1,0,1,+1,0,1,1); /* E + D: right turn + right strafe */
        check_chord(v,0,0,1,0,0,1,0,0);  /* Q alone: normal retail strafe */
        check_chord(v,0,0,0,1,0,0,1,0);  /* E alone */
        check_chord(v,0,1,0,0,0,0,1,1);  /* D alone: original chord */
        check_chord(v,1,0,0,0,0,1,0,1);  /* A alone */
        check_chord(v,0,1,1,1,0,0,1,1);  /* opposing strafe keys neutral */
        check_chord(v,1,1,0,0,0,0,0,0);  /* A+D: absolutely no yaw */
        check_chord(v,1,1,1,0,0,1,0,0);  /* A+D+Q: strafe, no yaw */
        check_chord(v,1,1,0,1,0,0,1,0);  /* A+D+E: strafe, no yaw */
    }
    check_chord(v,0,1,1,0,-1,0,1,1);
    v->menu_open=1;assert(!avp_pc_video_keyboard_strafe_during_turn(v));v->menu_open=0;
    v->frontend_input=1;assert(!avp_pc_video_keyboard_strafe_during_turn(v));v->frontend_input=0;
    v->capture_mode=1;assert(!avp_pc_video_keyboard_strafe_during_turn(v));v->capture_mode=0;
    v->mouse_focused=0;assert(!avp_pc_video_keyboard_strafe_during_turn(v));v->mouse_focused=1;
    v->cfg.controller_mode_jaguar=1;
    assert(!avp_pc_video_keyboard_strafe_during_turn(v));
    v->cfg.controller_mode_jaguar=0;
    v->cfg.buffer_short_presses=1;
    v->pad_buffered=(1u<<JOY_LEFT);
    v->previous_poll_pad=0xffffffffu;
    /* Buffered Q must not reinsert the left pad bit while D is turning. */
    {u32 p=avp_pc_keyboard_action_horizontal_pad(0xffffffffu,0,1,1,0,FIRE_C);
     p=avp_pc_video_simulation_pad(v,p);
     assert(!(p&(1u<<JOY_RIGHT)) && (p&(1u<<JOY_LEFT)));
     assert(!v->pad_buffered);}
    setkey(v,10,(KeySym)v->cfg.key_turn_left,0);
    setkey(v,11,(KeySym)v->cfg.key_turn_right,0);
    setkey(v,12,(KeySym)v->cfg.key_strafe_left,0);
    setkey(v,13,(KeySym)v->cfg.key_strafe_right,0);
    /* Buffered alternating taps within one 10-Hz tick must never turn into
     * a simultaneous left+right chord. Newest press wins until consumed. */
    for(int species=0;species<3;species++){
        u32 pad;
        player_type=species==0?PT_HUMAN:(species==1?PT_ALIEN:PT_PREDATOR);
        v->pad_buffered=0;
        short_key_press(v,(KeySym)v->cfg.key_turn_left);
        short_key_press(v,(KeySym)v->cfg.key_turn_right);
        assert((v->pad_buffered&(1u<<JOY_LEFT))==0);
        pad=avp_pc_video_simulation_pad(v,0xffffffffu);
        assert(!(pad&(1u<<JOY_RIGHT)) && (pad&(1u<<JOY_LEFT)));
        short_key_press(v,(KeySym)v->cfg.key_turn_right);
        short_key_press(v,(KeySym)v->cfg.key_turn_left);
        pad=avp_pc_video_simulation_pad(v,0xffffffffu);
        assert(!(pad&(1u<<JOY_LEFT)) && (pad&(1u<<JOY_RIGHT)));
        /* Held A+D always neutral, even with an old buffered press. */
        setkey(v,10,(KeySym)v->cfg.key_turn_left,1);
        setkey(v,11,(KeySym)v->cfg.key_turn_right,1);
        short_key_press(v,(KeySym)v->cfg.key_turn_left);
        pad=avp_pc_video_simulation_pad(v,0xffffffffu);
        assert((pad&(1u<<JOY_LEFT)) && (pad&(1u<<JOY_RIGHT)));
        assert(pad&(1u<<(player_type==PT_ALIEN?OPTION:FIRE_C)));
        setkey(v,10,(KeySym)v->cfg.key_turn_left,0);
        setkey(v,11,(KeySym)v->cfg.key_turn_right,0);
    }
    free(v);
    puts("S86 keyboard taps, A+D cancellation, strafe chords and short buffer PASS");
    return 0;
}
