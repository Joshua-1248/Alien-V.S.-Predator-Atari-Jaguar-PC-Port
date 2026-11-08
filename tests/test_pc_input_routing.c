#include "joypad.h"
#include <linux/joystick.h>
#include "pc/pc_input_routing.h"
#include "pc/pc_joybinding.h"
#include <assert.h>
int main(void)
{
    const u32 blank=0xffffffffu;
    assert(KEY_1==19 && KEY_4==18 && KEY_8==5);
    u32 p=avp_pc_digit_pad(blank,4);
    assert((p&(1u<<KEY_4))==0u);
    assert((p&(1u<<KEY_8))!=0u);
    assert((p&(1u<<KEY_1))!=0u);
    p=avp_pc_digit_pad(blank,1);
    assert((p&(1u<<KEY_1))==0u && (p&(1u<<KEY_8))!=0u);
    p=avp_pc_digit_pad(blank,8);
    assert((p&(1u<<KEY_8))==0u && (p&(1u<<KEY_4))!=0u);
    /* PC action joystick Pause/Option preserve unrelated Jaguar bits. */
    p=avp_pc_pause_option_pad(blank,1,0);
    assert((p&(1u<<PAUSE))==0u&&(p&(1u<<OPTION))!=0u);
    p=avp_pc_pause_option_pad(blank,0,1);
    assert((p&(1u<<PAUSE))!=0u&&(p&(1u<<OPTION))==0u);
    assert(avp_pc_pause_option_pad(blank,0,0)==blank);
    p=avp_pc_pause_option_pad(blank,1,1);
    assert((p&(1u<<PAUSE))==0u&&(p&(1u<<OPTION))==0u);
    p=avp_pc_map_pad(blank,0);
    assert(p==blank);
    p=avp_pc_map_pad(blank,1);
    assert((p&(1u<<KEY_8))==0u && (p&(1u<<KEY_4))!=0u);
    p=avp_pc_map_pad(avp_pc_digit_pad(blank,4),0);
    assert((p&(1u<<KEY_8))!=0u);
    /* An assigned physical joystick button and an axis-as-button both send
       the very same Jaguar map bit, independently of the Jaguar C binding. */
    {int16_t axes[2]={0,27000}; unsigned char buttons[3]={0,0,1};
     p=avp_pc_map_pad(blank,avp_pc_binding_down(2,axes,2,buttons,3));
     assert((p&(1u<<KEY_8))==0u && (p&(1u<<FIRE_C))!=0u);
     p=avp_pc_map_pad(blank,avp_pc_binding_down(avp_pc_axis_button_token(1,1),axes,2,buttons,3));
     assert((p&(1u<<KEY_8))==0u);
    }
    /* Each movement direction may be assigned to an ordinary joystick button
       or an axis sign, independent of the continuous move-axis setting. */
    {int16_t axes[3]={0,-29000,0}; unsigned char buttons[4]={0,1,0,0};
     p=avp_pc_move_pad(blank,avp_pc_binding_down(1,axes,3,buttons,4),
                            avp_pc_binding_down(-1,axes,3,buttons,4));
     assert((p&(1u<<JOY_UP))==0u && (p&(1u<<JOY_DOWN))!=0u);
     assert((p&(1u<<JOY_LEFT))!=0u && (p&(1u<<FIRE_A))!=0u);
     p=avp_pc_move_pad(blank,
          avp_pc_binding_down(avp_pc_axis_button_token(1,0),axes,3,buttons,4),
          avp_pc_binding_down(avp_pc_axis_button_token(1,1),axes,3,buttons,4));
     assert((p&(1u<<JOY_UP))==0u && (p&(1u<<JOY_DOWN))!=0u);
     axes[1]=28000;
     p=avp_pc_move_pad(blank,
          avp_pc_binding_down(avp_pc_axis_button_token(1,0),axes,3,buttons,4),
          avp_pc_binding_down(avp_pc_axis_button_token(1,1),axes,3,buttons,4));
     assert((p&(1u<<JOY_UP))!=0u && (p&(1u<<JOY_DOWN))==0u);
     p=avp_pc_move_pad(blank,avp_pc_binding_down(-1,axes,3,buttons,4),
                            avp_pc_binding_down(-1,axes,3,buttons,4));
     assert(p==blank);
     p=avp_pc_move_pad(blank,1,1);
     assert((p&(1u<<JOY_UP))==0u && (p&(1u<<JOY_DOWN))==0u);
    }
    /* Dedicated strafe buttons/axis directions must contribute ordinary
     * sidestep input regardless of the chosen controller mode. The caller
     * excludes frontend navigation, while this helper preserves all other
     * Jaguar pad bits, including turn mode, firing and keypad controls. */
    {
        int16_t a[2]={-28000,28000};
        unsigned char b[2]={1,0};
        u32 controlled=avp_pc_digit_pad(blank,7);
        p=avp_pc_strafe_pad(controlled,
            avp_pc_binding_down(0,a,2,b,2),
            avp_pc_binding_down(-1,a,2,b,2));
        assert((p&(1u<<JOY_LEFT))==0u);
        assert((p&(1u<<JOY_RIGHT))!=0u);
        assert((p&(1u<<KEY_7))==0u && (p&(1u<<FIRE_C))!=0u);
        p=avp_pc_strafe_pad(blank,
            avp_pc_binding_down(avp_pc_axis_button_token(0,0),a,2,b,2),
            avp_pc_binding_down(avp_pc_axis_button_token(1,1),a,2,b,2));
        assert((p&(1u<<JOY_LEFT))==0u && (p&(1u<<JOY_RIGHT))==0u);
        assert(avp_pc_strafe_pad(blank,0,0)==blank);
    }
    /* Alien's retail third attack is Jaguar C, not a weapon-cycle action.
     * Older saved PC settings may leave its new binding unassigned. */
    p=avp_pc_alien_tail_fallback(blank,1,1,1,1,0);
    assert((p&(1u<<FIRE_C))==0u && (p&(1u<<FIRE_A))!=0u);
    p=avp_pc_alien_tail_fallback(blank,1,1,1,0,1);
    assert((p&(1u<<FIRE_C))==0u);
    assert(avp_pc_alien_tail_fallback(blank,0,1,1,1,1)==blank);
    assert(avp_pc_alien_tail_fallback(blank,1,0,0,1,1)==blank);
    /* R110 incorrectly mapped any nonzero gamepad axis to a Jaguar button.
       Short analog travel must not cancel a keyboard's full-speed forward. */
    assert(!avp_pc_axis_digital_positive(0.21f));
    assert(!avp_pc_axis_digital_negative(-0.50f));
    assert(!avp_pc_axis_digital_positive(0.65f));
    assert(avp_pc_axis_digital_positive(0.80f));
    assert(avp_pc_axis_digital_negative(-0.80f));
    /* An analog stick can fall short of its hardware endpoint while held. */
    assert(avp_pc_axis_motion_full_travel(0.95f)==1.0f);
    assert(avp_pc_axis_motion_full_travel(-0.95f)==-1.0f);
    assert(avp_pc_axis_motion_full_travel(0.50f)==0.50f);
    assert(avp_pc_axis_motion_full_travel(-0.50f)==-0.50f);
    assert(avp_pc_axis_motion_full_travel(0.0f)==0.0f);
    /* R208: analog forward/backward must operate original lift Up/Down only
     * with Use held at a lift panel, in either controller mode.  Ordinary
     * world movement and joystick-free keyboard Use remain unaffected. */
    {
        u32 use=blank&~(1u<<FIRE_B),selected=use&~(1u<<KEY_4);
        assert(avp_pc_lift_analog_pad(blank,1,1,0.95f)==blank);
        assert(avp_pc_lift_analog_pad(use,0,1,0.95f)==use);
        assert(avp_pc_lift_analog_pad(use,1,0,0.95f)==use);
        assert(avp_pc_lift_analog_pad(use,1,1,0.0f)==use);
        assert((avp_pc_lift_analog_pad(use,1,1,0.5f)&(1u<<JOY_UP))==0u);
        assert((avp_pc_lift_analog_pad(use,1,1,-0.5f)&(1u<<JOY_DOWN))==0u);
        p=avp_pc_lift_analog_pad(selected,1,1,0.90f);
        assert((p&(1u<<JOY_UP))==0u && (p&(1u<<JOY_DOWN))!=0u);
        assert((p&(1u<<FIRE_B))==0u && (p&(1u<<KEY_4))==0u);
        p=avp_pc_lift_analog_pad(selected,1,1,-0.90f);
        assert((p&(1u<<JOY_DOWN))==0u && (p&(1u<<JOY_UP))!=0u);
        assert((p&(1u<<FIRE_B))==0u && (p&(1u<<KEY_4))==0u);
        p=avp_pc_lift_analog_pad(selected&~(1u<<JOY_DOWN),1,1,0.91f);
        assert((p&(1u<<JOY_DOWN))==0u && (p&(1u<<JOY_UP))==0u);
    }
    return 0;
}
