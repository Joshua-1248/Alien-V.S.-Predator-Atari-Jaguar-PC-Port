#include "local_mp_native_input.h"
#include "pc/pc_mouse_look.h"
#include "joypad.h"
#include "player.h"
#include <math.h>
#include <stdint.h>

static int pressed(uint32_t a, unsigned bit)
{ return (a & (1u<<bit))!=0u; }

/* Jaguar has no separate turn-left vs strafe-left bits. Respect original
 * Alien OPTION/other-species FIRE_C modifier, and use the existing original
 * host sidestep bridge for simultaneous independent turn+strafe. */
int avp_mp_native_input_prepare(AvpMpActorNativeState *actor,
                                const AvpMpPlayerCommand *c,
                                const AvpMpInputProfile *profile)
{
    uint32_t pad=UINT32_MAX,old,edge;
    unsigned a,turn,strafe;
    int alien,predator;
    float mouse;
    if (!actor || !c || !profile || !avp_mp_player_native_valid(&actor->player) ||
        !avp_mp_weapon_native_valid(&actor->weapon) ||
        actor->player.cur_weps!=actor->weapon.cur_weps ||
        actor->player.cur_wepno!=actor->weapon.cur_wepno ||
        actor->player.old_weps!=actor->weapon.old_weps ||
        actor->player.new_wepno!=actor->weapon.new_wepno ||
        (c->actions>>AVP_MP_INPUT_ACTIONS) ||
        (c->digital_actions>>AVP_MP_INPUT_ACTIONS) ||
        !isfinite(c->analog_forward) || !isfinite(c->analog_strafe) ||
        !isfinite(c->analog_turn) ||
        ((c->analog_enabled || (c->analog_axis_mask&4u)) &&
            (!isfinite(c->analog_turn_sensitivity) || c->analog_turn_sensitivity<=0.0f ||
             !isfinite(c->analog_turn_speed) || c->analog_turn_speed<=0.0f)) ||
        !isfinite(profile->mouse_sensitivity) ||
        !isfinite(profile->mouse_acceleration) ||
        !isfinite(profile->gamepad_turn_sensitivity) ||
        !isfinite(profile->gamepad_turn_speed) ||
        profile->gamepad_turn_sensitivity<0.25f || profile->gamepad_turn_sensitivity>5.0f ||
        profile->gamepad_turn_speed<0.25f || profile->gamepad_turn_speed>5.0f ||
        profile->mouse_sensitivity<0.1f || profile->mouse_sensitivity>5.0f ||
        profile->mouse_acceleration<0.0f || profile->mouse_acceleration>3.0f)
        return 0;
    a=(c->analog_enabled && c->analog_axis_mask)?c->digital_actions:c->actions;
    alien=actor->player.player_type==PT_ALIEN;
    predator=actor->player.player_type==PT_PREDATOR;
    turn=pressed(a,AVP_MP_TURN_LEFT)||pressed(a,AVP_MP_TURN_RIGHT);
    strafe=pressed(a,AVP_MP_STRAFE_LEFT)||pressed(a,AVP_MP_STRAFE_RIGHT);
    if(pressed(a,AVP_MP_FORWARD))pad&=~(1u<<JOY_UP);
    if(pressed(a,AVP_MP_BACKWARD))pad&=~(1u<<JOY_DOWN);
    if(turn){
        pad&=~(1u<<(alien?OPTION:FIRE_C));
        if(pressed(a,AVP_MP_TURN_LEFT))pad&=~(1u<<JOY_LEFT);
        if(pressed(a,AVP_MP_TURN_RIGHT))pad&=~(1u<<JOY_RIGHT);
    }else if(strafe){
        if(pressed(a,AVP_MP_STRAFE_LEFT))pad&=~(1u<<JOY_LEFT);
        if(pressed(a,AVP_MP_STRAFE_RIGHT))pad&=~(1u<<JOY_RIGHT);
    }
    if(pressed(a,AVP_MP_FIRE)||(alien&&pressed(a,AVP_MP_ALIEN_CLAW)))pad&=~(1u<<FIRE_A);
    if(pressed(a,AVP_MP_USE)||(alien&&pressed(a,AVP_MP_ALIEN_BITE)))pad&=~(1u<<FIRE_B);
    if(alien&&pressed(a,AVP_MP_ALIEN_TAIL))pad&=~(1u<<FIRE_C);
    if(actor->weapon.hugkill && c->analog_enabled && (c->analog_axis_mask&4u)){
        if(c->analog_turn < -0.65f)pad&=~(1u<<JOY_LEFT);
        if(c->analog_turn >  0.65f)pad&=~(1u<<JOY_RIGHT);
    }
    if(pressed(a,AVP_MP_MAP))pad&=~(1u<<KEY_8);
    if(predator&&pressed(a,AVP_MP_PRED_MEDKIT))pad&=~(1u<<KEY_7);
    if((predator&&pressed(a,AVP_MP_PRED_CLOAK))||pressed(a,AVP_MP_OPTION))pad&=~(1u<<OPTION);
    if(predator&&pressed(a,AVP_MP_PRED_VISION))pad&=~(1u<<KEY_6);
    if(pressed(a,AVP_MP_PAUSE))pad&=~(1u<<PAUSE);
    /* Original PlayAvP handles only direct numeric weapon selection. Cycling
     * and Quick Turn are host-only actions, and must be routed in their own
     * stage, not smuggled in as Jaguar pad or unrequested instantaneous turn. */
    old=actor->player.joy_cur;
    edge=(~(old^pad))|pad; /* EXACT joypad.c edge_from() active-low formula */
    actor->player.joy_cur=pad;
    actor->player.joy_edge=edge;
    actor->player.host_keyboard_strafe_during_turn=turn?
        (int)pressed(a,AVP_MP_STRAFE_RIGHT)-(int)pressed(a,AVP_MP_STRAFE_LEFT):0;
    actor->player.host_analog_motion=!!(c->analog_enabled &&
                                         (c->analog_axis_mask&3u));
    actor->player.host_analog_forward=c->analog_enabled ? c->analog_forward : 0.0f;
    actor->player.host_analog_strafe=c->analog_enabled ? c->analog_strafe : 0.0f;
    mouse=profile->mouse_enabled ?
        avp_pc_mouse_turn((float)c->mouse_dx,profile->mouse_sensitivity,
                          profile->mouse_acceleration) : 0.0f;
    actor->player.host_analog_aim=
        (profile->mouse_enabled && profile->mouse_path[0]) ||
        (c->analog_enabled && (c->analog_axis_mask&4u));
    actor->player.host_mouse_aim=profile->mouse_enabled && mouse!=0.0f;
    actor->player.host_enhanced_analog_aim=c->analog_enabled &&
        c->enhanced_analog_enabled && !actor->player.host_mouse_aim;
    /* Enhanced heading is already scoped per actor by PLAYER.S capture. */
    actor->player.host_live_heading_enabled=0;
    actor->player.host_aim_turn=mouse!=0.0f ? mouse :
        ((c->analog_enabled && (c->analog_axis_mask&4u)) ? c->analog_turn : 0.0f);
    actor->player.host_aim_sensitivity=mouse!=0.0f ? 1.0f :
        (c->analog_enabled && (c->analog_axis_mask&4u)) ?
          c->analog_turn_sensitivity : profile->mouse_sensitivity;
    actor->player.host_aim_speed=mouse!=0.0f ? 1.0f :
        (c->analog_enabled && (c->analog_axis_mask&4u)) ?
          c->analog_turn_speed : 1.0f;
    /* Secondary-player buttons/D-pad (not just stick axis) honor their
     * independent turn sensitivity and maximum speed. Both default to 1x. */
    actor->player.host_digital_turn_sensitivity=profile->gamepad_turn_sensitivity;
    actor->player.host_digital_turn_speed=profile->gamepad_turn_speed;
    /* mouse_dy requires a separate per-viewport pitch/camera bridge; no
     * vertical aiming is invented in the original Jaguar player machine. */
    return 1;
}
