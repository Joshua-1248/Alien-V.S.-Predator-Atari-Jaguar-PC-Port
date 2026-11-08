#include "local_mp_input.h"
#include "joypad.h"
#include "local_mp_mice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define CHECK(X) do{if(!(X)){fprintf(stderr,"FAIL: %s line %d\n",#X,__LINE__);return 1;}}while(0)
int main(void)
{
    AvpMpInputConfig c,copy;AvpMpMouseState s[4]={{0}};char line[512];FILE *f;
    avp_mp_input_defaults(&c);
    CHECK(c.player[0].key[AVP_MP_FIRE]==0 && c.player[1].key[AVP_MP_FIRE]==0);
    /* R3: shared P1 automap controls work by default in SP and MP. */
    CHECK(c.player[0].key[AVP_MP_MAP_ZOOM_IN]==0x003dul);
    CHECK(c.player[0].key[AVP_MP_MAP_ZOOM_OUT]==0x002dul);
    CHECK(c.player[1].key[AVP_MP_MAP_ZOOM_IN]==0ul);
    CHECK(c.player[1].key[AVP_MP_MAP_ZOOM_OUT]==0ul);
    CHECK(avp_mp_input_assign_mouse(&c,0,"/dev/input/by-id/mouse-zero-event-mouse"));
    CHECK(avp_mp_input_assign_mouse(&c,1,"/dev/input/by-id/mouse-one-event-mouse"));
    CHECK(!avp_mp_input_assign_mouse(&c,2,"/dev/input/by-id/mouse-one-event-mouse"));
    CHECK(avp_mp_input_assign_mouse(&c,2,"/dev/input/by-id/mouse-two-event-mouse"));
    CHECK(avp_mp_input_mouse_owner(&c,"/dev/input/by-id/mouse-one-event-mouse")==1);
    CHECK(avp_mp_input_mouse_motion(&c,s,"/dev/input/by-id/mouse-one-event-mouse",13,-7));
    CHECK(avp_mp_input_mouse_motion(&c,s,"/dev/input/by-id/mouse-two-event-mouse",-4,2));
    CHECK(s[1].dx==13 && s[1].dy==-7 && s[2].dx==-4 && s[0].dx==0);
    CHECK(avp_mp_input_mouse_button(&c,s,"/dev/input/by-id/mouse-two-event-mouse",3,1));
    CHECK(s[2].buttons==(1u<<2) && s[1].buttons==0);
    CHECK(!avp_mp_input_mouse_motion(&c,s,"unknown",88,99));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.key_0","0xff51"));
    /* S113: existing action IDs unchanged; new terminal EXIT is last. */
    CHECK(AVP_MP_TERMINAL_NEXT==20u && AVP_MP_TERMINAL_PREVIOUS==21u);
    CHECK(AVP_MP_MAP_ZOOM_OUT==23u && AVP_MP_TERMINAL_EXIT==24u);
    CHECK(!strcmp(avp_mp_input_action_name(AVP_MP_TERMINAL_EXIT),"Terminal Exit"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.key_24","0xff1b"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.gamepad_button_24","8"));
    {
        int await_release=1;
        const uint32_t b=1u<<FIRE_B;
        /* Use is held when the actor opens the computer: no automatic B. */
        CHECK((avp_mp_terminal_initial_use_guard(UINT32_MAX&~b,&await_release)&b)!=0);
        CHECK(await_release);
        CHECK((avp_mp_terminal_initial_use_guard(UINT32_MAX&~b,&await_release)&b)!=0);
        CHECK(await_release);
        CHECK((avp_mp_terminal_initial_use_guard(UINT32_MAX,&await_release)&b)!=0);
        CHECK(!await_release);
        /* Once released, new B/Previous presses reach original COMPUTER.S. */
        CHECK((avp_mp_terminal_initial_use_guard(UINT32_MAX&~b,&await_release)&b)==0);
    }
    CHECK(avp_mp_terminal_actions_pad(0u)==UINT32_MAX);
    CHECK((avp_mp_terminal_actions_pad((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_TERMINAL_NEXT))&(1u<<JOY_UP))==0u);
    CHECK((avp_mp_terminal_actions_pad(1u<<AVP_MP_TERMINAL_NEXT)&(1u<<FIRE_A))==0u);
    CHECK((avp_mp_terminal_actions_pad(1u<<AVP_MP_USE)&(1u<<FIRE_B))==0u);
    CHECK((avp_mp_terminal_actions_pad(1u<<AVP_MP_TERMINAL_EXIT)&(1u<<FIRE_C))==0u);
    CHECK((avp_mp_terminal_actions_pad(1u<<AVP_MP_OPTION)&(1u<<FIRE_C))==0u);
    CHECK((avp_mp_terminal_actions_pad(1u<<AVP_MP_TERMINAL_EXIT)&(1u<<FIRE_A))!=0u);
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.key_25","5"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.mouse_button_2","999"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.mouse_device","/dev/input/by-id/mouse-one-event-mouse"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.gamepad_button_2","7"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.gamepad_axis_1","5"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.gamepad_axis_1_invert","1"));
    CHECK(c.player[2].gamepad_axis[1]==5 && c.player[2].gamepad_axis_invert[1]);
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.gamepad_analog_movement","1"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.gamepad_enhanced_analog_controls","1"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.gamepad_turn_sensitivity","2.5"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.gamepad_turn_speed","3.0"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.gamepad_turn_sensitivity","0.5"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.gamepad_turn_speed","nan"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.gamepad_turn_speed","10"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.gamepad_analog_movement","2"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.mouse_sensitivity","2.5"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.mouse_acceleration","nan"));
    CHECK(c.player[0].audio_player_pain && c.player[3].audio_facehug_shake);
    /* One-to-one parser pointer mapping: all toggles remain independent. */
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.audio_facehug_pain","0"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.audio_facehug_shake","0"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.remember_automap","1"));
    CHECK(!c.player[1].audio_facehug_pain && !c.player[1].audio_facehug_shake);
    CHECK(c.player[1].remember_automap && !c.player[0].remember_automap && !c.player[2].remember_automap);
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.remember_automap","2"));
    CHECK(c.player[3].audio_facehug_shake && !c.player[3].remember_automap);
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.cheat_infinite_health","1"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.audio_player_pain","0"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.cheat_noclip","1"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.audio_motion_tracker","0"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p2.cheat_no_facehug","1"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p3.cheat_no_enemy_attack","1"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.cheat_no_enemy_attack","2"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p1.gamepad_button_22","6"));
    CHECK(avp_mp_input_parse_setting(&c,"local_multiplayer.input.p1.gamepad_button_23","7"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.audio_player_pain","2"));
    CHECK(!avp_mp_input_parse_setting(&c,"local_multiplayer.input.p4.cheat_tracker","nan"));
    CHECK(c.player[2].cheat_infinite_health && !c.player[0].cheat_infinite_health);
    CHECK(!c.player[3].audio_player_pain && c.player[0].audio_player_pain);
    f=tmpfile();CHECK(f);
    avp_mp_input_write_settings(&c,f);rewind(f);
    avp_mp_input_defaults(&copy);
    while(fgets(line,sizeof(line),f)){
        char *eq=strchr(line,'=');char *nl;
        if(!eq)continue;
        *eq++=0;nl=strchr(eq,'\n');if(nl)*nl=0;
        CHECK(avp_mp_input_parse_setting(&copy,line,eq));
    }
    fclose(f);
    avp_mp_input_validate(&copy);
    CHECK(!strcmp(copy.player[0].mouse_path,c.player[0].mouse_path));
    CHECK(!strcmp(copy.player[1].mouse_path,c.player[1].mouse_path));
    CHECK(!strcmp(copy.player[2].mouse_path,c.player[2].mouse_path));
    CHECK(copy.player[3].key[0]==0xff51ul);
    CHECK(copy.player[1].key[AVP_MP_TERMINAL_EXIT]==0xff1bul);
    CHECK(copy.player[2].gamepad_button[AVP_MP_TERMINAL_EXIT]==8);
    CHECK(copy.player[2].gamepad_button[2]==7);
    CHECK(copy.player[2].gamepad_axis[1]==5 && copy.player[2].gamepad_axis_invert[1]);
    CHECK(copy.player[2].mouse_sensitivity==2.5f);
    CHECK(copy.player[1].gamepad_analog_movement==1);
    CHECK(copy.player[1].gamepad_enhanced_analog_controls==1);
    CHECK(copy.player[1].gamepad_turn_sensitivity==2.5f);
    CHECK(copy.player[1].gamepad_turn_speed==3.0f);
    CHECK(copy.player[2].gamepad_turn_sensitivity==0.5f);
    CHECK(copy.player[2].gamepad_turn_speed==1.0f);
    CHECK(copy.player[2].cheat_infinite_health && !copy.player[1].cheat_infinite_health);
    CHECK(copy.player[1].cheat_noclip && !copy.player[0].cheat_noclip);
    CHECK(copy.player[1].cheat_no_facehug && !copy.player[0].cheat_no_facehug);
    CHECK(copy.player[2].cheat_no_enemy_attack && !copy.player[1].cheat_no_enemy_attack);
    CHECK(copy.player[0].gamepad_button[AVP_MP_MAP_ZOOM_IN]==6);
    CHECK(copy.player[0].gamepad_button[AVP_MP_MAP_ZOOM_OUT]==7);
    CHECK(copy.player[0].key[AVP_MP_MAP_ZOOM_IN]==0x003dul);
    CHECK(copy.player[0].key[AVP_MP_MAP_ZOOM_OUT]==0x002dul);
    CHECK(!copy.player[3].audio_player_pain && copy.player[2].audio_player_pain);
    CHECK(!copy.player[1].audio_motion_tracker && copy.player[0].audio_motion_tracker);
    CHECK(copy.player[1].remember_automap && !copy.player[0].remember_automap);
    CHECK(!copy.player[1].audio_facehug_pain && !copy.player[1].audio_facehug_shake);
    CHECK(copy.player[2].audio_facehug_pain && copy.player[2].audio_facehug_shake);
    copy.player[3].gamepad_turn_sensitivity=99.0f;
    copy.player[3].gamepad_turn_speed=-1.0f;
    avp_mp_input_validate(&copy);
    CHECK(copy.player[3].gamepad_turn_sensitivity==5.0f);
    CHECK(copy.player[3].gamepad_turn_speed==0.25f);
    CHECK(avp_mp_input_parse_setting(&copy,"local_multiplayer.input.p2.gamepad_device","/dev/input/js1"));
    CHECK(avp_mp_input_parse_setting(&copy,"local_multiplayer.input.p3.gamepad_device","/dev/input/js1"));
    avp_mp_input_validate(&copy);
    CHECK(copy.player[1].gamepad_path[0] && !copy.player[2].gamepad_path[0]);
    {
        /* Reproduction: P1 owns js0, P2 owns js1. P3 selection must skip
         * both and reach js2, not bounce back to Unassigned. */
        static const char *const pads[]={"/dev/input/js0","/dev/input/js1",
              "/dev/input/js2","/dev/input/js3"};
        AvpMpInputConfig cycle;
        avp_mp_input_defaults(&cycle);
        strcpy(cycle.player[1].gamepad_path,pads[1]);
        CHECK(avp_mp_input_cycle_gamepad(&cycle,2,pads,4,pads[0],1));
        CHECK(!strcmp(cycle.player[2].gamepad_path,pads[2]));
        avp_mp_input_validate(&cycle);
        CHECK(!strcmp(cycle.player[2].gamepad_path,pads[2]));
        CHECK(avp_mp_input_cycle_gamepad(&cycle,3,pads,4,pads[0],1));
        CHECK(!strcmp(cycle.player[3].gamepad_path,pads[3]));
        CHECK(avp_mp_input_cycle_gamepad(&cycle,3,pads,4,pads[0],1));
        CHECK(!cycle.player[3].gamepad_path[0]); /* explicit Unassigned */
        CHECK(avp_mp_input_cycle_gamepad(&cycle,3,pads,4,pads[0],-1));
        CHECK(!strcmp(cycle.player[3].gamepad_path,pads[3]));
        CHECK(avp_mp_input_cycle_gamepad(&cycle,2,pads,4,pads[0],-1));
        CHECK(!cycle.player[2].gamepad_path[0]); /* occupied js1/js0 skipped */
        CHECK(avp_mp_input_cycle_gamepad(&cycle,2,pads,4,pads[0],1));
        CHECK(!strcmp(cycle.player[2].gamepad_path,pads[2]));
        CHECK(avp_mp_input_cycle_gamepad(&cycle,2,pads,0,pads[0],1));
        CHECK(!cycle.player[2].gamepad_path[0]);
        /* If all devices are occupied, Unassigned must remain selectable. */
        CHECK(avp_mp_input_cycle_gamepad(&cycle,2,pads,2,pads[0],1));
        CHECK(!cycle.player[2].gamepad_path[0]);
        CHECK(!avp_mp_input_cycle_gamepad(&cycle,0,pads,4,pads[0],1));
        CHECK(!avp_mp_input_cycle_gamepad(&cycle,4,pads,4,pads[0],1));
    }
    /* R23: bot-control input can feed all 16 native actor slots. The
     * host previously only translated weapon controls for human P1-P4;
     * headless P5-P16 would never actually change weapons. Independent
     * per-actor edges must not bleed across neighboring bot slots. */
    {
        uint32_t prior[16]={0};
        const uint32_t next=1u<<AVP_MP_NEXT_WEAPON;
        const uint32_t prev=1u<<AVP_MP_PREV_WEAPON;
        for(unsigned p=0;p<16u;p++){
            CHECK(avp_mp_input_weapon_cycle_edge(next,&prior[p])==1);
            CHECK(avp_mp_input_weapon_cycle_edge(next,&prior[p])==0);
        }
        CHECK(avp_mp_input_weapon_cycle_edge(prev,&prior[15])==-1);
        CHECK(prior[0]==next && prior[15]==prev);
        CHECK(avp_mp_input_weapon_cycle_edge(0u,&prior[0])==0);
        CHECK(avp_mp_input_weapon_cycle_edge(prev,&prior[0])==-1);
        CHECK(avp_mp_input_weapon_cycle_edge(next|prev,&prior[0])==1);
        CHECK(avp_mp_input_weapon_cycle_edge(next,NULL)==0);
        for(unsigned p=1;p<15u;p++)CHECK(prior[p]==next);
    }
    CHECK(copy.player[0].key[AVP_MP_FIRE]==0); /* Player 1 never serialized. */
    s[1].dx=INT32_MAX; CHECK(avp_mp_input_mouse_motion(&c,s,c.player[1].mouse_path,8,0));
    CHECK(s[1].dx==INT32_MAX);
    printf("PASS: 4 isolated input profiles, independent mice, serial roundtrip, validation, routing\n");
    return 0;
}
