/* Exact input-frame ownership and 10-Hz consumption tests; no OS devices. */
#include "local_mp_commands.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
int main(void)
{
    AvpMpInputConfig cfg;
    AvpMpCommandInput in={0};
    AvpMpPlayerCommand cmd[4];
    avp_mp_input_defaults(&cfg);
    cfg.player[1].key[AVP_MP_FORWARD]='j';
    cfg.player[2].key[AVP_MP_FORWARD]='k';
    cfg.player[3].key[AVP_MP_FIRE]='l';
    in.keysyms[4]='j';in.keysyms[9]='l';
    CHECK(avp_mp_input_assign_mouse(&cfg,1,"mouse-2"));
    CHECK(avp_mp_input_assign_mouse(&cfg,2,"mouse-3"));
    CHECK(avp_mp_input_assign_mouse(&cfg,3,"mouse-4"));
    cfg.player[1].mouse_button[AVP_MP_FIRE]=1;
    cfg.player[2].mouse_button[AVP_MP_USE]=3;
    cfg.player[3].mouse_button[AVP_MP_MAP]=2;
    in.mice[1].dx=INT32_MAX;in.mice[1].dy=7;in.mice[1].buttons=1;
    in.mice[2].dx=-33;in.mice[2].buttons=1u<<2;
    in.mice[3].dx=41;in.mice[3].buttons=1u<<1;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(cmd[0].actions==0 && cmd[0].mouse_dx==0);
    CHECK(cmd[1].actions==((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_FIRE)));
    CHECK(cmd[1].mouse_dx==INT32_MAX && cmd[1].mouse_dy==7);
    CHECK(cmd[2].actions==(1u<<AVP_MP_USE) && cmd[2].mouse_dx==-33);
    CHECK(cmd[3].actions==((1u<<AVP_MP_FIRE)|(1u<<AVP_MP_MAP)) && cmd[3].mouse_dx==41);
    CHECK(in.mice[1].buttons==1u && in.mice[1].dx==0 && in.mice[2].dx==0);
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(cmd[1].mouse_dx==0 && cmd[1].actions==((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_FIRE)));
    /* The controller input is matched by physical path (no global pooling),
     * with user-configured deadzone and inversion. */
    strcpy(cfg.player[2].gamepad_path,"pad-3");
    cfg.player[2].gamepad_button[AVP_MP_FIRE]=5;
    cfg.player[2].gamepad_axis[0]=0;
    cfg.player[2].gamepad_axis[1]=1;
    cfg.player[2].gamepad_axis[2]=2;
    cfg.player[2].gamepad_axis_invert[2]=1;
    cfg.player[2].gamepad_axis_invert[0]=1;
    strcpy(in.gamepads[1].path,"pad-3");
    in.gamepads[1].buttons=1u<<5;in.gamepads[1].axis_count=3;
    in.gamepads[1].axes[0]=-32768;
    in.gamepads[1].axes[1]=26000;
    in.gamepads[1].axes[2]=32767;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK((cmd[2].actions & ((1u<<AVP_MP_FIRE)|(1u<<AVP_MP_FORWARD)|
           (1u<<AVP_MP_STRAFE_RIGHT)|(1u<<AVP_MP_TURN_LEFT)))==
          ((1u<<AVP_MP_FIRE)|(1u<<AVP_MP_FORWARD)|
           (1u<<AVP_MP_STRAFE_RIGHT)|(1u<<AVP_MP_TURN_LEFT)));
    in.gamepads[1].axes[0]=3000;in.gamepads[1].axes[1]=1000;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(!(cmd[2].actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
           (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))));
    /* Exact secondary-controller analog sampling: no digital-only precision
     * loss, no shared P1 axes, and each profile has its own deadzone. */
    in.gamepads[1].axes[0]=-16384;
    in.gamepads[1].axes[1]=1000;
    in.gamepads[1].axes[2]=-8192;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(cmd[2].analog_axis_mask==7u);
    CHECK(fabsf(cmd[2].analog_forward-0.500015f)<0.0001f);
    CHECK(cmd[2].analog_strafe==0.0f);
    CHECK(fabsf(cmd[2].analog_turn-0.250007f)<0.0001f);
    CHECK(!(cmd[2].digital_actions&(1u<<AVP_MP_FORWARD)));
    CHECK((cmd[2].actions&(1u<<AVP_MP_FORWARD))==0);
    /* Proportional movement and turn tuning MUST reach the native step;
     * this used to fail because analog_enabled was never populated. */
    cfg.player[2].gamepad_analog_movement=1;
    cfg.player[2].gamepad_enhanced_analog_controls=1;
    cfg.player[2].gamepad_turn_sensitivity=3.0f;
    cfg.player[2].gamepad_turn_speed=2.5f;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(cmd[2].analog_enabled && cmd[2].enhanced_analog_enabled);
    CHECK(!(cmd[2].actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
           (1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT))));
    /* Full stick travel is P1-equivalent; never cap at 0.94..0.99. */
    in.gamepads[1].axes[0]=-30000;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(cmd[2].analog_forward==1.0f);
    in.gamepads[1].axes[0]=-16384;
    CHECK(cmd[2].analog_turn_sensitivity==3.0f && cmd[2].analog_turn_speed==2.5f);
    CHECK(!cmd[1].analog_enabled && !cmd[3].analog_enabled);
    cfg.player[2].gamepad_analog_movement=0;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(!cmd[2].analog_enabled && cmd[2].analog_turn_sensitivity==3.0f);
    CHECK(cmd[2].analog_turn>0.0f &&
          !(cmd[2].actions&(1u<<AVP_MP_TURN_RIGHT)));
    /* Digital fallback must wait for the same 0.65 movement threshold as P1. */
    in.gamepads[1].axes[0]=-15000;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(!(cmd[2].actions&(1u<<AVP_MP_FORWARD)));
    in.gamepads[1].axes[0]=-30000;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(cmd[2].actions&(1u<<AVP_MP_FORWARD));
    in.gamepads[1].axes[0]=-16384;
    cfg.player[2].gamepad_analog_movement=1;
    cfg.player[2].gamepad_deadzone=0.60f;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(cmd[2].analog_forward==0.0f && cmd[2].analog_turn==0.0f);
    cfg.player[2].gamepad_deadzone=0.20f;
    /* Match the original host's negative axis token for a positive axis
     * and verify that it is independent of the motion axis deadzone. */
    cfg.player[2].gamepad_button[AVP_MP_QUICK_TURN]=-3; /* axis 0 positive */
    in.gamepads[1].axes[0]=24000;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(cmd[2].actions&(1u<<AVP_MP_QUICK_TURN));
    in.gamepads[1].axes[0]=10000;
    CHECK(avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(!(cmd[2].actions&(1u<<AVP_MP_QUICK_TURN)));
    cfg.player[2].gamepad_button[AVP_MP_QUICK_TURN]=-1;
    /* Conflicting gamepad path is rejected, without consuming pending mouse
     * motion or publishing partially composed command frames. */
    strcpy(cfg.player[3].gamepad_path,"pad-3");
    in.mice[1].dx=44;memset(cmd,0xa5,sizeof(cmd));
    CHECK(!avp_mp_commands_take(&cfg,&in,4,cmd));
    CHECK(in.mice[1].dx==44 && cmd[1].mouse_dx==(int32_t)0xa5a5a5a5u);
    cfg.player[3].gamepad_path[0]=0;
    CHECK(!avp_mp_commands_take(&cfg,&in,0,cmd));
    CHECK(!avp_mp_commands_take(&cfg,&in,5,cmd));
    CHECK(avp_mp_commands_take(&cfg,&in,2,cmd));
    CHECK(cmd[1].mouse_dx==44 && cmd[2].actions==0 && cmd[3].actions==0);
    avp_mp_commands_clear(&in);
    CHECK(in.keysyms[4]==0 && in.mice[1].buttons==0 && in.gamepads[1].axis_count==0);
    puts("PASS: four isolated commands; held/edge, mouse consumption, identifiable controllers, deadzones, inversion, atomic failure");
    return 0;
}
