#ifndef AVP_NEW_LOCAL_MP_INPUT_H
#define AVP_NEW_LOCAL_MP_INPUT_H

#include <stdio.h>
#include <stdint.h>

/* PC-only, isolated profile data. Player 1 continues to use the original
 * PC settings and input implementation; profiles 1..3 are Players 2..4. */
#define AVP_MP_INPUT_PLAYERS 4u
#define AVP_MP_INPUT_ACTIONS 25u
#define AVP_MP_DEVICE_PATH 192u
#define AVP_MP_MOUSE_BUTTONS 16u

/* Stable action order; recorded as versioned explicit config keys. */
enum AvpMpAction {
    AVP_MP_FORWARD, AVP_MP_BACKWARD, AVP_MP_TURN_LEFT, AVP_MP_TURN_RIGHT,
    AVP_MP_STRAFE_LEFT, AVP_MP_STRAFE_RIGHT, AVP_MP_FIRE, AVP_MP_USE,
    AVP_MP_MAP, AVP_MP_PREV_WEAPON, AVP_MP_NEXT_WEAPON,
    AVP_MP_ALIEN_CLAW, AVP_MP_ALIEN_BITE, AVP_MP_ALIEN_TAIL,
    AVP_MP_PRED_MEDKIT, AVP_MP_PRED_CLOAK, AVP_MP_PRED_VISION,
    AVP_MP_QUICK_TURN, AVP_MP_PAUSE, AVP_MP_OPTION,
    AVP_MP_TERMINAL_NEXT, AVP_MP_TERMINAL_PREVIOUS,
    AVP_MP_MAP_ZOOM_IN, AVP_MP_MAP_ZOOM_OUT,
    AVP_MP_TERMINAL_EXIT /* append: preserve every existing settings/action ID */
};

typedef struct AvpMpInputProfile {
    unsigned long key[AVP_MP_INPUT_ACTIONS]; /* X11 KeySym values, 0=unbound */
    int mouse_button[AVP_MP_INPUT_ACTIONS]; /* 1..15, -1=unbound */
    int gamepad_button[AVP_MP_INPUT_ACTIONS]; /* 0..31, -1=unbound */
    char mouse_path[AVP_MP_DEVICE_PATH]; /* distinct physical device path */
    char gamepad_path[AVP_MP_DEVICE_PATH];
    int gamepad_axis[3]; /* forward/back, strafe, turn: -1 means unbound */
    int gamepad_axis_invert[3];
    float gamepad_deadzone;
    /* Per-player original PC analog tuning. P1's legacy settings remain
     * authoritative; these are persisted for secondary local players. */
    int gamepad_analog_movement, gamepad_enhanced_analog_controls;
    float gamepad_turn_sensitivity, gamepad_turn_speed;
    float mouse_sensitivity;
    float mouse_acceleration;
    int mouse_enabled;
    int gamepad_enabled;
    /* Co-op only; per-actor toggles. Global/world cheats remain shared. */
    int cheat_infinite_health,cheat_infinite_ammo,cheat_tracker,cheat_noclip;
    int cheat_no_facehug,cheat_no_enemy_attack; /* per-target AMP restrictions */
    int audio_motion_tracker,audio_player_pain,audio_facehug_pain,audio_facehug_shake;
    int remember_automap; /* PC enhancement, isolated for each participant */
    int cheat_reveal_map; /* Per-player full geometry reveal, all visited levels */
} AvpMpInputProfile;

typedef struct AvpMpInputConfig { AvpMpInputProfile player[AVP_MP_INPUT_PLAYERS]; } AvpMpInputConfig;

/* Map an owning MP actions to the active-low Jaguar computer pad.
 * Pure conversion: no gameplay clock, no input polling or menu ownership. */
/* Convert a bot/human action bitfield into a rising-edge weapon cycle.
 * Keep one previous-button state per ACTOR, not per rendered human viewport.
 * The caller applies the resulting direction to the native Jaguar weapons. */
int avp_mp_input_weapon_cycle_edge(uint32_t actions,uint32_t *prior_buttons);
uint32_t avp_mp_terminal_actions_pad(uint32_t actions);
/* Consume the gameplay Use/B hold that entered the modal until B has been
 * observed released. Subsequent deliberate B presses work normally. */
uint32_t avp_mp_terminal_initial_use_guard(uint32_t pad,int *waiting_for_release);

void avp_mp_input_defaults(AvpMpInputConfig *cfg);
void avp_mp_input_validate(AvpMpInputConfig *cfg);
const char *avp_mp_input_action_name(unsigned action);
/* Bounded parser: unknown keys return 0, malformed keys do not change state. */
int avp_mp_input_parse_setting(AvpMpInputConfig *cfg, const char *key, const char *value);
void avp_mp_input_write_settings(const AvpMpInputConfig *cfg, FILE *out);
/* Refuse a physical mouse assigned to a different player. */
int avp_mp_input_assign_mouse(AvpMpInputConfig *cfg, unsigned player, const char *path);
/* Return index of the unique profile using a physical mouse, or -1. */
int avp_mp_input_mouse_owner(const AvpMpInputConfig *cfg, const char *path);
/* Cycle P2-P4 controllers including Unassigned, skipping devices owned by
 * another player or the legacy P1 joystick. Uses physical identity for
 * Linux aliases. Testable without actual joystick hardware. */
int avp_mp_input_cycle_gamepad(AvpMpInputConfig *cfg, unsigned player,
                               const char *const *devices, unsigned count,
                               const char *p1_device, int direction);

typedef struct AvpMpMouseState {
    int32_t dx, dy;
    uint16_t buttons;
} AvpMpMouseState;
/* Hardware-independent event router used by OS backends; unassigned device
 * events are ignored. Mouse events must not be forwarded to Player 1's
 * legacy X11 core pointer handler. */
int avp_mp_input_mouse_motion(const AvpMpInputConfig *cfg, AvpMpMouseState states[AVP_MP_INPUT_PLAYERS],
                              const char *device, int dx, int dy);
int avp_mp_input_mouse_button(const AvpMpInputConfig *cfg, AvpMpMouseState states[AVP_MP_INPUT_PLAYERS],
                              const char *device, unsigned button, int down);

#endif
