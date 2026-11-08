#ifndef AVP_PC_INPUT_ROUTING_H
#define AVP_PC_INPUT_ROUTING_H
#include "joypad.h"
/* linux/joystick.h pulls in input-event-codes.h, whose KEY_0..KEY_9 macros
 * overwrite the Jaguar's keypad bit positions. Remove ONLY those colliding
 * host macros before evaluating Jaguar pad commands. */
#ifdef KEY_0
#undef KEY_0
#endif
#ifdef KEY_1
#undef KEY_1
#endif
#ifdef KEY_2
#undef KEY_2
#endif
#ifdef KEY_3
#undef KEY_3
#endif
#ifdef KEY_4
#undef KEY_4
#endif
#ifdef KEY_5
#undef KEY_5
#endif
#ifdef KEY_6
#undef KEY_6
#endif
#ifdef KEY_7
#undef KEY_7
#endif
#ifdef KEY_8
#undef KEY_8
#endif
#ifdef KEY_9
#undef KEY_9
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(KEY_1==19 && KEY_4==18 && KEY_8==5,
               "Jaguar keypad constants must not be Linux evdev codes");
#endif
/* Express PC key shortcuts in the game's original active-low keypad bits. */
static inline u32 avp_pc_digit_pad(u32 pad,unsigned digit)
{
    static const unsigned bits[10]={KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,
                                     KEY_5,KEY_6,KEY_7,KEY_8,KEY_9};
    return digit<10u ? pad&~(1u<<bits[digit]) : pad;
}
/* Host action movement uses the ordinary retail active-low Up/Down inputs.
 * Button/axis decoding happens through pc_joybinding; this helper preserves
 * other keypad bits and the game's original digital movement semantics. */
static inline u32 avp_pc_move_pad(u32 pad,int forward,int backward)
{
    if(forward)pad&=~(1u<<JOY_UP);
    if(backward)pad&=~(1u<<JOY_DOWN);
    return pad;
}
/* Strafe bindings act on the ordinary unmodified Jaguar left/right pad
 * bits in both PC-action and Jaguar-pad gameplay modes.  The frontend must
 * not use these host-only movement aliases for navigation. */
static inline u32 avp_pc_strafe_pad(u32 pad,int left,int right)
{
    if(left)pad&=~(1u<<JOY_LEFT);
    if(right)pad&=~(1u<<JOY_RIGHT);
    return pad;
}
/* PC-action keyboard directions are independent controls, but the retail
 * Jaguar pad has only one horizontal direction pair. When turn and strafe are
 * held together, publish ONLY the turn chord through the original Jaguar pad
 * and deliver sidestep separately to the host player movement bridge.
 * Unmodified single actions keep their exact historical pad encoding. */
static inline int avp_pc_keyboard_chord_strafe(int turn_left,int turn_right,
                                               int strafe_left,int strafe_right)
{
    return (turn_left!=turn_right) ? (strafe_right!=0)-(strafe_left!=0) : 0;
}
static inline u32 avp_pc_keyboard_action_horizontal_pad(u32 pad,
        int turn_left,int turn_right,int strafe_left,int strafe_right,
        unsigned turn_modifier)
{
    if(turn_left!=turn_right){
        pad&=~(1u<<(turn_left?JOY_LEFT:JOY_RIGHT));
        pad&=~(1u<<turn_modifier);
    }else{
        /* Simultaneous A+D neutralizes yaw; Q/E remain ordinary strafe. */
        pad=avp_pc_strafe_pad(pad,strafe_left,strafe_right);
    }
    return pad;
}
/* Digital Jaguar D-pad emulation must ignore partial axis travel.  The
 * physical Jaguar D-pad is a switch, not an analog value.  In R110 the
 * PC-action path used axis > 0, so a small controller deflection could
 * inject a conflicting direction even during full-speed keyboard movement. */
static inline int avp_pc_axis_digital_positive(float axis)
{
    return axis > 0.65f;
}
static inline int avp_pc_axis_digital_negative(float axis)
{
    return axis < -0.65f;
}
/* PC-only proportional movement: many physical sticks stop a little short of
 * +/-32767. Treat near-end travel as fully held, matching the full-strength
 * Jaguar acceleration without changing the game's movement formulas. */
static inline float avp_pc_axis_motion_full_travel(float axis)
{
    if(axis >= 0.90f)return 1.0f;
    if(axis <= -0.90f)return -1.0f;
    return axis;
}
/* Original DOORS.S::do_lifts reads FIRE_B (Use) with JOY_UP/JOY_DOWN.
 * Proportional PC movement deliberately does not set those digital pad bits.
 * Only at an actual lift, and only while Use is held, bridge the forward axis
 * back to the source Jaguar direction. No extra movement, new lift logic,
 * or changes to the original digital/keyboard controls are introduced. */
static inline u32 avp_pc_lift_analog_pad(u32 pad,int analog_enabled,
                                         int facing_lift,float forward)
{
    if(!analog_enabled||!facing_lift||(pad&(1u<<FIRE_B)))return pad;
    /* axis_value() already applies the user-configured deadzone, so any
     * remaining nonzero direction is an intentional lift request. */
    if(forward>0.0f)pad&=~(1u<<JOY_UP);
    else if(forward<0.0f)pad&=~(1u<<JOY_DOWN);
    return pad;
}
/* Older PC preferences saved before the third Alien attack had a default
 * binding contain action_4=Unbound.  Keep the original Jaguar C action
 * reachable without changing the user's saved preferences or other species.
 * A user-specified Alien Tail binding always takes precedence. */
static inline u32 avp_pc_alien_tail_fallback(u32 pad,int alien,
                                              int key_unbound,int button_unbound,
                                              int c_key_down,int jag_c_button_down)
{
    if(alien && ((key_unbound && c_key_down) ||
                 (button_unbound && jag_c_button_down)))
        pad&=~(1u<<FIRE_C);
    return pad;
}
/* PC-action Pause and Option are independent from virtual Jaguar pad
 * joystick assignments. Keyboard and joystick presses share active-low bits. */
static inline u32 avp_pc_pause_option_pad(u32 pad,int pause,int option)
{
    if(pause)pad&=~(1u<<PAUSE);
    if(option)pad&=~(1u<<OPTION);
    return pad;
}
static inline u32 avp_pc_map_pad(u32 pad,int active)
{
    return active?pad&~(1u<<KEY_8):pad;
}
#endif
